#include <Arduino.h>
#include <Wire.h>

// Definición de pines I2C seguros para ESP32-CAM
constexpr uint8_t SDA_PIN = 13;
constexpr uint8_t SCL_PIN = 14;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("\n==========================================");
  Serial.println("  SCANNER I2C PARA ESP32-CAM (ROBÓTICA)   ");
  Serial.println("==========================================");
  Serial.printf("Iniciando bus I2C en SDA: GPIO %d, SCL: GPIO %d\n", SDA_PIN, SCL_PIN);

  Wire.begin(SDA_PIN, SCL_PIN, 100000); // 100 kHz estándar
}

void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println("\nEscaneando direcciones I2C (0x01 a 0x7F)...");

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("-> Dispositivo detectado en dirección 0x%02X", address);
      if (address == 0x40) {
        Serial.print("  [*** DRIVER PWM PCA9685 CONFIRMADO ***]");
      } else if (address == 0x70) {
        Serial.print("  [Dirección All-Call PCA9685]");
      }
      Serial.println();
      nDevices++;
    } else if (error == 4) {
      Serial.printf("-> Error desconocido en dirección 0x%02X\n", address);
    }
  }

  if (nDevices == 0) {
    Serial.println("No se detectó ningún dispositivo I2C.");
    Serial.println("VERIFICAR: 1. Cables SDA/SCL cruzados. 2. GND común conectado. 3. Alimentación del PCA9685.");
  } else {
    Serial.printf("Escaneo completado: %d dispositivo(s) encontrado(s).\n", nDevices);
  }

  delay(4000); // Repetir cada 4 segundos
}
