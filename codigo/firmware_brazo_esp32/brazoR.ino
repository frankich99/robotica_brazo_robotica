#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_PWMServoDriver.h>

// Ambos servidores definen HTTP_ANY con valores distintos; aquí no se usa.
#undef HTTP_ANY
#include "camara.h"
#include "pagina.h"

// Cambia la clave antes de usar el brazo fuera de un entorno controlado.
constexpr char AP_NAME[] = "Brazo-ESP32CAM";
constexpr char AP_PASSWORD[] = "CambiaEstaClave123";
constexpr uint8_t SDA_PIN = 13;
constexpr uint8_t SCL_PIN = 14;
constexpr uint8_t PCA_ADDRESS = 0x40;
constexpr uint32_t LINK_TIMEOUT_MS = 10000;
constexpr uint32_t MOTION_INTERVAL_MS = 20;

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

// Límites conservadores de partida: calibrar ANTES de montar los brazos.
Joint joints[] = {
    {"Base",       0, 30, 150, 90, 1000, 2000, 90, 90, false},
    {"Hombro",    1, 30, 150, 90, 1000, 2000, 90, 90, false},
    {"Codo",       2, 30, 150, 90, 1000, 2000, 90, 90, false},
    {"Muneca 1",   3, 30, 150, 90, 1000, 2000, 90, 90, false},
    {"Muneca 2",   4, 30, 150, 90, 1000, 2000, 90, 90, false},
    {"Pinza",      5, 30, 150, 90, 1000, 2000, 90, 90, false},
};
constexpr size_t JOINT_COUNT = sizeof(joints) / sizeof(joints[0]);

Adafruit_PWMServoDriver pwm(PCA_ADDRESS, Wire);
WebServer server(80);
bool driverReady = false;
uint32_t lastHeartbeat = 0;
uint32_t lastMotion = 0;

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

void stopAll() {
  if (!driverReady) return;
  for (Joint &joint : joints) {
    // FULL_OFF: sin pulsos; el servo puede caer por gravedad.
    pwm.setPWM(joint.channel, 0, 4096);
    joint.enabled = false;
    joint.target = joint.current;
  }
}

void writeAngle(Joint &joint) {
  const uint16_t pulse = map(joint.current, 0, 180,
                             joint.minPulseUs, joint.maxPulseUs);
  pwm.writeMicroseconds(joint.channel, pulse);
}

bool selectedJoint(Joint *&joint) {
  int index;
  if (!server.hasArg("i") || !parseNumber(server.arg("i"), index) ||
      index >= static_cast<int>(JOINT_COUNT)) {
    json(400, "{\"error\":\"Indice de servo invalido\"}");
    return false;
  }
  joint = &joints[index];
  return true;
}

bool requireDriver() {
  if (!driverReady) {
    json(503, "{\"error\":\"PCA9685 no detectado: revisar cableado y reiniciar\"}");
    return false;
  }
  return true;
}

void sendStatus() {
  String body;
  body.reserve(780);
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
    // Primera activación: ir al centro nominal. Sujetar el brazo.
    if (!joint->enabled) {
      joint->current = joint->home;
      joint->target = joint->home;
      writeAngle(*joint);
      joint->enabled = true;
    }
    lastHeartbeat = millis();
  } else {
    pwm.setPWM(joint->channel, 0, 4096);
    joint->enabled = false;
    joint->target = joint->current;
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
    json(409, "{\"error\":\"Activar primero este servo\"}");
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

void moveServos() {
  const uint32_t now = millis();
  bool anyEnabled = false;
  for (const Joint &joint : joints) anyEnabled |= joint.enabled;
  if (anyEnabled && now - lastHeartbeat >= LINK_TIMEOUT_MS) {
    stopAll();
    Serial.println("Sin latido del navegador: PWM desactivado");
  }
  if (now - lastMotion < MOTION_INTERVAL_MS) return;
  lastMotion = now;
  for (Joint &joint : joints) {
    if (!joint.enabled || joint.current == joint.target) continue;
    joint.current += (joint.current < joint.target) ? 1 : -1;
    writeAngle(joint);
  }
}

void setup() {
  Serial.begin(115200);
  startCamera();
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  driverReady = pwm.begin();
  if (driverReady) {
    pwm.setPWMFreq(50);
    stopAll();
  } else {
    Serial.println("PCA9685 no detectado en 0x40. Revisar SDA/SCL, GND y VCC.");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  if (!WiFi.softAP(AP_NAME, AP_PASSWORD)) {
    Serial.println("ERROR: no se pudo crear la red Wi-Fi");
    return;
  }
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });
  server.on("/api/status", HTTP_GET, sendStatus);
  server.on("/api/enable", HTTP_POST, enableJoint);
  server.on("/api/servo", HTTP_POST, setAngle);
  server.on("/api/home", HTTP_POST, home);
  server.on("/api/stop", HTTP_POST, []() { stopAll(); sendStatus(); });
  server.on("/api/heartbeat", HTTP_POST, []() {
    lastHeartbeat = millis();
    sendStatus();
  });
  server.onNotFound([]() { json(404, "{\"error\":\"Ruta no encontrada\"}"); });
  server.begin();
  startCameraStream();
  Serial.print("Conectar a ");
  Serial.print(AP_NAME);
  Serial.print("; abrir http://");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();
  moveServos();
}
