# Cámara ESP32-CAM — Modo AP (stream MJPEG)

Proyecto **funcional y verificado** en hardware real (ESP32-CAM AI Thinker + base MB).

La placa **crea su propia red WiFi** y sirve video en vivo. **No necesita router ni internet.**

---

## Hardware

| Elemento | Detalle |
|---|---|
| Placa | ESP32-CAM AI Thinker |
| Sensor | OV2640 |
| Base | ESP32-CAM-MB (chip CH340) o adaptador USB-TTL |
| Alimentación | 5V / ~500 mA |

---

## Configuración

| Parámetro | Valor |
|---|---|
| Red WiFi (SSID) | `FRANKI-HE` |
| Contraseña | `123456789` |
| IP del AP | `192.168.4.1` |
| Modelo de cámara | `CAMERA_MODEL_AI_THINKER` (en `board_config.h`) |
| Resolución inicial | QVGA (320×240), ajustable desde la web |
| Formato | JPEG / MJPEG |

Para cambiar la red o la clave, edita en el `.ino`:

```cpp
const char *ssid     = "FRANKI-HE";
const char *password = "123456789";   // mínimo 8 caracteres
```

---

## Cómo grabar

### Arduino IDE
1. Abre `camara_esp32_ap_stream.ino`
2. **Board:** `AI Thinker ESP32-CAM`
3. **Port:** el COM de la base (ej. `COM7`)
4. **Upload Speed:** `115200`
5. Upload

### Línea de comandos (más confiable)
```powershell
esptool --port COM7 --baud 115200 write-flash -z `
  0x1000  bootloader.bin `
  0x8000  partitions.bin `
  0xe000  boot_app0.bin `
  0x10000 app.bin
```

---

## Cómo usar

1. Conéctate a la red **`FRANKI-HE`** (clave `123456789`)
2. Abre en el navegador:

| URL | Qué muestra |
|---|---|
| `http://192.168.4.1` | Interfaz web (empieza aquí) |
| `http://192.168.4.1/stream` | Solo el video MJPEG |
| `http://192.168.4.1/control` | Panel de ajustes de cámara |

3. Pulsa **"Start Stream"**

> ⚠️ Mientras estés conectado a `FRANKI-HE` no tendrás internet.

---

## Salida esperada en el Monitor Serie (115200)

```
Camara inicializada correctamente
Punto de acceso ACTIVO
SSID: FRANKI-HE
Clave: 123456789
IP:   192.168.4.1
Camera Ready! Use 'http://192.168.4.1/stream' to connect
```

---

## Solución de problemas

| Síntoma | Causa | Solución |
|---|---|---|
| No aparece el COM | Cable solo-carga o conector | Cambiar cable USB de datos |
| `Camera init failed 0x...` | Sensor o pines | Verificar `board_config.h` = AI_THINKER |
| Bucle de puntos `.....` | Está en modo STA buscando router | Usar modo AP (por defecto) |
| No carga `192.168.4.1` | No estás en la red del AP | Conectar a `FRANKI-HE` |
| Reinicios al iniciar stream | Caída de tensión | Fuente 5V más potente |
| Imagen oscura / verde | Sensor OV2640 mal configurado | Ajustar en `/control` |

---

## Archivos del proyecto

| Archivo | Función |
|---|---|
| `camara_esp32_ap_stream.ino` | Setup principal + WiFi AP |
| `app_httpd.cpp` | Servidor HTTP y stream MJPEG |
| `board_config.h` | Selección del modelo de cámara |
| `camera_pins.h` | Mapa de pines del sensor |
| `camera_index.h` | Interfaz web embebida (HTML/JS) |
| `partitions.csv` | Tabla de particiones |
