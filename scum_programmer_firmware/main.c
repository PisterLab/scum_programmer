#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "app_uart.h"
#include "string.h"
#include "app_error.h"
#include "nrf_delay.h"
#include "nrf.h"
#include "bsp.h"
#include "nrf_uart.h"
#include "nrf.h"
#include "nrf_drv_timer.h"

#define UART_TX_BUF_SIZE 256                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE 256                         /**< UART RX buffer size. */

// demo leds (for applicaiton could switch to other pins)
#define LED_data 13
#define LED_latch 14
#define LED_clk 15
#define LED_finish 16

/* When UART is used for communication with the host do not use flow control.*/
#define UART_HWFC UART_CONFIG_HWFC_Enabled


uint8_t cr;  // point to character received
#define MAX_CMD_LEN 512
char cmd1[MAX_CMD_LEN];  // place to hold the received command
char cmd2[MAX_CMD_LEN];  // place to hold the received command
bool boot_flag = false;
// allocate space for the binary data
uint8_t bin_data[65536];


/**
 * @brief Creates a timer instance (TIMER0)
 */
const nrfx_timer_t TIMER_3WB = NRFX_TIMER_INSTANCE(0);


// define the structure for holding the different time ticks
typedef struct times_st {
    uint32_t comp_event_0_time;
    uint32_t comp_event_1_time;
    uint32_t comp_event_2_time;
    uint32_t comp_event_3_time;
} Times, *TimesPtr;


/**
 * @brief Initialize the event triggering times using the event_times as the output parameter
 */
void init_event_trigger_times(TimesPtr event_times) {
    // calculate the different time ticks that trigger the compare events
    event_times->comp_event_0_time = nrfx_timer_ms_to_ticks(&TIMER_3WB, 100);  // 100ms
    event_times->comp_event_1_time = nrfx_timer_ms_to_ticks(&TIMER_3WB, 200);  // 200ms
    event_times->comp_event_2_time = nrfx_timer_ms_to_ticks(&TIMER_3WB, 300);  // 300ms
    event_times->comp_event_3_time = nrfx_timer_ms_to_ticks(&TIMER_3WB, 400);  // 400ms
}



/**
 * @brief Handler for timer events.
 *     COMPARE0: write data_pin, update index
 *     COMPARE1: write latch pin
 *     COMPARE2: set clk pin
 *     COMPARE3: clear clk pin, reset / disable timer based on the index
 */
void timer_event_handler(nrf_timer_event_t event_type, void* p_context) {
    static uint32_t byte_index = 0;  // initialize the byte index
    static uint8_t bit_index = 0;    // initialize the bit index
    uint8_t cur_byte;
    uint8_t cur_bit;

    // different event-handling based on condition
    switch (event_type) {
        case NRF_TIMER_EVENT_COMPARE0:
            // write the data pin
            // take data from the loaded region
             
            cur_byte = bin_data[byte_index];
            cur_bit = (cur_byte >> bit_index) & 1;
            if (cur_bit == 0) {
                nrf_gpio_pin_set(LED_data);
            } else {
                nrf_gpio_pin_clear(LED_data);
            }

            // increment the index
            bit_index += 1;
            if (bit_index == 8) {
                byte_index += 1;
                bit_index = 0;
            }
            break;

        case NRF_TIMER_EVENT_COMPARE1:
            // write the latch pin: every 32 bits
            if (bit_index == 0 && byte_index % 4 == 0) {
                nrf_gpio_pin_set(LED_latch);
            } else {
                nrf_gpio_pin_clear(LED_latch);
            }
            break;

        case NRF_TIMER_EVENT_COMPARE2:
            // set the clk pin
            nrf_gpio_pin_clear(LED_clk);            
            break;

        case NRF_TIMER_EVENT_COMPARE3:
            // clear the clk pin
            nrf_gpio_pin_set(LED_clk);      
            if (byte_index == 32 * 2) {
                nrf_gpio_pin_clear(LED_finish);
                // when bootload completes, send success message to the PC 
                sendMessage("bootload_success\n", 17);
                nrf_drv_timer_disable(&TIMER_3WB);
            }
            break;

        default:
            //Do nothing.
            break;
    }
}


/**
 * @brief Initialize the timer.
 */
void initialize_timer(void) {
    uint32_t err_code = NRF_SUCCESS;
    Times event_time_ticks;

    // structure that holds the tiemr config
    nrfx_timer_config_t timer_config = NRFX_TIMER_DEFAULT_CONFIG;

    // initialize the TIEMR0 and does error check
    err_code = nrfx_timer_init(&TIMER_3WB, &timer_config, timer_event_handler);
    APP_ERROR_CHECK(err_code);

    // initialize the time ticks of different compare events
    init_event_trigger_times(&event_time_ticks);

    // assign these to different compare channels, and enable the interrupt
    nrfx_timer_compare(&TIMER_3WB, NRF_TIMER_CC_CHANNEL0, (&event_time_ticks)->comp_event_0_time, true);
    nrfx_timer_compare(&TIMER_3WB, NRF_TIMER_CC_CHANNEL1, (&event_time_ticks)->comp_event_1_time, true);
    nrfx_timer_compare(&TIMER_3WB, NRF_TIMER_CC_CHANNEL2, (&event_time_ticks)->comp_event_2_time, true);
    // reset the TIMER_3WB
    nrfx_timer_extended_compare(&TIMER_3WB, NRF_TIMER_CC_CHANNEL3, (&event_time_ticks)->comp_event_3_time, NRF_TIMER_SHORT_COMPARE3_CLEAR_MASK, true);

}


/**
 * @brief Initialize the 3-wire-bus output pins. For demo purposes, use the LED to display.
 */
void three_wire_bus_pin_init(void) {
    nrf_gpio_cfg_output(LED_data);
    nrf_gpio_cfg_output(LED_latch);
    nrf_gpio_cfg_output(LED_clk);
    nrf_gpio_cfg_output(LED_finish);

    // turn off the leds initially
    nrf_gpio_pin_set(LED_data);
    nrf_gpio_pin_set(LED_latch);
    nrf_gpio_pin_set(LED_clk);
    nrf_gpio_pin_set(LED_finish);
}



// uart error handler
void uart_error_handle(app_uart_evt_t * p_event) {
    if (p_event->evt_type == APP_UART_COMMUNICATION_ERROR) {
        APP_ERROR_HANDLER(p_event->data.error_communication);
    } else if (p_event->evt_type == APP_UART_FIFO_ERROR) {
        APP_ERROR_HANDLER(p_event->data.error_code);
    }
}


// send message to the PC
void sendMessage(char * message, int mes_len) {
    for (int i = 0; i < mes_len; i++) {
        // put a new byte to be sent
        while (app_uart_put(message[i]) != NRF_SUCCESS);
    }
}


// receive message from PC
bool receiveMessage(char * buffer, int start_index, bool cmd_flag) {
    while (1) {
        while (app_uart_get(&cr) != NRF_SUCCESS);
        buffer[start_index] = cr;
//        if (cmd_flag == 0) {
//            printf("%x", buffer[start_index]);
//        }
        start_index += 1;


        if (cmd_flag) {
            if (cr == '\n') {
                if (strcmp(buffer, "TRANSFER\n") == 0) {
                    // printf("command is: %s\r\n", buffer);
                    sendMessage("transfer_started\n", 17);
                } 
                
                if (strcmp(buffer, "BOOT3WB\n") == 0) {
                    boot_flag = true;
                    sendMessage("bootload_started\n", 17);                    
                }
                start_index = 0;
                return true;
            }
        } else {
            if (start_index == 65536) {
                sendMessage("data_ack\n", 9);
                return true;
            }
        }
    }
}


/**
 * @brief Function for main application entry.
 */
int main(void) {
    uint32_t err_code;
    bool res;
    const app_uart_comm_params_t comm_params = {
        RX_PIN_NUMBER,
        TX_PIN_NUMBER,
        RTS_PIN_NUMBER,
        CTS_PIN_NUMBER,
        UART_HWFC,
        false,
        NRF_UART_BAUDRATE_115200
    };

    // init the uart
    APP_UART_FIFO_INIT(&comm_params, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE, uart_error_handle, APP_IRQ_PRIORITY_LOWEST, err_code);
    APP_ERROR_CHECK(err_code);

    // now we can use the uart
    // Print a start msg over uart port
    //printf("Hello PC from nordic Device!!\n", 30);

    // step 1: receive message from PC
    res = receiveMessage(cmd1, 0, true);

    // step 2: receive the data
    res = receiveMessage(bin_data, 0, false);

    // step 3: initiate 3-wire-bus program if the user sends the command
    res = receiveMessage(cmd2, 0, true);

    // initialize the output pins (demo uses LEDs)
    three_wire_bus_pin_init();

    // start the timer
    initialize_timer();

    if (boot_flag) {
        // enable the timer
        nrfx_timer_enable(&TIMER_3WB);
        // clean up
    }

    while (1) {
        __WFI();  // CPU low power mode
    }

}



