#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include "FS.h"
#include "SD_MMC.h"

// --- ピンアサイン ---
#define VSPI_SCLK 18
#define VSPI_MISO 19
#define VSPI_MOSI 23
#define BMP_CS    26
#define MPU_CS    5

#define LED1 32
#define LED2 33

// --- MPU6500 レジスタアドレス ---
#define MPU_PWR_MGMT_1   0x6B
#define MPU_ACCEL_CONFIG 0x1C
#define MPU_GYRO_CONFIG  0x1B
#define MPU_ACCEL_XOUT_H 0x3B

// 保存するデータ構造 (36バイト: L + 8f)
struct DataPack {
  uint32_t t_us;
  float ax, ay, az;
  float gx, gy, gz;
  float temp, pres;
};

QueueHandle_t dataQueue;
SPIClass vspi(VSPI);
Adafruit_BMP280 bmp(BMP_CS, &vspi);
File logFile;
char logFileName[32]; // 動的に決定するファイル名を格納するバッファ

// 指定したレジスタにデータを書き込む関数
void writeMPURegister(uint8_t reg, uint8_t data) {
  vspi.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(MPU_CS, LOW);
  vspi.transfer(reg);
  vspi.transfer(data);
  digitalWrite(MPU_CS, HIGH);
  vspi.endTransaction();
}

// --- Core 0: SD書き込み専用タスク ---
void core0Task(void *pvParameters) {
  DataPack received;
  uint32_t count = 0;
  for (;;) {
    // キューにデータが来るまで待機
    if (xQueueReceive(dataQueue, &received, portMAX_DELAY) == pdPASS) {
      if (logFile) {
        logFile.write((const uint8_t*)&received, sizeof(DataPack));
        // 512行ごとに強制的にSDカードへ物理書き出し
        if (++count >= 512) {
          logFile.flush();
          count = 0;
        }
      }
    }
  }
}

void setup() {
  pinMode(BMP_CS, OUTPUT);
  pinMode(MPU_CS, OUTPUT);
  digitalWrite(BMP_CS, HIGH);
  digitalWrite(MPU_CS, HIGH);
  
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);

  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== MPU6500(1kHz) & BMP280(100Hz Max Speed) Logging Start ===");

  vspi.begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI);

  // --- BMP280 初期化（最高速度設定: 約140Hz測定） ---
  if (!bmp.begin()) {
    Serial.println("[NG] BMP280 Fail");
    while(1) { digitalWrite(LED1, HIGH); delay(100); digitalWrite(LED1, LOW); delay(100); }
  }
  // オーバーサンプリングX1、フィルタOFF、待機0.5ms にして測定時間を最速(約6.5ms)に縮小
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X1,
                  Adafruit_BMP280::SAMPLING_X1,
                  Adafruit_BMP280::FILTER_OFF,
                  Adafruit_BMP280::STANDBY_MS_1);

  // --- MPU6500 初期化 (Raw SPI) ---
  writeMPURegister(MPU_PWR_MGMT_1, 0x80); // デバイスリセット
  delay(100);
  writeMPURegister(MPU_PWR_MGMT_1, 0x01); // クロック源を自動選択
  delay(10);
  writeMPURegister(MPU_ACCEL_CONFIG, 0x18); // 加速度: +/- 16G
  writeMPURegister(MPU_GYRO_CONFIG, 0x18);  // ジャイロ: +/- 2000 deg/s

  // --- SDMMC (4bitモード) 初期化 ---
  if (!SD_MMC.begin("/sdcard", false)) {
    Serial.println("[NG] SD Card Fail");
    while(1) { digitalWrite(LED1, HIGH); delay(100); digitalWrite(LED1, LOW); delay(100); }
  } else {
    // SDカード内のファイルを検索し、まだ存在しない最小の番号 (data_000.bin, data_001.bin ...) を探す
    int fileIndex = 0;
    while (fileIndex < 1000) {
      sprintf(logFileName, "/data_%03d.bin", fileIndex);
      if (!SD_MMC.exists(logFileName)) {
        break;
      }
      fileIndex++;
    }

    logFile = SD_MMC.open(logFileName, FILE_WRITE);
    if(logFile) {
      Serial.print("[OK] SD Ready. Writing to: ");
      Serial.println(logFileName);
    } else {
      Serial.println("[NG] File Open Fail");
      while(1) { digitalWrite(LED1, HIGH); delay(100); digitalWrite(LED1, LOW); delay(100); }
    }
  }

  // キューとタスク作成
  dataQueue = xQueueCreate(1024, sizeof(DataPack));
  xTaskCreatePinnedToCore(core0Task, "core0Task", 16384, NULL, 1, NULL, 0);
  
  digitalWrite(LED1, HIGH);
  Serial.println(">>> 1kHz Logging Started <<<");
}

void loop() {
  static uint32_t nextTargetUs = micros();
  static uint32_t lastBMP = 0;
  static uint32_t lastLED = 0;
  static float cT = 0, cP = 0;

  // 1msの厳密な同期ループ
  while ((int32_t)(nextTargetUs - micros()) > 0);
  uint32_t now = micros();

  // --- BMP280 高速読み出し (100Hz / 10ms間隔) ---
  if (now - lastBMP >= 10000) {
    lastBMP = now;
    cT = bmp.readTemperature();
    cP = bmp.readPressure() / 100.0f;
  }

  // --- MPU6500 14バイト一括読み出し (1MHz SPI) ---
  vspi.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(MPU_CS, LOW);
  vspi.transfer(MPU_ACCEL_XOUT_H | 0x80); 
  
  int16_t ax_raw = (vspi.transfer(0x00) << 8) | vspi.transfer(0x00);
  int16_t ay_raw = (vspi.transfer(0x00) << 8) | vspi.transfer(0x00);
  int16_t az_raw = (vspi.transfer(0x00) << 8) | vspi.transfer(0x00);
  
  vspi.transfer(0x00); 
  vspi.transfer(0x00); 
  
  int16_t gx_raw = (vspi.transfer(0x00) << 8) | vspi.transfer(0x00);
  int16_t gy_raw = (vspi.transfer(0x00) << 8) | vspi.transfer(0x00);
  int16_t gz_raw = (vspi.transfer(0x00) << 8) | vspi.transfer(0x00);
  
  digitalWrite(MPU_CS, HIGH);
  vspi.endTransaction();

  // データパック作成と物理単位への変換
  DataPack data = {
    now,
    ax_raw / 2048.0f, ay_raw / 2048.0f, az_raw / 2048.0f,
    gx_raw / 16.4f, gy_raw / 16.4f, gz_raw / 16.4f,
    cT, cP
  };

  // コア0のSD保存用キューに送信
  if (xQueueSend(dataQueue, &data, 0) != pdPASS) {
    // キュー詰まりは無視してループ継続（データ落ちを許容してフリーズを防ぐ）
  }

  // LED2の生存確認（ハートビート）: 1秒に1回光る
  if (now - lastLED >= 1000000) {
    lastLED = now;
    digitalWrite(LED2, HIGH);
  } else if (now - lastLED >= 50000) {
    digitalWrite(LED2, LOW);
  }

  // 次の1msターゲットタイムを計算
  nextTargetUs += 1000;
  
  // もし処理落ちで遅延が蓄積しすぎた場合は現在時刻でリセット
  if (now > nextTargetUs + 5000) nextTargetUs = now + 1000;
}