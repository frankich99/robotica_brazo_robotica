/*
 * ============================================================================
 *  BRAZO ROBÓTICO + CÁMARA ESP32-CAM  —  Control total por Wi-Fi
 * ============================================================================
 *
 *  PLACA:   ESP32-CAM AI Thinker (OV2640)
 *  DRIVER:  PCA9685 por I2C  (SDA = GPIO13, SCL = GPIO14)
 *  SERVOS:  6 canales (0 a 5)
 *
 *  RED WiFi (modo AP / "router" propio):
 *      SSID:     BRAZO-ESP32CAM
 *      Clave:    123456789
 *      IP:       192.168.4.1
 *
 *  URLs:
 *      http://192.168.4.1/            -> interfaz web (video + servos)
 *      http://192.168.4.1:81/stream   -> video MJPEG
 *      http://192.168.4.1:81/capture  -> una foto JPEG
 *
 *  API (POST, x-www-form-urlencoded):
 *      /api/status                  -> GET  estado JSON
 *      /api/enable   i=0..5 on=0|1  -> activar/desactivar un servo
 *      /api/servo    i=0..5 angle=  -> mover un servo
 *      /api/home                    -> centrar servos activos
 *      /api/stop                    -> desactivar todos los pulsos
 *      /api/heartbeat               -> mantener vivos los servos
 *
 *  SEGURIDAD:
 *      - Si el navegador deja de enviar latido durante 10 s, se desactivan
 *        los pulsos PWM (el brazo puede caer por gravedad).
 *      - Los servos se alimentan de una fuente externa de 5 V.
 *        NUNCA del pin 5V del ESP32-CAM.
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>

// esp_http_server.h define HTTP_ANY como macro y rompe WebServer.h
#undef HTTP_ANY

#include "servos.h"
#include "camara.h"
#include "pagina.h"

// ============================================================================
//  CONFIGURACIÓN
// ============================================================================
constexpr char AP_NAME[]     = "BRAZO-ESP32CAM";
constexpr char AP_PASSWORD[] = "123456789";   // mínimo 8 caracteres

constexpr uint8_t SDA_PIN     = 13;
constexpr uint8_t SCL_PIN     = 14;
constexpr uint8_t PCA_ADDRESS = 0x40;

constexpr uint32_t LINK_TIMEOUT_MS    = 10000;  // sin latido -> apagar PWM
constexpr uint32_t MOTION_INTERVAL_MS = 20;     // velocidad de interpolación

// ============================================================================
//  ARTICULACIONES
// ============================================================================
struct Joint {
  const char *name;
  uint8_t channel;
  uint8_t minAngle;
  uint8_t maxAngle;
  uint8_t home;
  uint16_t minPulseUs;
  uint16_t maxPulseUs;
  uint8_t current;
  uint8_t target;
  bool enabled;
};

// Límites conservadores: CALIBRAR antes de montar los brazos.
Joint joints[] = {
    {"Base",     0, 30, 150, 90, 1000, 2000, 90, 90, false},
    {"Hombro",   1, 30, 150, 90, 1000, 2000, 90, 90, false},
    {"Codo",     2, 30, 150, 90, 1000, 2000, 90, 90, false},
    {"Muneca 1", 3, 30, 150, 90, 1000, 2000, 90, 90, false},
    {"Muneca 2", 4, 30, 150, 90, 1000, 2000, 90, 90, false},
    {"Pinza",    5, 30, 150, 90, 1000, 2000, 90, 90, false},
};
constexpr size_t JOINT_COUNT = sizeof(joints) / sizeof(joints[0]);

// ============================================================================
//  ESTADO GLOBAL
// ============================================================================
PCA9685 pwm(PCA_ADDRESS);
WebServer server(80);

bool driverReady = false;
uint32_t lastHeartbeat = 0;
uint32_t lastMotion = 0;

// ============================================================================
//  UTILIDADES
// ============================================================================
bool parseNumber(const String &value, int &out) {
  if (value.isEmpty() || value.length() > 3) return false;
  int number = 0;
  for (size_t i = 0; i < value.length(); ++i) {
    if (value[i] < '0' || value[i] > '9') return false;
    number = number * 10 + (value[i] - '0');
  }
  out = number;
  return true;
}

void json(int code, const String &body) {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.send(code, "application/json; charset=utf-8", body);
}

void writeAngle(Joint &joint) {
  const uint16_t pulse = map(joint.current, 0, 180,
                             joint.minPulseUs, joint.maxPulseUs);
  pwm.writeMicroseconds(joint.channel, pulse);
}

void stopAll() {
  if (!driverReady) return;
  for (Joint &joint : joints) {
    pwm.fullOff(joint.channel);   // sin pulso
    joint.enabled = false;
    joint.target = joint.current;
  }
}

bool requireDriver() {
  if (!driverReady) {
    json(503, "{\"error\":\"PCA9685 no detectado: revisar cableado\"}");
    return false;
  }
  return true;
}

bool selectedJoint(Joint *&joint) {
  int index;
  if (!server.hasArg("i") || !parseNumber(server.arg("i"), index) ||
      index < 0 || index >= static_cast<int>(JOINT_COUNT)) {
    json(400, "{\"error\":\"Indice de servo invalido\"}");
    return false;
  }
  joint = &joints[index];
  return true;
}

// ============================================================================
//  API
// ============================================================================
void sendStatus() {
  String body;
  body.reserve(900);
  body = "{\"ready\":";
  body += driverReady ? "true" : "false";
  body += ",\"camera\":";
  body += cameraReady ? "true" : "false";
  body += ",\"psram\":";
  body += psramReady ? "true" : "false";
  body += ",\"servos\":[";
  for (size_t i = 0; i < JOINT_COUNT; ++i) {
    const Joint &joint = joints[i];
    if (i) body += ',';
    body += "{\"name\":\"";
    body += joint.name;
    body += "\",\"angle\":";
    body += String(joint.current);
    body += ",\"target\":";
    body += String(joint.target);
    body += ",\"min\":";
    body += String(joint.minAngle);
    body += ",\"max\":";
    body += String(joint.maxAngle);
    body += ",\"enabled\":";
    body += joint.enabled ? "true" : "false";
    body += '}';
  }
  body += "]}";
  json(200, body);
}

void enableJoint() {
  if (!requireDriver()) return;
  Joint *joint;
  if (!selectedJoint(joint)) return;

  int on;
  if (!server.hasArg("on") || !parseNumber(server.arg("on"), on) || on > 1) {
    json(400, "{\"error\":\"on debe ser 0 o 1\"}");
    return;
  }

  if (on) {
    if (!joint->enabled) {
      // Primera activación: ir al centro. Sujeta el brazo.
      joint->current = joint->home;
      joint->target  = joint->home;
      writeAngle(*joint);
      joint->enabled = true;
    }
    lastHeartbeat = millis();
  } else {
    pwm.fullOff(joint->channel);
    joint->enabled = false;
    joint->target  = joint->current;
  }
  sendStatus();
}

void setAngle() {
  if (!requireDriver()) return;
  Joint *joint;
  if (!selectedJoint(joint)) return;

  int angle;
  if (!server.hasArg("angle") || !parseNumber(server.arg("angle"), angle) ||
      angle < joint->minAngle || angle > joint->maxAngle) {
    json(400, "{\"error\":\"Angulo fuera de los limites configurados\"}");
    return;
  }
  if (!joint->enabled) {
    json(409, "{\"error\":\"Activa primero este servo\"}");
    return;
  }

  joint->target = angle;
  lastHeartbeat = millis();
  sendStatus();
}

void home() {
  if (!requireDriver()) return;
  for (Joint &joint : joints) {
    if (joint.enabled) joint.target = joint.home;
  }
  lastHeartbeat = millis();
  sendStatus();
}

// ============================================================================
//  INTERPOLACIÓN SUAVE
// ============================================================================
void moveServos() {
  const uint32_t now = millis();

  bool anyEnabled = false;
  for (const Joint &joint : joints) anyEnabled |= joint.enabled;

  if (anyEnabled && now - lastHeartbeat >= LINK_TIMEOUT_MS) {
    stopAll();
    Serial.println("[SEG] Sin latido del navegador: PWM desactivado");
  }

  if (now - lastMotion < MOTION_INTERVAL_MS) return;
  lastMotion = now;

  for (Joint &joint : joints) {
    if (!joint.enabled || joint.current == joint.target) continue;
    joint.current += (joint.current < joint.target) ? 1 : -1;
    writeAngle(joint);
  }
}

// ============================================================================
//  SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n==============================================");
  Serial.println("  BRAZO ROBOTICO + CAMARA ESP32-CAM");
  Serial.println("==============================================");

  // ---- Cámara ----
  if (!startCamera()) {
    Serial.println("[CAM] Continuando sin cámara");
  }

  // ---- Bus I2C y PCA9685 ----
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  driverReady = pwm.begin();
  if (driverReady) {
    pwm.setPWMFreq(50);
    stopAll();
    Serial.println("[PCA] PCA9685 detectado en 0x40 · 50 Hz");
  } else {
    Serial.println("[PCA] NO detectado en 0x40. Revisar SDA/SCL/GND/VCC");
  }

  // ---- Red WiFi (AP) ----
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1),
                    IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));

  if (!WiFi.softAP(AP_NAME, AP_PASSWORD)) {
    Serial.println("[WiFi] ERROR: no se pudo crear el punto de acceso");
    return;
  }

  Serial.printf("[WiFi] Red: %s  Clave: %s\n", AP_NAME, AP_PASSWORD);
  Serial.printf("[WiFi] Abrir: http://%s\n", WiFi.softAPIP().toString().c_str());

  // ---- Rutas HTTP ----
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });

  server.on("/api/status",    HTTP_GET,  sendStatus);
  server.on("/api/enable",    HTTP_POST, enableJoint);
  server.on("/api/servo",     HTTP_POST, setAngle);
  server.on("/api/home",      HTTP_POST, home);
  server.on("/api/stop",      HTTP_POST, []() { stopAll(); sendStatus(); });
  server.on("/api/heartbeat", HTTP_POST, []() {
    lastHeartbeat = millis();
    sendStatus();
  });

  server.onNotFound([]() { json(404, "{\"error\":\"Ruta no encontrada\"}"); });

  server.begin();
  Serial.println("[HTTP] Interfaz web en puerto 80");

  // ---- Stream de video (puerto 81) ----
  startCameraStream();

  Serial.println("==============================================");
  Serial.println("  LISTO. Conectate a la red y abre la IP.");
  Serial.println("==============================================\n");
}

// ============================================================================
//  LOOP
// ============================================================================
void loop() {
  server.handleClient();
  moveServos();
}
