/*
 * ============================================================================
 *  servos.h  -  Driver mínimo para PCA9685 (sin librerías externas)
 * ============================================================================
 *  Controla hasta 16 canales PWM por I2C. Pensado para los 6 servomotores
 *  del brazo robótico.
 *
 *  Conexión:
 *    ESP32-CAM GPIO13 (SDA)  ->  PCA9685 SDA
 *    ESP32-CAM GPIO14 (SCL)  ->  PCA9685 SCL
 *    ESP32-CAM GND           ->  PCA9685 GND
 *    Fuente externa 5V       ->  PCA9685 VCC (borne)
 *
 *  IMPORTANTE: los servos NO se alimentan del ESP32-CAM.
 * ============================================================================
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>

// --- Registros del PCA9685 ---------------------------------------------------
#define PCA9685_MODE1      0x00
#define PCA9685_PRESCALE   0xFE
#define PCA9685_LED0_ON_L  0x06

class PCA9685 {
 public:
  explicit PCA9685(uint8_t address = 0x40) : _addr(address) {}

  // Devuelve true si el chip responde en el bus I2C
  bool begin() {
    Wire.beginTransmission(_addr);
    if (Wire.endTransmission() != 0) return false;

    write8(PCA9685_MODE1, 0x00);   // modo normal
    delay(5);
    return true;
  }

  // Frecuencia PWM. Para servomotores analógicos: 50 Hz
  void setPWMFreq(float freq) {
    if (freq < 24.0f)    freq = 24.0f;
    if (freq > 1526.0f)  freq = 1526.0f;

    float prescaleval = 25000000.0f;   // reloj interno 25 MHz
    prescaleval /= 4096.0f;            // 12 bits
    prescaleval /= freq;
    prescaleval -= 1.0f;

    uint8_t prescale = (uint8_t)(prescaleval + 0.5f);

    uint8_t oldmode = read8(PCA9685_MODE1);
    uint8_t newmode = (oldmode & 0x7F) | 0x10;   // sleep
    write8(PCA9685_MODE1, newmode);
    write8(PCA9685_PRESCALE, prescale);
    write8(PCA9685_MODE1, oldmode);
    delay(5);
    write8(PCA9685_MODE1, oldmode | 0x80);       // restart
  }

  // on/off en cuentas de 0..4095 (4096 = apagado total)
  void setPWM(uint8_t channel, uint16_t on, uint16_t off) {
    Wire.beginTransmission(_addr);
    Wire.write((uint8_t)(PCA9685_LED0_ON_L + 4 * channel));
    Wire.write((uint8_t)(on & 0xFF));
    Wire.write((uint8_t)(on >> 8));
    Wire.write((uint8_t)(off & 0xFF));
    Wire.write((uint8_t)(off >> 8));
    Wire.endTransmission();
  }

  // Pulso en microsegundos. A 50 Hz el periodo es 20000 us -> 4096 cuentas
  void writeMicroseconds(uint8_t channel, uint16_t us) {
    uint32_t off = ((uint32_t)us * 4096UL) / 20000UL;
    if (off > 4095UL) off = 4095UL;
    setPWM(channel, 0, (uint16_t)off);
  }

  // Sin pulso: el servo deja de recibir señal
  void fullOff(uint8_t channel) { setPWM(channel, 0, 4096); }

 private:
  uint8_t _addr;

  void write8(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
  }

  uint8_t read8(uint8_t reg) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(_addr, (uint8_t)1);
    return Wire.available() ? (uint8_t)Wire.read() : 0;
  }
};
