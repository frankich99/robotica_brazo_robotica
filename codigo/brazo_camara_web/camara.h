/*
 * ============================================================================
 *  camara.h  -  Inicialización de la cámara OV2640 + stream MJPEG
 * ============================================================================
 *  Placa: ESP32-CAM AI Thinker
 *  Stream: http://192.168.4.1:81/stream
 *  Captura: http://192.168.4.1:81/capture
 *
 *  Usa su propio servidor HTTP (puerto 81) para no bloquear el servidor
 *  principal del brazo (puerto 80).
 * ============================================================================
 */

#pragma once

#include <Arduino.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// --- Mapa de pines: ESP32-CAM AI Thinker ------------------------------------
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

// --- Estado global -----------------------------------------------------------
static bool cameraReady = false;
static bool psramReady  = false;
static httpd_handle_t cameraHttpd = nullptr;

static const char *STREAM_TYPE     = "multipart/x-mixed-replace;boundary=frame";
static const char *STREAM_BOUNDARY = "\r\n--frame\r\n";
static const char *STREAM_PART     = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// --- Handler: stream MJPEG ---------------------------------------------------
static esp_err_t streamHandler(httpd_req_t *req) {
  camera_fb_t *fb = nullptr;
  esp_err_t res = httpd_resp_set_type(req, STREAM_TYPE);
  if (res != ESP_OK) return res;

  char partBuf[64];

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      res = ESP_FAIL;
      break;
    }

    size_t hlen = snprintf(partBuf, sizeof(partBuf), STREAM_PART, fb->len);
    res = httpd_resp_send_chunk(req, partBuf, hlen);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));

    esp_camera_fb_return(fb);
    fb = nullptr;

    if (res != ESP_OK) break;
  }
  return res;
}

// --- Handler: una foto JPEG --------------------------------------------------
static esp_err_t captureHandler(httpd_req_t *req) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=captura.jpg");
  esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

// --- Inicialización del sensor ----------------------------------------------
static bool startCamera() {
  // Evita reinicios por caídas de tensión durante los picos de WiFi
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode    = CAMERA_GRAB_LATEST;

  psramReady = psramFound();

  if (psramReady) {
    config.frame_size   = FRAMESIZE_VGA;    // 640 x 480
    config.jpeg_quality = 12;
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size   = FRAMESIZE_QVGA;   // 320 x 240
    config.jpeg_quality = 14;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Error de inicializacion: 0x%x\n", err);
    cameraReady = false;
    return false;
  }

  cameraReady = true;
  Serial.printf("[CAM] OV2640 lista (%s, %s)\n",
                psramReady ? "PSRAM" : "sin PSRAM",
                psramReady ? "VGA 640x480" : "QVGA 320x240");
  return true;
}

// --- Servidor de video en puerto 81 -----------------------------------------
static bool startCameraStream() {
  if (!cameraReady) return false;

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 81;
  config.ctrl_port   = 32768;
  config.max_open_sockets = 4;

  httpd_uri_t streamUri = {};
  streamUri.uri     = "/stream";
  streamUri.method  = HTTP_GET;
  streamUri.handler = streamHandler;

  httpd_uri_t captureUri = {};
  captureUri.uri     = "/capture";
  captureUri.method  = HTTP_GET;
  captureUri.handler = captureHandler;

  if (httpd_start(&cameraHttpd, &config) != ESP_OK) {
    Serial.println("[CAM] No se pudo iniciar el servidor de video");
    return false;
  }

  httpd_register_uri_handler(cameraHttpd, &streamUri);
  httpd_register_uri_handler(cameraHttpd, &captureUri);

  Serial.println("[CAM] Stream disponible en http://192.168.4.1:81/stream");
  return true;
}
