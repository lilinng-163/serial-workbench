// SPDX-License-Identifier: Apache-2.0
// UART0 via an external USB-to-UART bridge, 115200 8N1.
// This source example is not part of the desktop application or installer.
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"

static constexpr uart_port_t CONTROL_UART = UART_NUM_0;

extern "C" void app_main(void)
{
    uart_config_t config = {};

    config.baud_rate = 115200;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_param_config(CONTROL_UART, &config));

    ESP_ERROR_CHECK(uart_set_pin(
        CONTROL_UART,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    ));

    ESP_ERROR_CHECK(uart_driver_install(CONTROL_UART, 2048, 0, 0, nullptr, 0));

    uint8_t buffer[256];

    ESP_LOGI("control_echo", "UART0 echo ready, 115200 8N1");

    while (true)
    {
        // Sleep until at least one byte arrives, instead of waiting for 256 bytes.
        int length = uart_read_bytes(CONTROL_UART, buffer, 1, portMAX_DELAY);

        if (length <= 0)
        {
            continue;
        }

        // Drain only bytes already buffered; zero timeout adds no batching delay.
        const int available = uart_read_bytes(
            CONTROL_UART,
            buffer + length,
            sizeof(buffer) - length,
            0
        );

        if (available > 0)
        {
            length += available;
        }

        uart_write_bytes(CONTROL_UART, buffer, length);
    }
}
