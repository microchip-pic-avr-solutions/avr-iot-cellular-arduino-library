#include "sequans_controller.h"

#include "log.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stddef.h>
#include <string.h>
#include <util/delay.h>

#include <cryptoauthlib.h>

#include <Arduino.h>
#include <pins_arduino.h>

#ifdef __AVR_AVR128DB48__ // MINI

#define TX_PIN PIN_PC0
#define RX_PIN PIN_PC1

#define CTS_PIN     PIN_PC4
#define CTS_PIN_bm  PIN4_bm
#define CTS_INT_bm  PORT_INT4_bm
#define RING_PIN    PIN_PC6
#define RING_INT_bm PORT_INT6_bm
#define RTS_PORT    PORTC
#define RTS_PIN     PIN_PC7
#define RTS_PIN_bm  PIN7_bm
#define RESET_PIN   PIN_PC5
#define HWSERIALAT  USART1

#else

#ifdef __AVR_AVR128DB64__ // Non-Mini

#define TX_PIN PIN_PC0
#define RX_PIN PIN_PC1

#define CTS_PIN     PIN_PC6
#define CTS_PIN_bm  PIN6_bm
#define CTS_INT_bm  PORT_INT6_bm
#define RING_PIN    PIN_PC4
#define RING_INT_bm PORT_INT4_bm
#define RTS_PORT    PORTC
#define RTS_PIN     PIN_PC7
#define RTS_PIN_bm  PIN7_bm
#define RESET_PIN   PIN_PE1
#define HWSERIALAT  USART1

#else
#error "INCOMPATIBLE_DEVICE_SELECTED"
#endif
#endif

#define SEQUANS_MODULE_BAUD_RATE 115200

// Defines for the amount of retries before we timeout and the interval between
// them
#define COMMAND_RETRY_SLEEP_MS 500
#define COMMAND_NUM_RETRIES    5

#define READ_TIMEOUT_MS 2000

// Sizes for the circular buffers
#define RX_BUFFER_SIZE 512
#define TX_BUFFER_SIZE 512

#define MAX_URC_CALLBACKS          10
#define URC_IDENTIFIER_BUFFER_SIZE 28

#define LINE_FEED          '\n'
#define CARRIAGE_RETURN    '\r'
#define SPACE_CHARACTER    ' '
#define RESPONSE_DELIMITER ','

/**
 * @brief State enumeration used in the USART RX ISR.
 */
typedef enum {
    URC_PARSING_IDENTIFIER,
    URC_EVALUATING_IDENTIFIER,
    URC_PARSING_DATA,
    URC_NOT_PARSING
} UrcParseState;

/**
 * @brief Struct for a registered URC with a callback.
 */
typedef struct {
    uint8_t identifier[URC_IDENTIFIER_BUFFER_SIZE] = "";

    /**
     * @brief The length of the URC registered is kept in a seperate variable in
     * order to make comparison in the interrupt faster. In that way we can
     * eliminate URC faster than e.g. calling strlen.
     */
    uint8_t identifier_length = 0;

    /**
     * @brief In order to prevent the receive buffer filling up for URCs where
     * the registered callback is called and the data is passed anyway, this
     * variable specify that we clear the contents in the receive buffer for
     * this specific URC.
     */
    bool should_clear = false;

    /**
     * @brief Callback for the URC.
     *
     * @param data The data content of the URC (everything after = and before
     * \r\n)
     */
    void (*callback)(char* data) = NULL;

} Urc;

static const char OK_TERMINATION[]    = "\r\nOK\r\n";
static const char ERROR_TERMINATION[] = "\r\nERROR\r\n";

// Specifies the valid bits for the index in the buffers. Use constepxr here in
// order to compute this at compile time and reduce the instruction of
// calculating the -1.
constexpr uint16_t RX_BUFFER_MASK = (RX_BUFFER_SIZE - 1);
constexpr uint16_t TX_BUFFER_MASK = (TX_BUFFER_SIZE - 1);

constexpr uint16_t RX_BUFFER_ALMOST_FULL = (RX_BUFFER_SIZE - 2);

static uint8_t rx_buffer[RX_BUFFER_SIZE];
static volatile uint16_t rx_head_index   = 0;
static volatile uint16_t rx_tail_index   = 0;
static volatile uint16_t rx_num_elements = 0;

static uint8_t tx_buffer[TX_BUFFER_SIZE];
static volatile uint16_t tx_head_index   = 0;
static volatile uint16_t tx_tail_index   = 0;
static volatile uint16_t tx_num_elements = 0;

/**
 * @brief Used to checking whether the cellular modem has been initialized.
 */
static bool initialized = false;

/**
 * @brief Whilst paring RX data, if we overcome an URC, the identifier is placed
 * in this buffer.
 */
static volatile uint8_t urc_identifier_buffer[URC_IDENTIFIER_BUFFER_SIZE];

/**
 * @brief The data part of the URC. We keep two buffers for identifier and data
 * so that the data buffer will only be overwritten if we find an URC we are
 * looking for.
 */
static volatile char urc_data_buffer[URC_DATA_BUFFER_SIZE];

/**
 * @brief Length (and index in the #urc_identifier_buffer) of the current URC
 * identifier parsed. Is used to compare against the table of registered URCs.
 * Is used to prevent having to call strlen() on the identifiers to reduce time
 * spent in the URC.
 */
static volatile uint16_t urc_identifier_buffer_length = 0;

/**
 * @brief Current length and index of the data part of the URC. Used to update
 * the data buffer.
 */
static volatile uint16_t urc_data_buffer_length = 0;

/**
 * @brief Current parsing state.
 */
static UrcParseState urc_parse_state = URC_NOT_PARSING;

/*
 * @brief If a registered URC callback match is found, this holds the index of
 * it in the lookup table.
 */
static volatile uint8_t urc_index = 0;

/**
 * @brief Keeps track of the current URCs registered.
 */
static volatile Urc urcs[MAX_URC_CALLBACKS] = {};

/**
 * @brief Used to keep a pointer to the URC we are processing and found to be
 * matching, which will fire after we are finished processing the URC.
 */
void (*urc_current_callback)(char*);

/**
 * @brief Power save mode for the modem. 1 is powering down the modem, 0 is
 * active.
 */
static volatile uint8_t power_save_mode = 0;

/**
 * @brief Function pointer to the ring line. Is registered and set to a function
 * address in #setPowerSaveMode().
 */
static void (*ring_line_callback)(void) = NULL;

/**
 * @brief Used within the RTS flow control update to assert or deassert the RTS
 * line such that the modem won't send any new data during a critical section.
 */
static bool critical_section_enabled = false;

/**
 * @brief Used for polling when the #waitForURC() callback gets called and thus
 * the URC has been registered.
 */
static volatile bool got_wait_for_urc_callback = false;

/**
 * @brief This holds the size of the data for the URC in #waitForURC(). Used in
 * order to prevent neededin to copy the total size of #URC_DATA_BUFFER_SIZE if
 * the user knows that the URC data will have a smaller size.
 */
static volatile uint16_t wait_for_urc_buffer_size = 0;

/**
 * @brief Is used to copy over the URC data in the #waitForURC() callback. This
 * buffer is later copied to the callers buffer (if not NULL).
 */
static char wait_for_urc_buffer[URC_DATA_BUFFER_SIZE];

/**
 * @brief Singleton. Defined for use of rest of library
 */
SequansControllerClass SequansController = SequansControllerClass::instance();

/** @brief Flow control update for the receive part of the USART interface with
 * the cellular modem.
 *
 * Updates RTS line based on space available in receive buffer. If the buffer
 * is close to full the RTS line is asserted (set high) to signal to the
 * target that no more data should be sent
 */
static inline void rtsUpdate(void) {
    // If we are in a power save mode, flow control is disabled until we get a
    // RING0 ack
    if (power_save_mode == 1) {
        return;
    }

    if (critical_section_enabled) {
        return;
    }

    // We prefer to not use arduino's digitalWrite here to reduce code in the
    // ISR
    if (rx_num_elements < RX_BUFFER_ALMOST_FULL) {
        // Space for more data, assert RTS line (active low)
        VPORTC.OUT &= (~RTS_PIN_bm);
    } else {
        // Buffer is filling up, tell the target to stop sending data
        // for now by de-asserting RTS
        VPORTC.OUT |= RTS_PIN_bm;
    }
}

/**
 * @brief Flow control update for the transmit part of the USART interface with
 * the cellular modem.
 *
 * Updates the USART's DREIE register if the CTS line is not asserted (logically
 * low) and the transmit buffer is not empty. This is necessary to do as the CTS
 * falling flank is sometimes missed due to having to use Arduino's
 * attachInterrupt() system. This adds quite a lot of instructions and the CTS
 * pulse is short (some microseconds), which leads to missing the flank.
 */
static inline void ctsUpdate(void) {
    if (!(HWSERIALAT.CTRLA & USART_DREIE_bm) && !(VPORTC.IN & CTS_PIN_bm) &&
        tx_num_elements > 0) {
        HWSERIALAT.CTRLA |= USART_DREIE_bm;
    }
}

/**
 * @brief Callback for the waitForURC() function. This will copy over the URC
 * data to the wait_for_urc_buffer.
 */
static void wait_for_urc_callback(char* urc_data) {
    memcpy(wait_for_urc_buffer, urc_data, wait_for_urc_buffer_size);
    got_wait_for_urc_callback = true;
}

void CTSInterrupt(void) {

    if (VPORTC.INTFLAGS & CTS_INT_bm) {

        if (VPORTC.IN & CTS_PIN_bm) {
            // CTS is not asserted (active low) so disable USART data register
            // empty interrupt where the logic is to send more data
            HWSERIALAT.CTRLA &= (~USART_DREIE_bm);
        } else {
            // CTS is asserted so we enable the USART data register empty
            // interrupt so more data can be sent
            HWSERIALAT.CTRLA |= USART_DREIE_bm;
        }

        VPORTC.INTFLAGS = CTS_INT_bm;
    }
}

void RingInterrupt(void) {
    if (VPORTC.INTFLAGS & RING_INT_bm) {
        if (VPORTC.IN & RING_PIN) {
            if (ring_line_callback != NULL) {
                ring_line_callback();
            }
        }

        VPORTC.INTFLAGS = RING_INT_bm;
    }
}

// RX complete
ISR(USART1_RXC_vect) {
    uint8_t data = USART1.RXDATAL;

    // We do an logical AND here as a means of allowing the index to wrap
    // around since we have a circular buffer
    rx_head_index            = (rx_head_index + 1) & RX_BUFFER_MASK;
    rx_buffer[rx_head_index] = data;
    rx_num_elements++;

    // Here we keep track of the length of the URC when it starts and
    // compare it against the look up table of lengths of the strings we are
    // looking for. We compare against them first in order to save some
    // cycles in the ISR and if the lengths match, we compare the string for
    // the URC and against the buffer. If they match, call the callback
    switch (urc_parse_state) {

    case URC_NOT_PARSING:

        if (data == URC_IDENTIFIER_START_CHARACTER) {
            urc_identifier_buffer_length = 0;
            urc_parse_state              = URC_EVALUATING_IDENTIFIER;
        }

        break;

    case URC_EVALUATING_IDENTIFIER:

        // Some commands return a '+' followed by numbers which can be mistaken
        // for an URC, so we check against that and disregard the data if that
        // is the case
        if (data >= '0' && data <= '9') {
            urc_parse_state = URC_NOT_PARSING;
        } else {
            urc_identifier_buffer[urc_identifier_buffer_length++] = data;
            urc_parse_state = URC_PARSING_IDENTIFIER;
        }
        break;

    case URC_PARSING_IDENTIFIER:

        if (data == URC_IDENTIFIER_END_CHARACTER || data == CARRIAGE_RETURN) {

            // We set this as the initial condition and if we find a match
            // for the URC we go on parsing the data
            urc_parse_state = URC_NOT_PARSING;

            for (uint8_t i = 0; i < MAX_URC_CALLBACKS; i++) {

                if (urcs[i].identifier_length == urc_identifier_buffer_length) {
                    if (memcmp((const void*)urc_identifier_buffer,
                               (const void*)urcs[i].identifier,
                               urcs[i].identifier_length) == 0) {

                        urc_index            = i;
                        urc_current_callback = urcs[i].callback;
                        urc_parse_state      = URC_PARSING_DATA;

                        // Clear data if requested and if the data hasn't
                        // already been read
                        // We apply the + 2 here as we also want to remove the
                        // start character and the end character of the URC (+
                        // and :/line feed)
                        if (urcs[urc_index].should_clear &&
                            rx_num_elements >=
                                (urc_identifier_buffer_length + 2)) {

                            rx_head_index =
                                (rx_head_index -
                                 (urc_identifier_buffer_length + 2)) &
                                RX_BUFFER_MASK;

                            rx_num_elements -= (urc_identifier_buffer_length +
                                                2);
                        }

                        // Reset the index in order to prepare the URC
                        // buffer for data
                        urc_data_buffer_length = 0;

                        break;
                    }
                }
            }
            urc_identifier_buffer_length = 0;

        } else if (urc_identifier_buffer_length == URC_IDENTIFIER_BUFFER_SIZE) {
            urc_parse_state = URC_NOT_PARSING;
        } else {
            urc_identifier_buffer[urc_identifier_buffer_length++] = data;
        }

        break;

    case URC_PARSING_DATA:

        if (data == CARRIAGE_RETURN || data == LINE_FEED) {

            // Add termination since we're done
            urc_data_buffer[urc_data_buffer_length] = 0;

            // Clear the buffer for the URC if requested and if it already
            // hasn't been read
            if (urcs[urc_index].should_clear &&
                rx_num_elements >= urc_data_buffer_length) {

                rx_head_index = (rx_head_index - urc_data_buffer_length) &
                                RX_BUFFER_MASK;
                rx_num_elements = (rx_num_elements - urc_data_buffer_length);
            }

            if (urc_current_callback != NULL) {
                // Apply flow control here for the modem, we make it wait to
                // send more data until we've finished the URC callback
                RTS_PORT.OUTSET = RTS_PIN_bm;
                urc_current_callback((char*)urc_data_buffer);
                urc_current_callback = NULL;
                RTS_PORT.OUTCLR      = RTS_PIN_bm;
            }

            urc_parse_state        = URC_NOT_PARSING;
            urc_data_buffer_length = 0;

        } else if (urc_data_buffer_length == URC_DATA_BUFFER_SIZE) {
            // This is just a failsafe
            urc_parse_state = URC_NOT_PARSING;
        } else {
            urc_data_buffer[urc_data_buffer_length++] = data;
        }
        break;

    default:
        break;
    }

    rtsUpdate();
}

/**
 * @brief Data register empty. Allows us to keep track of when the data has
 * been transmitted on the line and set up new data to be transmitted from
 * the ring buffer.
 */
ISR(USART1_DRE_vect) {
    if (tx_num_elements != 0) {
        tx_tail_index      = (tx_tail_index + 1) & TX_BUFFER_MASK;
        HWSERIALAT.TXDATAL = tx_buffer[tx_tail_index];
        tx_num_elements--;
    } else {
        HWSERIALAT.CTRLA &= (~USART_DREIE_bm);
    }
}

void SequansControllerClass::begin(void) {

    // PIN SETUP
    pinConfigure(TX_PIN, PIN_DIR_OUTPUT | PIN_INPUT_ENABLE);
    pinConfigure(RX_PIN, PIN_DIR_INPUT | PIN_INPUT_ENABLE);

    // Request to send (RTS) and clear to send (CTS) are the control lines
    // on the UART line. From the configuration the MCU and the LTE modem is
    // in, we control the RTS line from the MCU to signalize if we can
    // process more data or not from the LTE modem. The CTS line is
    // controlled from the LTE modem and gives us the ability to know
    // whether the LTE modem can receive more data or if we have to wait.
    //
    // Both pins are active low.

    pinConfigure(RTS_PIN, PIN_DIR_OUTPUT | PIN_INPUT_ENABLE);
    digitalWrite(RTS_PIN, HIGH);

    // Clear to send is input and we want interrupts on both edges to know
    // when the LTE modem has changed the state of the line.
    pinConfigure(CTS_PIN,
                 PIN_DIR_INPUT | PIN_PULLUP_ON | PIN_INT_CHANGE |
                     PIN_INPUT_ENABLE);

    // We use attach interrupt here instead of the ISR directly as other
    // libraries might use the same ISR and we don't want to override it to
    // create a linker issue
    attachInterrupt(CTS_PIN, CTSInterrupt, CHANGE);

    // Set reset low to reset the LTE modem
    pinConfigure(RESET_PIN, PIN_DIR_OUTPUT | PIN_INPUT_ENABLE);
    digitalWrite(RESET_PIN, HIGH);
    delay(10);
    digitalWrite(RESET_PIN, LOW);

    // SERIAL INTERFACE SETUP

    // LTE modules has set baud rate of 115200 for its UART0 interface
    USART1.BAUD = (uint16_t)(((float)F_CPU * 64 /
                              (16 * (float)SEQUANS_MODULE_BAUD_RATE)) +
                             0.5);

    // Interrupt on receive completed
    USART1.CTRLA = USART_RXCIE_bm | USART_DREIE_bm;

    USART1.CTRLB = USART_RXEN_bm | USART_TXEN_bm;

    // LTE module interface requires 8 data bits with one stop bit
    USART1.CTRLC = USART_CMODE_ASYNCHRONOUS_gc | USART_SBMODE_1BIT_gc |
                   USART_CHSIZE_8BIT_gc;

    rtsUpdate();

    // Wait for SYSSTART URC before we continue
    if (!waitForURC("SYSSTART")) {
        Log.error("Timed out waiting for cellular modem to start up\r\n");
        return;
    }

    clearReceiveBuffer();

    initialized = true;
}

bool SequansControllerClass::isInitialized(void) { return initialized; }

void SequansControllerClass::end(void) {
    USART1.CTRLA = 0;
    USART1.CTRLB = 0;
    USART1.CTRLC = 0;

    pinConfigure(RESET_PIN, PIN_INPUT_DISABLE | PIN_DIR_INPUT);

    // Set RTS high to halt the modem. Has external pull-up, so is just set to
    // input afterwards
    digitalWrite(RTS_PIN, HIGH);
    pinConfigure(RTS_PIN, PIN_DIR_INPUT | PIN_INPUT_DISABLE);

    pinConfigure(RING_PIN, PIN_DIR_INPUT | PIN_INPUT_DISABLE);
    detachInterrupt(RING_PIN);

    pinConfigure(CTS_PIN, PIN_DIR_INPUT | PIN_INPUT_DISABLE);
    detachInterrupt(CTS_PIN);

    pinConfigure(TX_PIN, PIN_DIR_INPUT | PIN_PULLUP_ON | PIN_INPUT_DISABLE);
    pinConfigure(RX_PIN, PIN_DIR_INPUT | PIN_PULLUP_ON | PIN_INPUT_DISABLE);

    initialized = false;
}

bool SequansControllerClass::isTxReady(void) {
    return tx_num_elements != TX_BUFFER_SIZE;
}

bool SequansControllerClass::isRxReady(void) { return rx_num_elements > 0; }

void SequansControllerClass::writeBytes(const uint8_t* data,
                                        const size_t buffer_size,
                                        const bool append_carriage_return) {

    for (size_t i = 0; i < buffer_size; i++) {
        cli();
        tx_head_index            = (tx_head_index + 1) & TX_BUFFER_MASK;
        tx_buffer[tx_head_index] = data[i];
        tx_num_elements++;

        if (i == buffer_size - 1 && append_carriage_return) {
            tx_head_index            = (tx_head_index + 1) & TX_BUFFER_MASK;
            tx_buffer[tx_head_index] = '\r';
            tx_num_elements++;
        }

        sei();
    }

    while (VPORTC.IN & CTS_PIN_bm) { delay(1); }

    // Enable data register empty interrupt so that the data gets pushed out
    HWSERIALAT.CTRLA |= USART_DREIE_bm;
}

int16_t SequansControllerClass::readByte(void) {
    if (!isRxReady()) {
        return -1;
    }

    // Disable interrupts temporarily here to prevent being interleaved
    // in the middle of updating the tail index
    cli();
    const uint16_t next_tail_index = (rx_tail_index + 1) & RX_BUFFER_MASK;
    rx_tail_index                  = next_tail_index;
    rx_num_elements--;
    sei();

    rtsUpdate();

    return rx_buffer[next_tail_index];
}

ResponseResult
SequansControllerClass::writeCommand(const char* command,
                                     char* result_buffer,
                                     const size_t result_buffer_size) {

    clearReceiveBuffer();
    Log.debugf("Sending AT command: %s", command);
    ResponseResult response;

    uint8_t retry_count = 0;

    do {
        writeBytes((const uint8_t*)command, strlen(command), true);
        response = readResponse(result_buffer, result_buffer_size);

        if (response != ResponseResult::OK) {
            delay(COMMAND_RETRY_SLEEP_MS);
        }
    } while (response != ResponseResult::OK &&
             retry_count++ < COMMAND_NUM_RETRIES);

    char response_string[18];
    responseResultToString(response, response_string);

    if (Log.getLogLevel() == LogLevel::DEBUG) {
        Log.rawf(" -> %s\r\n", response_string);
    }

    return response;
}

ResponseResult
SequansControllerClass::readResponse(char* out_buffer,
                                     const size_t out_buffer_size) {

    // Enough to hold the OK and ERROR termination if the out_buffer is NULL
    // and the result is not needed
    char placeholder_buffer[10] = "";

    char* buffer       = placeholder_buffer;
    size_t buffer_size = sizeof(placeholder_buffer);

    if (out_buffer != NULL && buffer_size != 0) {
        buffer      = out_buffer;
        buffer_size = out_buffer_size;
    }

    // Safe guard and place null termination at end of buffer
    buffer[buffer_size - 1] = '\0';

    size_t i = 0;

    uint64_t start = millis();

    while (i < buffer_size) {
        while (!isRxReady() && millis() - start < READ_TIMEOUT_MS) {
            // We update the CTS here in case the CTS interrupt didn't catch the
            // falling flank
            ctsUpdate();

            delay(1);
        }

        if (millis() - start >= READ_TIMEOUT_MS) {
            return ResponseResult::TIMEOUT;
        }

        // Reset timeout timer
        start = millis();

        buffer[i++] = (uint8_t)readByte();

        // We won't check for the buffer having a termination until at least
        // 2 bytes are in it
        if (i == 0) {
            continue;
        }

        // For AT command responses from the LTE module, "\r\nOK\r\n" or
        // "\r\nERROR\r\n" signifies the end of a response, so we look
        // "\r\n".
        //
        // Since we post increment the i variable, we have to take that into
        // consideration and look for the last 2 elements after the variable
        // is incremented
        if (buffer[i - 2] == CARRIAGE_RETURN && buffer[i - 1] == LINE_FEED) {

            char* ok_index = strstr(buffer, OK_TERMINATION);
            if (ok_index != NULL) {
                // Terminate and omit the rest from the OK index.
                *ok_index = '\0';
                return ResponseResult::OK;
            }

            char* error_index = strstr(buffer, ERROR_TERMINATION);
            if (error_index != NULL) {
                // Terminate and omit the rest from the ERROR index
                *error_index = '\0';
                return ResponseResult::ERROR;
            }
        }
    }

    // Didn't find the end marker within the number of bytes given for the
    // response. Caller should increase the buffer size.
    return ResponseResult::BUFFER_OVERFLOW;
}

void SequansControllerClass::clearReceiveBuffer(void) {
    cli();
    rx_num_elements = 0;
    rx_tail_index   = rx_head_index;
    sei();

    rtsUpdate();
}

bool SequansControllerClass::extractValueFromCommandResponse(
    char* response,
    const uint8_t index,
    char* buffer,
    const size_t buffer_size,
    const char start_character) {

    // We need a copy in order to not modify the original
    size_t rcp_size = strlen(response) + 1;
    char response_copy[rcp_size];
    strncpy(response_copy, response, rcp_size);

    // Enforce non buffer overflow
    response_copy[rcp_size - 1] = '\0';

    char* data;

    if (start_character != 0) {

        // Find the first occurrence of the data start character and move
        // pointer to there
        data = strchr(response_copy, start_character);

        if (data == NULL) {
            return false;
        }

        // Increment pointer to skip the data start character (and the
        // following space in the start sequence of the data if it is there)
        while (*data == start_character || *data == SPACE_CHARACTER) { data++; }
    } else {
        // If no start character is given, just set data start to string
        // start
        data = response_copy;
    }

    // Now we split the string by the response delimiter and search for the
    // index we're interested in
    //
    // We refrain from using strtok to split the string here as it will not
    // take into account empty strings between the delimiter, which can be
    // the case with certain command responses

    // These keep track of the contens between the delimiter
    char* start_value_ptr = data;
    char* end_value_ptr   = strchr(data, RESPONSE_DELIMITER);

    // We did not find the delimiter at all, abort if the index we request
    // is > 0. If it is 0, the command might only consist of one entry and
    // not have a delimiter
    if (end_value_ptr == NULL && index > 0) {
        return false;
    }

    uint8_t value_index = 1;

    while (end_value_ptr != NULL && value_index <= index) {
        // Find next occurrence and update accordingly
        start_value_ptr = end_value_ptr + 1;
        end_value_ptr   = strchr(start_value_ptr, RESPONSE_DELIMITER);
        value_index++;
    }

    // If we got all the way to the end, set the end_value_ptr to the end of
    // the data ptr
    if (end_value_ptr == NULL) {
        end_value_ptr = data + strlen(data);
    }
    end_value_ptr[0] = 0; // Add null termination

    // If found, set termination to the carriage return. If not, leave the
    // string be as it is
    char* first_carriage_return = strchr(start_value_ptr, '\r');
    if (first_carriage_return != NULL) {
        *first_carriage_return = 0;
    }

    size_t value_length = strlen(start_value_ptr);
    // We compare inclusive for value length as we want to take the null
    // termination into consideration. So the buffer size has be
    // value_length + 1
    if (value_length >= buffer_size) {
        Log.error("Buffer too small for value when extracting value for "
                  "command response, increase the buffer size");
        return false;
    }

    strcpy(buffer, start_value_ptr);

    return true;
}

bool SequansControllerClass::registerCallback(const char* urc_identifier,
                                              void (*urc_callback)(char*),
                                              const bool clear_data) {

    uint8_t urc_identifier_length = strlen(urc_identifier);

    // Check if we can override first
    for (uint8_t i = 0; i < MAX_URC_CALLBACKS; i++) {
        if (urcs[i].identifier_length == urc_identifier_length &&

            strcmp(urc_identifier, (const char*)urcs[i].identifier) == 0) {
            urcs[i].callback     = urc_callback;
            urcs[i].should_clear = clear_data;

            return true;
        }
    }

    // Look for empty spot
    for (uint8_t i = 0; i < MAX_URC_CALLBACKS; i++) {
        if (urcs[i].identifier_length == 0) {

            strcpy((char*)urcs[i].identifier, urc_identifier);
            urcs[i].identifier_length = strlen(urc_identifier);
            urcs[i].callback          = urc_callback;
            urcs[i].should_clear      = clear_data;

            return true;
        }
    }

    Log.error("Max amount of URC callbacks for SequansController reached");
    return false;
}

void SequansControllerClass::unregisterCallback(const char* urc_identifier) {
    const uint8_t urc_identifier_length = strlen(urc_identifier);

    for (uint8_t i = 0; i < MAX_URC_CALLBACKS; i++) {
        if (memcmp((const void*)urc_identifier,
                   (const void*)urcs[i].identifier,
                   urc_identifier_length) == 0) {

            // No need to fill the look up table identifier table, as we
            // override it if a new registration is issued, but the length
            // is used to check if the slot is active or not, so we set that
            // to 0 and reset the callback pointer for house keeping
            urcs[i].identifier_length = 0;
            urcs[i].callback          = NULL;
            break;
        }
    }
}

bool SequansControllerClass::waitForURC(const char* urc_identifier,
                                        char* out_buffer,
                                        const uint16_t out_buffer_size,
                                        const uint64_t timeout_ms) {
    got_wait_for_urc_callback = false;
    wait_for_urc_buffer_size  = out_buffer_size;

    // We might hit the maximum amount of URC callbacks allowed, so return
    // if that is the case
    if (!registerCallback(urc_identifier, wait_for_urc_callback)) {
        return false;
    }

    const uint64_t start = millis();

    while (!got_wait_for_urc_callback && millis() - start < timeout_ms) {
        // We update the CTS here in case the CTS interrupt didn't catch the
        // falling flank
        ctsUpdate();

        delay(1);
    }

    unregisterCallback(urc_identifier);

    if (millis() - start < timeout_ms) {
        if (out_buffer != NULL) {
            memcpy(out_buffer, wait_for_urc_buffer, out_buffer_size);
        }
        return true;
    } else {
        return true;
    }
}

void SequansControllerClass::setPowerSaveMode(const uint8_t mode,
                                              void (*ring_callback)(void)) {

    Log.debugf("Setting power save mode %d\r\n", mode);

    if (mode == 0) {
        ring_line_callback = NULL;
        power_save_mode    = 0;

        // Clear interrupt
        pinConfigure(RING_PIN, PIN_DIR_INPUT);
        detachInterrupt(RING_PIN);

        RTS_PORT.OUTCLR |= RTS_PIN_bm;
    } else if (mode == 1) {

        if (ring_callback != NULL) {
            ring_line_callback = ring_callback;

            // We have interrupt on change here since there is sometimes
            // a too small interval for the sensing to sense a rising edge.
            // This is fine as any change will yield that we are out of
            // power save mode.
            pinConfigure(RING_PIN, PIN_DIR_INPUT | PIN_INT_CHANGE);
            attachInterrupt(RING_PIN, RingInterrupt, CHANGE);
        }

        power_save_mode = 1;
        RTS_PORT.OUTSET |= RTS_PIN_bm;
    }
}

void SequansControllerClass::responseResultToString(
    const ResponseResult response_result,
    char* response_string) {

    switch (response_result) {
    case ResponseResult::OK:
        strcpy(response_string, "OK");
        break;
    case ResponseResult::ERROR:
        strcpy(response_string, "ERROR");
        break;
    case ResponseResult::BUFFER_OVERFLOW:
        strcpy(response_string, "BUFFER_OVERFLOW");
        break;
    case ResponseResult::TIMEOUT:
        strcpy(response_string, "TIMEOUT");
        break;
    case ResponseResult::SERIAL_READ_ERROR:
        strcpy(response_string, "SERIAL_READ_ERROR");
        break;
    case ResponseResult::NONE:
        strcpy(response_string, "NONE");
        break;
    }
}

bool SequansControllerClass::waitForByte(const uint8_t byte,
                                         const uint32_t timeout) {
    int16_t read_byte = SequansController.readByte();

    uint32_t start = millis();

    while (read_byte != byte) {
        read_byte = SequansController.readByte();

        // We update the CTS here in case the CTS interrupt didn't catch the
        // falling flank
        ctsUpdate();

        if (millis() - start >= timeout) {
            return false;
        }
    }

    return true;
}

void SequansControllerClass::startCriticalSection(void) {
    critical_section_enabled = true;
    RTS_PORT.OUTSET          = RTS_PIN_bm;
}

void SequansControllerClass::stopCriticalSection(void) {
    critical_section_enabled = false;
    RTS_PORT.OUTCLR          = RTS_PIN_bm;
}
