#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <rom/crc.h>

#define FW_VERSION "FW-V:1.0.5"

const char* firmwareUrl = "http://192.168.1.100/firmware.bin";
const char* versionUrl = "http://192.168.1.100/version";
const char* ssid = "CLEUDO";
const char* password = "91898487";

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

void checkForUpdate() {
  WiFiClient client;
  HTTPClient http;

  if (http.begin(client, firmwareUrl)) {
    int httpCode = http.GET(); // Faz a requisição HTTP GET

    if (httpCode == HTTP_CODE_OK) { // Verifica se a resposta foi 200 OK
      int contentLength = http.getSize();
      // 1. Iniciar a Atualização OTA
      // O 'contentLength' ajuda a verificar se o arquivo cabe.
      // O tipo 'U_FLASH' indica uma atualização de firmware da aplicação.
      if (Update.begin(contentLength, U_FLASH)) {
        Serial.println("OTA iniciado. Gravando dados...");

        // 2. Gravar os Dados (Byte a Byte ou em Chunks)
        // O cliente HTTP atua como a fonte de dados.
        size_t written = Update.writeStream(client);

        if (written == contentLength) {
          Serial.println("Dados OTA gravados com sucesso.");
        } else {
          Serial.printf("Erro de escrita. Escrito: %zu de %d\n", written, contentLength);
        }

        // 3. Finalizar e Verificar a Atualização
        if (Update.end()) {
          if (Update.isFinished()) {
            Serial.println("OTA concluída. Reiniciando...");
            // 4. Reiniciar o ESP32
            ESP.restart(); // O bootloader lerá o ota_data e carregará o novo firmware
          } else {
            Serial.println("OTA finalizada, mas não foi marcada como concluída.");
          }
        } else {
          Serial.printf("Erro ao finalizar a OTA: %s\n", Update.getError());
        }
      } else {
        Serial.printf("Falha ao iniciar a OTA: %s\n", Update.getError());
      }
    } else {
      Serial.printf("HTTP GET falhou, código: %d\n", httpCode);
    }
    http.end();
  } else {
    Serial.println("Falha ao iniciar a conexão HTTP.");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Mostrar a partição de execução
  const esp_partition_t *runningPartition = esp_ota_get_running_partition();
  if (runningPartition) {
    Serial.printf("\n*** Rodando na Partição: %s ***\n", runningPartition->label);
  }

  // Conectar ao WiFi
  Serial.printf("Conectando-se à rede WiFi: %s\n", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado ao WiFi!");
}

void loop() {
  String remoteVersion = getRemoteVersion();

  if (strstr(FW_VERSION, remoteVersion.c_str()) != NULL) {
    Serial.printf("Firmware atualizado, versão disponível na nuvem: %s\n", remoteVersion.c_str());
    delay(5000);
    return;
  }

  Serial.printf("Nova versão disponível: %s (local: %s)\n", remoteVersion.c_str(), FW_VERSION);
  checkForUpdate();
  delay(5000);
}