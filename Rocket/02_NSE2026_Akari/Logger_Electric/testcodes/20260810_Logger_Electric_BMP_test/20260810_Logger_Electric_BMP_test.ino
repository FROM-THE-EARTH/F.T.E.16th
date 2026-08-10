#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

#define VSPI_SCLK 18
#define VSPI_MISO 19
#define VSPI_MOSI 23
#define BMP_CS    26
#define MPU_CS    5

SPIClass vspi(VSPI);
Adafruit_BMP280 bmp(BMP_CS, &vspi);

void setup() {
  pinMode(BMP_CS, OUTPUT);
  pinMode(MPU_CS, OUTPUT);
  digitalWrite(BMP_CS, HIGH);
  digitalWrite(MPU_CS, HIGH); // 他デバイスの干渉防止

  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== BMP280 100Hz Test ===");

  vspi.begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI);

  if (!bmp.begin()) {
    Serial.println("[NG] BMP280 Init Fail");
    while (1);
  }

  // 高速測定設定
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X1,
                  Adafruit_BMP280::SAMPLING_X1,
                  Adafruit_BMP280::FILTER_OFF,
                  Adafruit_BMP280::STANDBY_MS_1);
}

void loop() {
  static uint32_t nextTargetUs = micros();
  static uint32_t sampleCount = 0;

  while ((int32_t)(nextTargetUs - micros()) > 0);
  uint32_t now = micros();

  float temp = bmp.readTemperature();
  float pres = bmp.readPressure() / 100.0f;

  // 100回（1秒）に1回シリアル表示
  if (++sampleCount >= 100) {
    sampleCount = 0;
    Serial.printf("Temp: %.2f degC | Pres: %.2f hPa\n", temp, pres);
  }

  nextTargetUs += 10000; // 10ms (100Hz)
  if (now > nextTargetUs + 50000) nextTargetUs = now + 10000;
}