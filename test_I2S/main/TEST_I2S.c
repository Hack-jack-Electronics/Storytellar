#include <stdio.h>
#include "driver/i2s.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 44100
#define TONE_HZ 1000
#define DURATION_SEC 5

void app_main(void) {
    // I2S config
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .dma_buf_count = 4,
        .dma_buf_len = 512,
        .intr_alloc_flags = 0,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    // I2S pin config (change to your actual wiring)
    i2s_pin_config_t pin_config = {
        .bck_io_num = 3,    // Bit Clock
        .ws_io_num = 9,     // Word Select (LRCK)
        .data_out_num = 14,  // Data Out
        .data_in_num = -1   // Not used
    };

    // Install and start I2S
    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);
    i2s_zero_dma_buffer(I2S_PORT);

    // Generate 1 kHz sine wave
    int samples = SAMPLE_RATE * DURATION_SEC;
    int16_t *buffer = malloc(samples * sizeof(int16_t));
    for (int i = 0; i < samples; i++) {
        buffer[i] = (int16_t)(32767 * sin(2 * M_PI * TONE_HZ * i / SAMPLE_RATE));
    }

    size_t bytes_written;
    i2s_write(I2S_PORT, buffer, samples * sizeof(int16_t), &bytes_written, portMAX_DELAY);

    // Clean up
    free(buffer);
    i2s_driver_uninstall(I2S_PORT);
}
