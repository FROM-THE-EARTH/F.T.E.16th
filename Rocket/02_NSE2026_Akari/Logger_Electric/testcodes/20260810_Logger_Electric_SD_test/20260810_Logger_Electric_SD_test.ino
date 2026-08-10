#include "FS.h"
#include "SD_MMC.h"

#define LED1 32

struct DataPack {
  uint32_t t_us;
  float ax, ay, az;
  float gx, gy, gz;
  float temp, pres;
};

QueueHandle_t dataQueue;
File logFile;
char logFileName[32];

void core0Task(void *pvParameters) {
  DataPack received;
  uint32_t count = 0;
  for (;;) {
    if (xQueueReceive(dataQueue, &received, portMAX_DELAY) == pdPASS) {
      if (logFile) {
        logFile.write((const uint8_t*)&received, sizeof(DataPack));
        if (++count >= 512) {
          logFile.flush();
          count = 0;
        }
      }
    }
  }
}

void setup() {
  pinMode(LED1, OUTPUT);
  digitalWrite(LED1, LOW);

  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== SD_MMC Dummy Write 1kHz Test ===");

  if (!SD_MMC.begin("/sdcard", false)) {
    Serial.println("[NG] SD Card Fail");
    while (1);
  }

  int fileIndex = 0;
  while (fileIndex < 1000) {
    sprintf(logFileName, "/dummy_%03d.bin", fileIndex);
    if (!SD_MMC.exists(logFileName)) break;
    fileIndex++;
  }

  logFile = SD_MMC.open(logFileName, FILE_WRITE);
  if (logFile) {
    Serial.printf("[OK] Writing Dummy Data to: %s\n", logFileName);
  } else {
    Serial.println("[NG] File Open Fail");
    while (1);
  }

  dataQueue = xQueueCreate(1024, sizeof(DataPack));
  xTaskCreatePinnedToCore(core0Task, "core0Task", 16384, NULL, 1, NULL, 0);

  digitalWrite(LED1, HIGH);
}

void loop() {
  static uint32_t nextTargetUs = micros();
  static uint32_t totalWritten = 0;
  static uint32_t lastReport = 0;

  while ((int32_t)(nextTargetUs - micros()) > 0);
  uint32_t now = micros();

  // ダミーデータ生成
  DataPack dummyData = {
    now,
    1.0f, 2.0f, 3.0f,  // ax, ay, az
    0.1f, 0.2f, 0.3f,  // gx, gy, gz
    25.0f, 1013.25f    // temp, pres
  };

  if (xQueueSend(dataQueue, &dummyData, 0) == pdPASS) {
    totalWritten++;
  }

  // 1秒ごとに書き込み総数を表示
  if (now - lastReport >= 1000000) {
    lastReport = now;
    Serial.printf("Dummy Logs Written: %u samples (~%.1f KB)\n", 
                  totalWritten, (totalWritten * sizeof(DataPack)) / 1024.0f);
  }

  nextTargetUs += 1000; // 1ms (1kHz)
  if (now > nextTargetUs + 5000) nextTargetUs = now + 1000;
}