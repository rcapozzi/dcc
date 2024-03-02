#include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/queue.h"
// #include "esp_log.h"
#include "esp32-hal-gpio.h"
#include "driver/rmt_rx.h"
#include "rmt_dcc_decoder.h"

#define DCC_RESOLUTION_HZ       1000000 // 1MHz resolution, 1 tick = 1us
#define DCC_DECODE_MARGIN       5       // us Tolerance for parsing RMT symbols into bit stream

#define DCC_PAYLOAD_ZERO_DURATION_0  560
#define DCC_PAYLOAD_ZERO_DURATION_1  560
#define DCC_PAYLOAD_ONE_DURATION_0   560
#define DCC_PAYLOAD_ONE_DURATION_1   1690

static const char *TAG = "rmt_dcc_decoder";

/**
 * @brief Is pulse duration is within expected range
 */
static inline bool dcc_check_in_range(uint32_t signal_duration, uint32_t spec_duration)
{
    return (signal_duration < (spec_duration + DCC_DECODE_MARGIN)) &&
           (signal_duration > (spec_duration - DCC_DECODE_MARGIN));
}

/**
 * @brief Is RMT symbol represents logic zero
 */
static bool dcc_parse_logic0(rmt_symbol_word_t *rmt_symbol)
{
    return dcc_check_in_range(rmt_symbol->duration0, DCC_PAYLOAD_ZERO_DURATION_0) &&
           dcc_check_in_range(rmt_symbol->duration1, DCC_PAYLOAD_ZERO_DURATION_1);
}

/**
 * @brief Is RMT symbol represents logic zero
 */
static bool dcc_parse_logic1(rmt_symbol_word_t *rmt_symbol)
{
    return dcc_check_in_range(rmt_symbol->duration0, DCC_PAYLOAD_ONE_DURATION_0) &&
           dcc_check_in_range(rmt_symbol->duration1, DCC_PAYLOAD_ONE_DURATION_1);
}

/**
 * @brief Check whether the RMT symbols represent DCC preamble code
 */
// static bool rmt_dcc_decoder_parse_frame_i(rmt_symbol_word_t *rmt_dcc_symbols)
// {
//     return dcc_check_in_range(rmt_dcc_symbols->duration0, DCC_CODE_DURATION_0) &&
//            dcc_check_in_range(rmt_dcc_symbols->duration1, DCC_CODE_DURATION_1);
// }

/**
 * @brief Decode RMT symbols into DCC scan code and print the result
 */
static void rmt_dcc_decoder_parse_frame(rmt_symbol_word_t *rmt_dcc_symbols, size_t symbol_num)
{
    printf("DCC frame start---\r\n");
    size_t i;
    for (i = 0; i < symbol_num; i++) {
        printf("i=%d {%d:%d},{%d:%d}  ", i, rmt_dcc_symbols[i].level0, rmt_dcc_symbols[i].duration0,
               rmt_dcc_symbols[i].level1, rmt_dcc_symbols[i].duration1);
        if (i%4==3) printf("\r\n");
    }
    if (i%4 != 4) printf("\r\n");
    printf("---DCC frame end\n");
    // decode RMT symbols
    switch (symbol_num) {
    case 2: // NEC repeat frame
        // if (nec_parse_frame_repeat(rmt_dcc_symbols)) {
        //     printf("Address=%04X, Command=%04X, repeat\r\n\r\n", s_nec_code_address, s_nec_code_command);
        // }
        break;
    default:
        // printf("Unknown DCC frame\r\n\r\n");
        break;
    }
}

static bool 
rmt_dcc_decoder_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data)
{
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t receive_queue = (QueueHandle_t)user_data;
    // send the received RMT symbols to the parser task
    xQueueSendFromISR(receive_queue, edata, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

void
rmt_dcc_decoder_main(void *pvParameters){
    // ESP_LOGI(TAG, "create RMT RX channel");
    rmt_rx_channel_config_t rx_channel_cfg = {
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = DCC_RESOLUTION_HZ,
        .mem_block_symbols = 64, // amount of RMT symbols that the channel can store at a time
        .gpio_num          = DCC_SIGNAL_PIN,
        // .flags.io_loop_back = 0,
        //.flags.with_dma    = 1,  // NOTE: ESP_ERR_NOT_SUPPORTED on C3
    };
    rmt_channel_handle_t rx_channel = NULL;
    ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_channel_cfg, &rx_channel));

    // ESP_LOGI(TAG, "register RX done callback");
    QueueHandle_t receive_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    assert(receive_queue);
    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = rmt_dcc_decoder_rx_done_callback,
    };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_channel, &cbs, receive_queue));

    // the following timing requirement is based on DCC protocol
    rmt_receive_config_t receive_config = {
        .signal_range_min_ns = 1250,          // the shortest duration for DCC signal. Assume 56us half wave is 1.
        .signal_range_max_ns = 250 * 1000,    // the longest duration for DCC signal
    };

    // Serial.println("Demo_DCC_Server_01_RMT enable RMT RX channels");
    // ESP_LOGI(TAG, "enable RMT RX channels");
    ESP_ERROR_CHECK(rmt_enable(rx_channel));

    // save the received RMT symbols
    rmt_symbol_word_t raw_symbols[64]; // Is 64 symbols sufficient
    rmt_rx_done_event_data_t rx_data;
    // ready to receive
    ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols, sizeof(raw_symbols), &receive_config));
    while (1) {
        // wait for RX done signal
        if (xQueueReceive(receive_queue, &rx_data, pdMS_TO_TICKS(1000)) == pdPASS) {
          // parse the receive symbols and print the result
          rmt_dcc_decoder_parse_frame(rx_data.received_symbols, rx_data.num_symbols);
          // start receive again
          ESP_ERROR_CHECK(rmt_receive(rx_channel, raw_symbols, sizeof(raw_symbols), &receive_config));
        } else {
          // timeout, transmit predefined IR NEC packets
          // Serial.println("Demo_DCC_Server_01_RMT Heartbeat from loop()");
          // ESP_LOGI(TAG, "No message timeout");
        }
    }
}
