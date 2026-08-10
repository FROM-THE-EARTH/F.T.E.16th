#include <SPI.h>

#define VSPI_SCLK 18
#define VSPI_MISO 19
#define VSPI_MOSI 23
#define BMP_CS    26
#define MPU_CS    5

#define MPU_PWR_MGMT_1   0x6B
#define MPU_ACCEL_CONFIG 0x1C
#define MPU_GYRO_CONFIG  0x1B
#define MPU_ACCEL_XOUT_H 0x3B

SPIClass vspi(VSPI);

void writeMPURegister(uint8_t reg, uint8_t data) {
  vspi.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(MPU_CS, LOW);
  vspi.transfer(reg);
  vspi.transfer(data);
  digitalWrite(MPU_CS, HIGH);
  vspi.endTransaction();
}

void setup() {
  pinMode(BMP_CS, OUTPUT);
  pinMode(MPU_CS, OUTPUT);
  digitalWrite(BMP_CS, HIGH); // 他デバイスの干渉防止
  digitalWrite(MPU_CS, HIGH);

  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== MPU6500 1kHz Test ===");

  vspi.begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI);

  // MPU6500 初期化
  writeMPURegister(MPU_PWR_MGMT_1, 0x80); delay(100);
  writeMPURegister(MPU_PWR_MGMT_1, 0x01); delay(10);
  writeMPURegister(MPU_ACCEL_CONFIG, 0x18);
  writeMPURegister(MPU_GYRO_CONFIG, 0x18);
}

void loop() {
  static uint32_t nextTargetUs = micros();
  static uint32_t sampleCount = 0;

  while ((int32_t)(nextTargetUs - micros()) > 0);
  uint32_t now = micros();

  // 14バイト一括読み出し
  vspi.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(MPU_CS, LOW);
  vspi.transfer(MPU_ACCEL_XOUT_H | 0x80); 
  
  int16_t ax_raw = (vspi.transfer(0x00) << 8) | vspi.transfer(0x00);
  int16_t ay_raw = (vspi.transfer(0x00) << 8) | vspi.transfer(0x00);
  int16_t az_raw = (vspi.transfer(0x00) << 8) | vspi.transfer(0x00);
  vspi.transfer(0x00); vspi.transfer(0x00); // Temp空読み
  int16_t gx_raw = (vspi.transfer(0x00) << 8) | vspi.transfer(0x00);
  int16_t gy_raw = (vspi.transfer(0x00) << 8) | vspi.transfer(0x00);
  int16_t gz_raw = (vspi.transfer(0x00) << 8) | vspi.transfer(0x00);
  
  digitalWrite(MPU_CS, HIGH);
  vspi.endTransaction();

  // 1000回（1秒）に1回だけシリアル表示
  if (++sampleCount >= 1000) {
    sampleCount = 0;
    Serial.printf("ACC[G]: X=%.2f, Y=%.2f, Z=%.2f | GYRO[dps]: X=%.2f, Y=%.2f, Z=%.2f\n",
                  ax_raw / 2048.0f, ay_raw / 2048.0f, az_raw / 2048.0f,
                  gx_raw / 16.4f, gy_raw / 16.4f, gz_raw / 16.4f);
  }

  nextTargetUs += 1000;
  if (now > nextTargetUs + 5000) nextTargetUs = now + 1000;
}