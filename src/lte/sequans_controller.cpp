#include "sequans_controller.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stddef.h>
#include <string.h>
#include <util/delay.h>

#include <Arduino.h>
#include <pins_arduino.h>

#define TX_PIN      PIN_PC0
#define CTS_PIN     PIN_PC6
#define CTS_PIN_bm  PIN6_bm
#define CTS_INT_bm  PORT_INT6_bm
#define RING_INT_bm PORT_INT4_bm
#define RTS_PORT    PORTC
#define RTS_PIN     PIN_PC7
#define RTS_PIN_bm  PIN7_bm
#define RESET_PIN   PIN_PE1
#define RING_PIN    PIN_PC4

#define BAUD_RATE 115200

#define HWSERIALAT               USART1
#define SEQUANS_MODULE_BAUD_RATE 115200

// Sizes for the circular buffers
#define RX_BUFFER_SIZE        128
#define TX_BUFFER_SIZE        64
#define RX_BUFFER_ALMOST_FULL RX_BUFFER_SIZE - 2

#define URC_BUFFER_SIZE 32

// Specifies the valid bits for the index in the buffers
#define RX_BUFFER_MASK (RX_BUFFER_SIZE - 1)
#define TX_BUFFER_MASK (TX_BUFFER_SIZE - 1)

// CTS, control line for the MCU sending the LTE module data
#define sequansModuleIsReadyForData() (!(VPORTC.IN & CTS_PIN_bm))

#define LINE_FEED            '\n'
#define CARRIAGE_RETURN      '\r'
#define DATA_START_CHARACTER ':'
#define SPACE_CHARACTER      ' '
#define URC_START_CHARACTER  '+'
#define URC_END_CHARACTER    ':'
#define RESPONSE_DELIMITER   ","

static const char OK_TERMINATION[] = "OK\r\n";
static const char ERROR_TERMINATION[] = "ERROR\r\n";

static uint8_t rx_buffer[RX_BUFFER_SIZE];
static volatile uint8_t rx_head_index = 0;
static volatile uint8_t rx_tail_index = 0;
static volatile uint8_t rx_num_elements = 0;

static uint8_t tx_buffer[TX_BUFFER_SIZE];
static volatile uint8_t tx_head_index = 0;
static volatile uint8_t tx_tail_index = 0;
static volatile uint8_t tx_num_elements = 0;

// TODO: Not finished implementation yet
// static uint8_t urc_buffer[URC_BUFFER_SIZE];
// static uint8_t urc_buffer_length = 0;
// static bool parsing_urc = false;
// static char urc_lookup_table[8][URC_BUFFER_SIZE];
// static uint8_t urc_lookup_table_length[8] = {5, 7, 0, 0, 0, 0, 0, 0};
// void (*urc_callback[8])(void);
// static uint8_t number_of_urc_callbacks;

// Default values
static uint8_t number_of_retries = 5;
static double sleep_between_retries_ms = 20;

/** @brief Flow control update for the UART interface with the LTE modules
 *
 * Updates RTS line based on space available in receive buffer. If the buffer
 * is close to full the RTS line is de-asserted (set high) to signal to the
 * target that no more data should be sent
 */
static void flowControlUpdate(void) {

    // We prefer to not use arduino's digitalWrite here to reduce code in the
    // ISR
    if (rx_num_elements < RX_BUFFER_ALMOST_FULL) {
        // Space for more data, assert RTS line (active low)
        RTS_PORT.OUTCLR |= RTS_PIN_bm;
    } else {
        // Buffer is filling up, tell the target to stop sending data
        // for now by de-asserting RTS
        RTS_PORT.OUTSET |= RTS_PIN_bm;
    }
}

// For CTS interrupt
ISR(PORTC_PORT_vect) {
    if (VPORTC.INTFLAGS & CTS_INT_bm) {

        if (VPORTC.IN & CTS_PIN_bm) {
            // CTS is not asserted (active low) so disable USART Data Register
            // Empty Interrupt where the logic is to send more data
            HWSERIALAT.CTRLA &= ~USART_DREIE_bm;
        } else {
            // CTS is asserted check if there is data to transmit
            // before we enable interrupt
            HWSERIALAT.CTRLA |= USART_DREIE_bm;
        }
    }

    VPORTC.INTFLAGS = 0xff;
}

// RX complete
ISR(USART1_RXC_vect) {
    uint8_t data = USART1.RXDATAL;

    // We do an logical AND here as a means of allowing the index to wrap
    // around since we have a circular buffer
    rx_head_index = (rx_head_index + 1) & RX_BUFFER_MASK;
    rx_buffer[rx_head_index] = data;
    rx_num_elements++;

    /*
    // TODO: To interrupt or not to interrupt, ask Johan
    if (data == URC_START_CHARACTER) {
        parsing_urc = true;
        urc_buffer_length = 0;
    } else if (data == URC_END_CHARACTER) {
        parsing_urc = false;

        for (uint8_t i = 0; i < number_of_urc_callbacks; i++) {
            if (urc_lookup_table_length[i] == urc_buffer_length) {
                if (memcmp(
                        urc_buffer, urc_lookup_table[i], urc_buffer_length)) {
                    urc_callback[i]();
                    break;
                }
            }
        }
    } else if (parsing_urc) {
        urc_buffer[urc_buffer_length++] = data;
    }
    */

    flowControlUpdate();
}

/**
 * @brief Data register empty. Allows us to keep track of when the data has been
 * transmitted on the line and set up new data to be transmitted from the ring
 * buffer.
 */
ISR(USART1_DRE_vect) {
    if (tx_num_elements != 0) {
        // We do an logical AND here as a means of allowing the index to
        // wrap around since we have a circular buffer
        tx_tail_index = (tx_tail_index + 1) & TX_BUFFER_MASK;

        // Fill the transmit buffer since our ring buffer isn't empty
        // yet
        USART1.TXDATAL = tx_buffer[tx_tail_index];
        tx_num_elements--;
    } else {
        // Disable TX interrupt until we want to send more data
        USART1.CTRLA &= ~(1 << USART_DREIE_bp);
    }
}

void sequansControllerBegin(void) {

    // PIN SETUP

    pinConfigure(TX_PIN, PIN_DIR_OUTPUT | PIN_PULLUP_ON);

    // Request to send (RTS) and clear to send (CTS) are the control lines
    // on the UART line. From the configuration the MCU and the LTE modem is
    // in, we control the RTS line from the MCU to signalize if we can process
    // more data or not from the LTE modem. The CTS line is controlled from
    // the LTE modem and gives us the ability to know whether the LTE modem
    // can receive more data or if we have to wait.
    //
    // Both pins are active low.

    // We assert RTS high until we are ready to receive more data
    pinConfigure(RTS_PIN, PIN_DIR_OUTPUT);
    digitalWrite(RTS_PIN, HIGH);

    // Clear to send is input and we want interrupts on both edges to know
    // when the LTE modem has changed the state of the line.
    pinConfigure(CTS_PIN, PIN_DIR_INPUT | PIN_PULLUP_ON | PIN_INT_CHANGE);

    // Set reset low to reset the LTE modem
    pinConfigure(RESET_PIN, PIN_DIR_OUTPUT);
    digitalWrite(RESET_PIN, LOW);

    // SERIAL INTERFACE SETUP

    // LTE modules has set baud rate of 115200 for its UART0 interface
    USART1.BAUD =
        (uint16_t)(((float)F_CPU * 64 / (16 * (float)BAUD_RATE)) + 0.5);

    // Interrupt on receive completed
    USART1.CTRLA = USART_RXCIE_bm;

    USART1.CTRLB = USART_RXEN_bm | USART_TXEN_bm;

    // LTE module interface requires 8 data bits with one stop bit
    USART1.CTRLC = USART_CMODE_ASYNCHRONOUS_gc | USART_SBMODE_1BIT_gc |
                   USART_CHSIZE_8BIT_gc;

    flowControlUpdate();

    PORTG.OUTSET |= PIN2_bm;
}

void sequansControllerEnd(void) {
    USART1.CTRLA = 0;
    USART1.CTRLB = 0;
    USART1.CTRLC = 0;

    pinConfigure(CTS_PIN, 0);
}

void sequansControllerSetRetryConfiguration(const uint8_t num_retries,
                                            const double sleep_ms) {

    number_of_retries = num_retries;
    sleep_between_retries_ms = sleep_ms;
}

bool sequansControllerIsTxReady(void) {
    return (tx_num_elements != TX_BUFFER_SIZE);
}

bool sequansControllerIsRxReady(void) { return (rx_num_elements != 0); }

bool sequansControllerWriteByte(const uint8_t data) {

    uint8_t retry_count = 0;
    while (!sequansControllerIsTxReady()) {
        retry_count++;

        if (retry_count == number_of_retries) {
            return false;
        }

        _delay_ms(sleep_between_retries_ms);
    }

    tx_head_index = (tx_head_index + 1) & TX_BUFFER_MASK;
    tx_buffer[tx_head_index] = data;

    cli();
    tx_num_elements++;
    sei();

    // Enable TX interrupt if CTS (active low) is asserted
    // (i.e. device is ready for data)
    if (sequansModuleIsReadyForData()) {
        USART1.CTRLA |= (1 << USART_DREIE_bp);
    }

    return true;
}

bool sequansControllerWriteCommand(const char *command) {
    const size_t length = strlen(command);

    for (size_t i = 0; i < length; i++) {
        if (!sequansControllerWriteByte(command[i])) {
            return false;
        }
    }

    return sequansControllerWriteByte('\r');
}

bool sequansControllerWriteBytes(const uint8_t *data,
                                 const size_t buffer_size) {

    for (size_t i = 0; i < buffer_size; i++) {
        if (!sequansControllerWriteByte(data[i])) {
            return false;
        }
    }

    return sequansControllerWriteByte('\r');
}

int16_t sequansControllerReadByte() {
    if (!sequansControllerIsRxReady()) {
        return -1;
    }

    // Disable interrupts temporarily here to prevent being interleaved
    // in the middle of updating the tail index
    cli();
    uint8_t next_tail_index = (rx_tail_index + 1) & RX_BUFFER_MASK;
    rx_tail_index = next_tail_index;
    rx_num_elements--;
    sei();

    flowControlUpdate();

    return rx_buffer[next_tail_index];
}

ResponseResult sequansControllerReadResponse(char *out_buffer,
                                             uint16_t buffer_size) {
    uint8_t retry_count = 0;
    int16_t value;

    for (size_t i = 0; i < buffer_size; i++) {
        if (!sequansControllerIsRxReady()) {
            retry_count++;
            _delay_ms(sleep_between_retries_ms);

            i--;

            if (retry_count == number_of_retries) {
                return TIMEOUT;
            }

            continue;
        }

        // Reset if we get a valid value
        retry_count = 0;

        out_buffer[i] = (uint8_t)sequansControllerReadByte();

        // For AT command responses from the LTE module, "OK\r\n" or
        // "ERROR\r\n" signifies the end of a response, so we look "\r\n".
        if (i >= 1 && out_buffer[i - 1] == CARRIAGE_RETURN &&
            out_buffer[i] == LINE_FEED) {

            char *ok_termination_index = strstr(out_buffer, OK_TERMINATION);
            if (ok_termination_index != NULL) {

                // We set the rest of the buffer from the "OK\r\n" to 0
                memset(ok_termination_index,
                       0,
                       buffer_size - (ok_termination_index - out_buffer));

                return OK;
            }

            char *error_termination_index =
                strstr(out_buffer, ERROR_TERMINATION);

            if (error_termination_index != NULL) {
                // We set the rest of the buffer from the "ERROR\r\n" to 0
                memset(error_termination_index,
                       0,
                       buffer_size - (error_termination_index - out_buffer));

                return ERROR;
            }
        }
    }

    // Didn't find the end marker within the number of bytes given for the
    // response. Caller should increase the buffer size.
    return BUFFER_OVERFLOW;
}

ResponseResult sequansControllerFlushResponse(void) {

    // We use the termination buffer to look for a "\r\n", which can indicate
    // termination. We set it to max size of the error termination so that we
    // can fit it as well as the 'OK' termination
    char termination_buffer[sizeof(ERROR_TERMINATION)] = "";

    // Fill with non-null values in order for checking for substrings to work
    memset(termination_buffer, ' ', sizeof(termination_buffer) - 1);

    // Don't include null termination
    size_t termination_buffer_size = sizeof(termination_buffer) - 1;

    uint8_t retry_count = 0;

    // We will break out of the loop if we find the termination sequence
    // or if we pass the retry count.
    while (retry_count < number_of_retries) {

        if (!sequansControllerIsRxReady()) {
            retry_count++;
            _delay_ms(sleep_between_retries_ms);
            continue;
        }

        // Shift the buffer backwards
        for (size_t i = 0; i < termination_buffer_size - 1; i++) {
            termination_buffer[i] = termination_buffer[i + 1];
        }

        termination_buffer[termination_buffer_size - 1] =
            sequansControllerReadByte();
        // Reset retry count when we get some data
        retry_count = 0;

        if (termination_buffer[termination_buffer_size - 2] ==
                CARRIAGE_RETURN &&
            termination_buffer[termination_buffer_size - 1] == LINE_FEED) {

            char *ok_termination_index =
                strstr(termination_buffer, OK_TERMINATION);
            if (ok_termination_index != NULL) {
                return OK;
            }

            char *error_termination_index =
                strstr(termination_buffer, ERROR_TERMINATION);

            if (error_termination_index != NULL) {
                return ERROR;
            }
        }
    }

    return TIMEOUT;
}

bool sequansControllerExtractValueFromCommandResponse(
    char *response,
    const uint8_t index,
    char *buffer,
    const size_t buffer_size) {

    // Using strtok further down in this function would modify the original
    // string, so we create a copy to the end index + 1 (because of
    // NULL termination).
    const size_t response_size = strlen(response) + 1;
    char response_copy[response_size];
    strcpy(response_copy, response);

    // Find the first occurrence of the data start character and move pointer to
    // there
    char *data = strchr(response_copy, DATA_START_CHARACTER);

    if (data == NULL) {
        return false;
    }

    // Increment pointer to skip the data start character (and the following
    // space in the start sequence of the data if it is there)
    while (*data == DATA_START_CHARACTER || *data == SPACE_CHARACTER) {
        data++;
    }

    // Now we split the string by the response delimiter and search for the
    // index we're interested in
    char *value = strtok(data, RESPONSE_DELIMITER);

    uint8_t value_index = 1;
    while (value != NULL && value_index <= index) {
        value = strtok(NULL, RESPONSE_DELIMITER);

        value_index++;
    }

    char *first_carriage_return = strchr(value, '\r');

    // If found, set termination to the carriage return
    if (value != NULL) {
        *first_carriage_return = 0;
    }

    size_t value_length = strlen(value);

    // We compare inclusive for value length as we want to take the null
    // termination into consideration. So the buffer size has be
    // value_length + 1
    if (value == NULL || value_length >= buffer_size) {
        return false;
    }

    memcpy(buffer, value, value_length);

    return true;
}
