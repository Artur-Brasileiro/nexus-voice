/*
 * Título: Assistente de Voz TCC (Execução Única)
 * Autor: Artur (UEMG) & Gemini
 * Descrição: Versão Final. Este código executa o ciclo completo do assistente
 * de voz (ouvir, transcrever, falar) uma única vez e depois para.
 * É uma versão estável e robusta, ideal para demonstração e como
 * base para futuras expansões. A função loop() foi intencionalmente
 * desativada após a primeira execução.
 */

// --- INCLUDES ---
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "driver/i2s.h"
#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"


// --- CONFIGURAÇÃO DO USUÁRIO ---
const char* WIFI_SSID = "Apto-26";
const char* WIFI_PASSWORD = "apto26px26";
String WIT_AI_SERVER_TOKEN = "Z3VHCB2ABXASKFTXGX5TKHWCCC5PIRJ3";


// --- CONFIGURAÇÃO DOS PINOS I2S ---
#define I2S_BCLK_PIN      1
#define I2S_LRC_PIN       2
#define I2S_DATA_IN_PIN   3
#define I2S_DATA_OUT_PIN  4


// --- CONFIGURAÇÃO DO ÁUDIO ---
#define I2S_PORT              I2S_NUM_0
#define SAMPLE_RATE           16000
#define RECORD_DURATION_SEC   5
#define BITS_PER_SAMPLE_MIC   I2S_BITS_PER_SAMPLE_32BIT

const int audioBufferSize = (SAMPLE_RATE * sizeof(int16_t)) * RECORD_DURATION_SEC;
int16_t* audioBuffer = nullptr;
int audio_buffer_index = 0;

// --- OBJETOS GLOBAIS PARA SAÍDA DE ÁUDIO ---
AudioGeneratorMP3 *mp3;
AudioFileSourceHTTPStream *file;
AudioFileSourceBuffer *buff;
AudioOutputI2S *out;


// --- FUNÇÕES DE ÁUDIO ---

void setupI2SMicrophone() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = BITS_PER_SAMPLE_MIC,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRC_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_DATA_IN_PIN
    };
    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);
    i2s_zero_dma_buffer(I2S_PORT);
}

void recordAudio() {
    if (!audioBuffer) return;
    audio_buffer_index = 0;
    size_t bytes_read = 0;
    int32_t* temp_buffer_32bit = (int32_t*) malloc(1024 * sizeof(int32_t));
    unsigned long startTime = millis();
    while (millis() - startTime < (RECORD_DURATION_SEC * 1000)) {
        i2s_read(I2S_PORT, temp_buffer_32bit, 1024 * sizeof(int32_t), &bytes_read, pdMS_TO_TICKS(100));
        if (bytes_read > 0) {
            int samples_read = bytes_read / sizeof(int32_t);
            for (int i = 0; i < samples_read; i++) {
                if (audio_buffer_index < (audioBufferSize / sizeof(int16_t))) {
                    audioBuffer[audio_buffer_index] = (int16_t)(temp_buffer_32bit[i] >> 14);
                    audio_buffer_index++;
                }
            }
        }
    }
    free(temp_buffer_32bit);
}

String transcribeAudio() {
    if (!audioBuffer || audio_buffer_index == 0) return "";
    HTTPClient http;
    http.begin("https://api.wit.ai/speech");
    http.addHeader("Authorization", "Bearer " + WIT_AI_SERVER_TOKEN);
    http.addHeader("Content-Type", "audio/raw;encoding=signed-integer;bits=16;rate=" + String(SAMPLE_RATE) + ";endian=little");
    int httpResponseCode = http.POST((uint8_t*)audioBuffer, audio_buffer_index * sizeof(int16_t));
    String response = "";
    if (httpResponseCode == 200) {
        response = http.getString();
        int lastIndex = response.lastIndexOf("\"text\"");
        if (lastIndex != -1) {
            int startIndex = response.indexOf('"', lastIndex + 7);
            int endIndex = response.indexOf('"', startIndex + 1);
            if (startIndex != -1 && endIndex != -1) {
                return response.substring(startIndex + 1, endIndex);
            }
        }
    }
    http.end();
    return "";
}

String urlEncode(String str) {
    String encodedString = "";
    char c; char code0; char code1;
    for (int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (c == ' ') { encodedString += "%20"; }
        else if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') { encodedString += c; }
        else {
            encodedString += '%';
            code1 = (c & 0X0F) + '0'; if ((c & 0X0F) > 9) code1 = (c & 0X0F) - 10 + 'A';
            c = (c >> 4) & 0X0F;
            code0 = c + '0'; if (c > 9) code0 = c - 10 + 'A';
            encodedString += code0; encodedString += code1;
        }
    }
    return encodedString;
}

void speak(String textToSpeak) {
  if (textToSpeak == "") return;
  
  i2s_driver_uninstall(I2S_PORT);

  file = new AudioFileSourceHTTPStream();
  String encodedText = urlEncode(textToSpeak);
  String url = "http://translate.google.com/translate_tts?ie=UTF-8&q=" + encodedText + "&tl=pt-BR&client=tw-ob";

  if (file->open(url.c_str())) {
      buff = new AudioFileSourceBuffer(file, 2048);
      out = new AudioOutputI2S();
      out->SetPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DATA_OUT_PIN);
      mp3 = new AudioGeneratorMP3();
      mp3->begin(buff, out);
      while (mp3->isRunning()) {
          if (!mp3->loop()) {
              mp3->stop();
          }
      }
      out->stop();
      file->close(); 
      delete mp3; delete buff; delete file; delete out;
      i2s_driver_uninstall(I2S_PORT);
  } else {
    delete file;
  }
}


// --- SETUP E LOOP PRINCIPAL ---
void setup() {
    Serial.begin(115220);
    Serial.println("\n\nAssistente de Voz TCC - Execução Única");

    Serial.print("Conectando ao Wi-Fi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500); Serial.print(".");
    }
    Serial.println("\nWi-Fi conectado!");

    audioBuffer = (int16_t*) ps_malloc(audioBufferSize);
    if (!audioBuffer) {
        Serial.println("ERRO FATAL: Falha ao alocar buffer de áudio!");
        while (1);
    }
    
    // Configura o microfone para o ciclo único
    setupI2SMicrophone();
    Serial.println("Assistente pronto. Iniciando ciclo...");

    // --- EXECUÇÃO ÚNICA ---
    Serial.println("\n==================================");
    Serial.println("Ouvindo...");
    recordAudio();
    String userText = transcribeAudio();
    if (userText.length() > 0) {
        Serial.printf("Você disse: \"%s\"\n", userText.c_str());
        String responseText = "Entendi. Você disse: " + userText;
        Serial.printf("Assistente: \"%s\"\n", responseText.c_str());
        speak(responseText);
    }

    Serial.println("\n==================================");
    Serial.println("Ciclo finalizado. O sistema será pausado.");
}

void loop() {
  // O loop está intencionalmente vazio.
  // O processador ficará aqui após o setup completar.
  delay(10000);
}
