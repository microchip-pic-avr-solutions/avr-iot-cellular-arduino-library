#include "sequans_controller.h"

#define __AVR_AVR128DB64__

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stddef.h>
#include <string.h>
#include <util/delay.h>

#define TX_PIN_bm PIN0_bm
#define CTS_PIN_bm PIN6_bm
#define RTS_PIN_bm PIN7_bm

#define RESET_PIN_bm PIN1_bm

// The LTE modem operates at this baud rate
#define BAUD_RATE 115200

// Sizes for the circular buffers
#define RX_BUFFER_SIZE 128
#define TX_BUFFER_SIZE 64
#define RX_BUFFER_ALMOST_FULL RX_BUFFER_SIZE - 2

// Specifies the valid bits for the index in the buffers
#define RX_BUFFER_MASK (RX_BUFFER_SIZE - 1)
#define TX_BUFFER_MASK (TX_BUFFER_SIZE - 1)

// CTS, control line for the MCU sending the LTE module data
#define sequansModuleIsReadyForData() (!(VPORTC.IN & CTS_PIN_bm))

#define LINE_FEED 0xA
#define CARRIAGE_RETURN 0xD
#define RESPONSE_DELIMITER ","
#define DATA_START_CHARACTER ':'

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

/** @brief Flow control update for the UART interface with the LTE modules
 *
 * Updates RTS line based on space available in receive buffer. If the buffer
 * is close to full the RTS line is de-asserted (set high) to signal to the
 * target that no more data should be sent
 */
static void flowControlUpdate(void) {

    if (rx_num_elements < RX_BUFFER_ALMOST_FULL) {
        // Space for more data, assert RTS line (active low)
        PORTC.OUTCLR = RTS_PIN_bm;
    } else {
        // Buffer is filling up, tell the target to stop sending data
        // for now by  de-asserting RTS
        PORTC.OUTSET = RTS_PIN_bm;
    }
}

// CTS interrupt (PC6)
ISR(PORTC_PORT_vect) {
    // Check if PIN6 was interrupted
    if (VPORTC.INTFLAGS & PORT_INT6_bm) {

        if (!sequansModuleIsReadyForData()) {
            // CTS is not asserted so disable USART Data Register
            // Empty Interrupt where the logic is to send more data
            USART1.CTRLA &= ~(1 << USART_DREIE_bp);
        } else {
            // CTS is asserted check if there is data to transmit
            // before we enable interrupt
            if (tx_num_elements) {
                USART1.CTRLA |= (1 << USART_DREIE_bp);
            }
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

void sequansControllerInitialize(void) {

    // PIN SETUP

    PORTC.DIRSET = TX_PIN_bm;
    PORTC.PIN0CTRL |= PORT_PULLUPEN_bm;

    // Request to send (RTS) and clear to send (CTS) are the control lines
    // on the UART line. From the configuration the MCU and the LTE modem is
    // in, we control the RTS line from the MCU to signalize if we can process
    // more data or not from the LTE modem. The CTS line is controlled from
    // the LTE modem and gives us the ability to know whether the LTE modem
    // can receive more data or if we have to wait.
    //
    // Both pins are active low.

    // We assert RTS high until we are ready to receive more data
    PORTC.OUTSET = RTS_PIN_bm;
    PORTC.DIRSET = RTS_PIN_bm;

    // Clear to send is input and we want interrupts on both edges to know
    // when the LTE modem has changed the state of the line.
    PORTC.DIRCLR = CTS_PIN_bm;
    PORTC.PIN6CTRL |= PORT_PULLUPEN_bm | PORT_ISC_BOTHEDGES_gc;

    // Set reset low to reset the LTE modem
    PORTE.OUTCLR = RESET_PIN_bm;
    PORTE.DIRSET = RESET_PIN_bm;

    // USART INTERFACE SETUP

    // LTE modules has set baud rate of 115200 for its UART0 interface
    USART1.BAUD =
        (uint16_t)(((float)F_CPU * 64 / (16 * (float)BAUD_RATE)) + 0.5);

    // Interrupt on receive completed
    USART1.CTRLA = USART_RXCIE_bm;

    USART1.CTRLB = USART_RXEN_bm | USART_TXEN_bm;

    // LTE module interface requires 8 data bits with one stop bit
    USART1.CTRLC = USART_CMODE_ASYNCHRONOUS_gc | USART_SBMODE_1BIT_gc |
                   USART_CHSIZE_8BIT_gc;

    sei();

    flowControlUpdate();
}

bool sequansControllerIsTxReady(void) {
    return (tx_num_elements != TX_BUFFER_SIZE);
}

bool sequansControllerIsRxReady(void) { return (rx_num_elements != 0); }

bool sequansControllerIsTxDone(void) {
    return (USART1.STATUS & USART_TXCIF_bm);
}

void sequansControllerSendByte(const uint8_t data) {
    while (!sequansControllerIsTxReady()) {}

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
}

void sequansControllerSendCommand(const char *command) {
    const size_t length = strlen(command);
    for (size_t i = 0; i < length; i++) {
        sequansControllerSendByte(command[i]);
    }

    sequansControllerSendByte('\r');
}

uint8_t sequansControllerReadByte(void) {
    while (!sequansControllerIsRxReady()) {}

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

uint8_t sequansControllerReadResponse(char *out_buffer, uint16_t buffer_size) {

    for (size_t i = 0; i < buffer_size; i++) {
        out_buffer[i] = sequansControllerReadByte();

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

                return SEQUANS_CONTROLLER_RESPONSE_OK;
            }

            char *error_termination_index =
                strstr(out_buffer, ERROR_TERMINATION);

            if (error_termination_index != NULL) {
                // We set the rest of the buffer from the "ERROR\r\n" to 0
                memset(error_termination_index,
                       0,
                       buffer_size - (error_termination_index - out_buffer));

                return SEQUANS_CONTROLLER_RESPONSE_ERROR;
            }
        }
    }

    // Didn't find the end marker within the number of bytes given for the
    // response. Caller should increase the buffer size.
    return SEQUANS_CONTROLLER_BUFFER_OVERFLOW;
}

uint8_t sequansControllerFlushResponseWithRetries(const uint8_t retries,
                                                  const double sleep_ms) {

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
    while (retry_count < retries) {

        if (!sequansControllerIsRxReady()) {
            retry_count++;
            _delay_ms(sleep_ms);
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
                return SEQUANS_CONTROLLER_RESPONSE_OK;
            }

            char *error_termination_index =
                strstr(termination_buffer, ERROR_TERMINATION);

            if (error_termination_index != NULL) {
                return SEQUANS_CONTROLLER_RESPONSE_ERROR;
            }
        }
    }

    return SEQUANS_CONTROLLER_RESPONSE_TIMEOUT;
}

uint8_t sequansControllerFlushResponse(void) {
    return sequansControllerFlushResponseWithRetries(
        SEQUANS_CONTROLLER_DEFAULT_FLUSH_RETRIES,
        SEQUANS_CONTROLLER_DEFAULT_FLUSH_SLEEP_MS);
}

bool sequansControllerExtractValueFromCommandResponse(
    char *response,
    const uint8_t index,
    char *buffer,
    const size_t buffer_size) {

    // Using strtok further down in this function would modify the original
    // string, so we create a copy. + 1 because of NULL termination.
    char response_copy[strlen(response) + 1];
    strcpy(response_copy, response);

    // Find the first occurrence of the data start character and move pointer to
    // there
    char *data = strchr(response_copy, DATA_START_CHARACTER);

    if (data == NULL) {
        return true;
    }

    // Increment pointer by 2 to skip the data start character and the following
    // space in the start sequence of the data
    data += 2;

    char *value = strtok(data, RESPONSE_DELIMITER);

    uint8_t value_index = 1;
    while (value != NULL && value_index <= index) {
        value = strtok(NULL, RESPONSE_DELIMITER);
        value_index++;
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
