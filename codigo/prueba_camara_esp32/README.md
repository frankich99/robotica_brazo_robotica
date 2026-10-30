# Prueba de Transmisión de Cámara (ESP32-CAM + OV2640)

Este sketch permite verificar de forma aislada el funcionamiento del sensor de imagen OV2640 del módulo **AI-THINKER ESP32-CAM**.

### Características Técnicas:
1. **Punto de Acceso Independiente:** Despliega una red Wi-Fi propia llamada `Brazo-CAM-Stream` (Clave: `Robotica1234`).
2. **Servidor HTTP Dedicado:** Escucha en el puerto `81`.
3. **Endpoint de Transmisión:** `http://192.168.4.1:81/stream`.
4. **Formato de Salida:** Streaming continuo de video en protocolo MJPEG compatible con cualquier etiqueta `<img>` de HTML5.

### Instrucciones de Carga:
* **Placa en Arduino IDE:** `AI Thinker ESP32-CAM`.
* **CPU Frequency:** 240MHz (WiFi/BT).
* **Flash Frequency:** 80MHz.
* **Flash Mode:** QIO.
* **Partition Scheme:** Huge APP (3MB No OTA/1MB SPIFFS).
* **PSRAM:** Enabled.
* **Puerto:** COM7 (según reconocimiento de hardware verificado en Windows).
