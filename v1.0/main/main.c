/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_board_init.h"
#include "model_path.h"
#include "string.h"
#include "driver/i2s_std.h"

int detect_flag = 0;
static esp_afe_sr_iface_t *afe_handle = NULL;
static volatile int task_flag = 0;
static i2s_chan_handle_t rx_handle;

// Funções para configurar o AFE para um microfone
char* esp_get_input_format() { return "M"; }
int esp_get_feed_channel() { return 1; }

void feed_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;
    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    
    // Buffer para os dados brutos de 32 bits lidos do microfone
    int32_t* raw_i2s_buffer = malloc(audio_chunksize * sizeof(int32_t));
    assert(raw_i2s_buffer);

    // Buffer para os dados de 16 bits processados que serão enviados ao AFE
    int16_t* afe_buffer = malloc(audio_chunksize * sizeof(int16_t));
    assert(afe_buffer);

    size_t bytes_read;

    while (task_flag) {
        // 1. Lê os dados brutos de 32 bits do microfone
        i2s_channel_read(rx_handle, raw_i2s_buffer, audio_chunksize * sizeof(int32_t), &bytes_read, portMAX_DELAY);
        
        int samples_read = bytes_read / sizeof(int32_t);
        if (samples_read > 0) {
            // 2. Processa cada amostra, fazendo o bit shift para corrigir o formato do áudio
            for (int i = 0; i < samples_read; i++) {
                afe_buffer[i] = (int16_t)(raw_i2s_buffer[i] >> 16);
            }
            
            // 3. Alimenta o AFE com o áudio já processado e correto
            afe_handle->feed(afe_data, afe_buffer);
        }
    }

    free(raw_i2s_buffer);
    free(afe_buffer);
    vTaskDelete(NULL);
}

void detect_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = arg;
    printf("------------detect start------------\n");

    while (task_flag) {
        afe_fetch_result_t* res = afe_handle->fetch(afe_data); 
        if (!res || res->ret_value == ESP_FAIL) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (res->wakeup_state == WAKENET_DETECTED) {
            printf(">>>> WAKEWORD DETECTED! <<<<\n");
            printf("model index:%d, word index:%d\n", res->wakenet_model_index, res->wake_word_index);
            printf("-----------LISTENING-----------\n");
        }
    }
    vTaskDelete(NULL);
}

void app_main()
{
    // 1. Configuração do I2S para o microfone INMP441 nos seus pinos
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));
    
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_1,
            .ws   = GPIO_NUM_2,
            .dout = I2S_GPIO_UNUSED,
            .din  = GPIO_NUM_3,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv   = false, },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

    // 2. Configuração do AFE (Audio Front-End)
    srmodel_list_t *models = esp_srmodel_init("model");
    afe_config_t *afe_config = afe_config_init("MM", models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
    
    if (afe_config->wakenet_model_name) {
        printf("wakeword model in AFE config: %s\n", afe_config->wakenet_model_name);
    }

    afe_handle = esp_afe_handle_from_config(afe_config);
    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(afe_config);
    
    afe_config_free(afe_config);
    
    // 3. Criação das Tarefas de áudio e detecção
    task_flag = 1;
    xTaskCreatePinnedToCore(&feed_Task, "feed", 8 * 1024, (void*)afe_data, 5, NULL, 0);
    xTaskCreatePinnedToCore(&detect_Task, "detect", 4 * 1024, (void*)afe_data, 5, NULL, 1);
}
