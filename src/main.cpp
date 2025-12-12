#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <rom/crc.h>

#define FW_VERSION "FW-V:2.0.9"

const char* firmwareUrl = "http://192.168.1.100/firmware.bin";
const char* versionUrl = "http://192.168.1.100/version";
const char* ssid = "CLEUDO";
const char* password = "91898487";

// ------- FLAG GLOBAL PARA PAUSAR A APLICAÇÃO DURANTE OTA -------
volatile bool otaInProgress = false;
volatile bool canStart = false;

// --------- TASK HANDLES ----------
TaskHandle_t otaTaskHandle;
TaskHandle_t appTaskHandle;

// Função para obter a versão do firmware remoto
String getRemoteVersion() {
  WiFiClient client;
  HTTPClient http;

  if (!http.begin(client, versionUrl)) return "";

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) return "";

  String version = http.getString();
  http.end();
  return version;
}

// void para checar e realizar OTA
void checkForUpdate() {
  WiFiClient client;
  HTTPClient http;
  
  if (http.begin(client, firmwareUrl)) {

    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {

      int contentLength = http.getSize();

      // === SINALIZA PARA PARAR O CORE DA APLICAÇÃO ===
      otaInProgress = true;

      // Verificação dupla, só inicia a OTA quando a app liberar
      while (!canStart) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
      }

      if (Update.begin(contentLength, U_FLASH)) {
        Serial.println("OTA iniciado. Gravando...");

        size_t written = Update.writeStream(client);

        if (written != contentLength) {
          Serial.printf("Erro: escrito %zu de %d\n", written, contentLength);
        }

        if (Update.end()) {
          if (Update.isFinished()) {
            Serial.println("OTA concluída! Reiniciando...");
            vTaskDelay(200 / portTICK_PERIOD_MS);
            ESP.restart();
          } else {
            Serial.println("OTA finalizada, mas não marcada como concluída.");
          }
        } else {
          Serial.printf("Erro ao finalizar OTA: %s\n", Update.getError());
        }
      } else {
        Serial.printf("Falha ao iniciar OTA: %s\n", Update.getError());
      }

      otaInProgress = false;  // Libera o core da aplicação

    } else {
      Serial.printf("HTTP GET falhou, código: %d\n", httpCode);
    }

    http.end();
  } else {
    Serial.println("Falha ao iniciar conexão HTTP");
  }
}

// Task de OTA (CORE 0)
void otaTask(void *parameter) {

  for (;;) {
    String remoteVersion = getRemoteVersion();

    if (remoteVersion.length() > 0) {
      if (!strstr(FW_VERSION, remoteVersion.c_str())) {
        Serial.printf("[OTA] Nova versão detectada: %s (local: %s)\n",
            remoteVersion.c_str(), FW_VERSION);
        checkForUpdate();
      }
      else {
        Serial.println("[OTA] Firmware atualizado. Versão remota: " + remoteVersion);
      }
    }
    else {
      Serial.println("[OTA] Falha ao obter versão remota.");
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

// Task de aplicação (CORE 1)
void appTask(void *parameter) {

  for (;;) {

    if (otaInProgress) {
      // Mantém a task "respirando" para não ativar watchdog
      vTaskDelay(100 / portTICK_PERIOD_MS);
      canStart = true;
      continue;
    }

    // aqui vai o código real da aplicação
    Serial.println("[APP] Aplicação rodando normalmente...");
    canStart = false;
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  const esp_partition_t *runningPartition = esp_ota_get_running_partition();
  if (runningPartition) {
    Serial.printf("\n*** Rodando na Partição: %s ***\n", runningPartition->label);
  }

  Serial.printf("Conectando ao WiFi: %s\n", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");

  // Core 0 → OTA
  xTaskCreatePinnedToCore(otaTask, "OTATask", 10000, NULL, 1, &otaTaskHandle, 0);

  // Core 1 → Aplicação real
  xTaskCreatePinnedToCore(appTask, "APPTask", 12000, NULL, 1, &appTaskHandle, 1);
}

void loop() {
  // Nada aqui — tudo via tasks
}