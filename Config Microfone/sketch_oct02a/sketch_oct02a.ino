/*
 * Título: Transcritor de Voz com ESP32-S3 (Versão Final Corrigida)
 * Descrição: Esta versão implementa a correção crítica descoberta a partir
 * do código de FFT fornecido pelo usuário. O driver I2S agora é configurado
 * para ler amostras de 32 bits, e um deslocamento de bits (bit shift) é
 * aplicado para extrair os dados de áudio corretos. Isso resolve o problema
 * de baixo volume na fonte, eliminando a necessidade de amplificação digital.
 */

// --- INCLUDES ---
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "driver/i2s.h"

// --- CONFIGURAÇÃO DO USUÁRIO ---
const char* WIFI_SSID = "Apto-26";
const char* WIFI_PASSWORD = "apto26px26";
String WIT_AI_SERVER_TOKEN = "Z3VHCB2ABXASKFTXGX5TKHWCCC5PIRJ3";

// --- CONFIGURAÇÃO DOS PINOS I2S ---
#define I2S_BCLK_PIN      1 // SCK no microfone
#define I2S_LRC_PIN       2 // WS no microfone
#define I2S_DATA_IN_PIN   3 // SD no microfone

// --- CONFIGURAÇÃO DO ÁUDIO ---
#define I2S_PORT              I2S_NUM_0
#define SAMPLE_RATE           16000
#define RECORD_DURATION_SEC   5
// ALTERAÇÃO CRÍTICA: Lemos em 32 bits para capturar os dados completos do microfone
#define BITS_PER_SAMPLE       I2S_BITS_PER_SAMPLE_32BIT 

const int audioBufferSize = (SAMPLE_RATE * sizeof(int16_t)) * RECORD_DURATION_SEC;
int16_t* audioBuffer = nullptr;
int audio_buffer_index = 0;

// --- FUNÇÃO DE GRAVAÇÃO (CORRIGIDA) ---
void recordAudio() {
    Serial.printf("Iniciando gravação de %d segundos...\n", RECORD_DURATION_SEC);
    if (!audioBuffer) {
        Serial.println("ERRO CRÍTICO: Buffer de áudio não está alocado!");
        return;
    }
    audio_buffer_index = 0;
    size_t bytes_read = 0;
    
    // Buffer temporário para ler os dados brutos de 32 bits
    int32_t* temp_buffer_32bit = (int32_t*) malloc(1024 * sizeof(int32_t));
    unsigned long startTime = millis();

    while (millis() - startTime < (RECORD_DURATION_SEC * 1000)) {
        i2s_read(I2S_PORT, temp_buffer_32bit, 1024 * sizeof(int32_t), &bytes_read, portMAX_DELAY);
        
        if (bytes_read == 0) Serial.print("!");
        else Serial.print(".");

        if (bytes_read > 0) {
            int samples_read = bytes_read / sizeof(int32_t);
            for (int i = 0; i < samples_read; i++) {
                if (audio_buffer_index < (audioBufferSize / sizeof(int16_t))) {
                    // **A MÁGICA ACONTECE AQUI**
                    // Desloca os bits para a direita para extrair os dados de áudio válidos
                    // e converter de 32 para 16 bits. Um shift de 14 também dá um leve ganho.
                    audioBuffer[audio_buffer_index] = (int16_t)(temp_buffer_32bit[i] >> 14);
                    audio_buffer_index++;
                }
            }
        }
    }
    Serial.println();
    free(temp_buffer_32bit);
    Serial.println("Gravação finalizada.");
}


// --- FUNÇÃO DE TRANSCRIÇÃO (sem alterações) ---
String transcribeAudio() {
    if (!audioBuffer || audio_buffer_index == 0) return "Erro: Buffer de áudio vazio ou gravação falhou.";
    Serial.println("Enviando áudio para a API da Wit.ai...");
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, "https://api.wit.ai/speech");
    http.addHeader("Authorization", "Bearer " + WIT_AI_SERVER_TOKEN);
    http.addHeader("Content-Type", "audio/raw;encoding=signed-integer;bits=16;rate=" + String(SAMPLE_RATE) + ";endian=little");
    int httpResponseCode = http.POST((uint8_t*)audioBuffer, audio_buffer_index * sizeof(int16_t));
    String response = "";
    if (httpResponseCode > 0) {
        Serial.printf("Código de resposta HTTP: %d\n", httpResponseCode);
        response = http.getString();
    } else {
        Serial.printf("Falha na requisição HTTP: %s\n", http.errorToString(httpResponseCode).c_str());
        http.end();
        return "Erro de conexão com a API.";
    }
    http.end();
    Serial.println("--- Resposta Completa da API ---");
    Serial.println(response);
    Serial.println("--------------------------------");
    int lastIndex = response.lastIndexOf("\"text\"");
    if (lastIndex != -1) {
        int startIndex = response.indexOf('"', lastIndex + 7);
        int endIndex = response.indexOf('"', startIndex + 1);
        if (startIndex != -1 && endIndex != -1) {
            return response.substring(startIndex + 1, endIndex);
        }
    }
    return "Nenhuma transcrição encontrada na resposta.";
}

// --- FUNÇÃO DE CONFIGURAÇÃO DO I2S (CORRIGIDA) ---
void setupI2S() {
    Serial.println("Configurando I2S em MONO com leitura de 32 bits...");
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = BITS_PER_SAMPLE, // Alterado para 32BIT
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRC_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_DATA_IN_PIN
    };
    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);
    Serial.println("Driver I2S configurado com sucesso.");
}

// --- SETUP PRINCIPAL ---
void setup() {
    Serial.begin(115200);
    Serial.println("\n\nIniciando Transcritor de Voz (Versão Final Corrigida)...");
    Serial.print("Conectando a ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi conectado!");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());
    setupI2S();
    Serial.println("Alocando buffer de áudio na PSRAM...");
    audioBuffer = (int16_t*) ps_malloc(audioBufferSize);
    if (!audioBuffer) {
        Serial.println("ERRO FATAL: Falha ao alocar buffer de áudio no setup!");
        while (1);
    } else {
        Serial.println("Buffer de áudio alocado com sucesso.");
    }
}

// --- LOOP PRINCIPAL ---
void loop() {
    Serial.println("\n==================================");
    Serial.println("A gravação começará em 3 segundos. Fale por 5 segundos.");
    delay(3000);
    
    recordAudio();

    if (WiFi.status() == WL_CONNECTED) {
        String transcript = transcribeAudio();
        Serial.println("\n--- TEXTO TRANSCRITO (Wit.ai) ---");
        Serial.println(transcript);
        Serial.println("-----------------------------------");
    } else {
        Serial.println("Erro: Wi-Fi desconectado.");
    }
    
    Serial.println("Aguardando 10 segundos para o próximo ciclo...");
    delay(10000);
}
