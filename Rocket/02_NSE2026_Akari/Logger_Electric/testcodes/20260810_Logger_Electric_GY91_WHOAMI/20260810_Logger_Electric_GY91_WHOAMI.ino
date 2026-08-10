#include <SPI.h>

#define VSPI_SCLK 18
#define VSPI_MISO 19
#define VSPI_MOSI 23
#define BMP_CS    26
#define MPU_CS    5

SPIClass vspi(VSPI);

void setup() {
  pinMode(BMP_CS, OUTPUT);
  pinMode(MPU_CS, OUTPUT);
  digitalWrite(BMP_CS, HIGH);
  digitalWrite(MPU_CS, HIGH);

  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== Raw SPI Hardware Debugger ===");

  vspi.begin(VSPI_SCLK, VSPI_MISO, VSPI_MOSI);
}

// 指定した速度で特定のレジスタを読み取る直接通信関数
uint8_t readSPI(uint8_t cs_pin, uint8_t reg_addr, uint32_t speed) {
  vspi.beginTransaction(SPISettings(speed, MSBFIRST, SPI_MODE0));
  digitalWrite(cs_pin, LOW);
  vspi.transfer(reg_addr | 0x80); // 0x80は読み取り要求コマンド
  uint8_t response = vspi.transfer(0x00);
  digitalWrite(cs_pin, HIGH);
  vspi.endTransaction();
  return response;
}

void loop() {
  Serial.println("--- Reading WHO_AM_I Registers ---");

  // 1. MPU6500の生存確認 (レジスタ 0x75)
  // 高速(1MHz)と低速(100kHz)で応答が変わるかチェック
  uint8_t mpu_fast = readSPI(MPU_CS, 0x75, 1000000);
  uint8_t mpu_slow = readSPI(MPU_CS, 0x75, 100000);
  // 期待値を0x70に変更
  Serial.printf("MPU6500 [1MHz]: 0x%02X  |  [100kHz]: 0x%02X  (Expected: 0x70)\n", mpu_fast, mpu_slow);

  // 2. BMP280の生存確認 (レジスタ 0xD0)
  uint8_t bmp_fast = readSPI(BMP_CS, 0xD0, 1000000);
  uint8_t bmp_slow = readSPI(BMP_CS, 0xD0, 100000);
  Serial.printf("BMP280  [1MHz]: 0x%02X  |  [100kHz]: 0x%02X  (Expected: 0x58)\n\n", bmp_fast, bmp_slow);

  delay(2000);
}