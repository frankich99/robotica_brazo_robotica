# Brazo robótico + Cámara ESP32-CAM — Control por Wi-Fi

Proyecto **completo**: video en vivo + control de **6 servomotores** desde el
navegador del celular o laptop, sin router ni internet.

La ESP32-CAM crea su **propia red WiFi** (modo AP) y sirve una interfaz web
con la cámara y los controles del brazo.

---

## Hardware necesario

| Elemento | Detalle |
|---|---|
| Placa | ESP32-CAM AI Thinker (OV2640) |
| Base / programador | ESP32-CAM-MB (CH340) o adaptador USB-TTL |
| Driver PWM | PCA9685 (I2C, 16 canales) |
| Servos | 6 (base, hombro, codo, muñeca 1, muñeca 2, pinza) |
| Fuente servos | **5 V externa, mínimo 3 A** |
| Condensador | 470–1000 µF entre V+ y GND del PCA9685 (recomendado) |

---

## Cableado

### ESP32-CAM ↔ PCA9685 (I2C)

| ESP32-CAM | PCA9685 |
|---|---|
| GPIO **13** (SDA) | SDA |
| GPIO **14** (SCL) | SCL |
| GND | GND |
| 3V3 | VCC (lógica) |

### Fuente externa ↔ PCA9685

| Fuente 5 V | PCA9685 |
|---|---|
| +5 V | Borne **V+** |
| GND | Borne **GND** |

> ⚠️ **El GND de la fuente debe unirse al GND del ESP32-CAM.**
> ⚠️ **Nunca** alimentes los servos desde el pin 5V de la ESP32-CAM.

### Servos ↔ PCA9685

| Canal | Articulación |
|---|---|
| 0 | Base |
| 1 | Hombro |
| 2 | Codo |
| 3 | Muñeca 1 (giro) |
| 4 | Muñeca 2 (pitch) |
| 5 | Pinza |

---

## Pines ocupados por la cámara (NO usar)

```
GPIO 0, 5, 18, 19, 21, 22, 23, 25, 26, 27, 32, 34, 35, 36, 39
```

Libres para otros usos: `2, 12, 15, 16, 17, 33`.

---

## Red WiFi

| Parámetro | Valor |
|---|---|
| SSID | `BRAZO-ESP32CAM` |
| Clave | `123456789` |
| IP | `192.168.4.1` |
| Clientes | hasta 4 (celular + laptop a la vez) |

Se cambia al inicio del `.ino`:

```cpp
constexpr char AP_NAME[]     = "BRAZO-ESP32CAM";
constexpr char AP_PASSWORD[] = "123456789";   // mínimo 8 caracteres
```

---

## URLs

| URL | Qué hace |
|---|---|
| `http://192.168.4.1/` | Interfaz web (video + servos) |
| `http://192.168.4.1:81/stream` | Video MJPEG |
| `http://192.168.4.1:81/capture` | Una foto JPEG |

---

## API HTTP

| Método | Ruta | Parámetros | Descripción |
|---|---|---|---|
| GET | `/api/status` | — | Estado JSON de servos y cámara |
| POST | `/api/enable` | `i`, `on` | Activa (1) o desactiva (0) el servo `i` |
| POST | `/api/servo` | `i`, `angle` | Mueve el servo `i` al ángulo indicado |
| POST | `/api/home` | — | Centra todos los servos activos |
| POST | `/api/stop` | — | Desactiva todos los pulsos |
| POST | `/api/heartbeat` | — | Mantiene vivos los servos |

Ejemplo con `curl`:

```bash
curl -X POST -d "i=0&on=1"    http://192.168.4.1/api/enable
curl -X POST -d "i=0&angle=45" http://192.168.4.1/api/servo
curl -X POST                  http://192.168.4.1/api/home
```

---

## Cómo grabar

### Arduino IDE
1. Abre `brazo_camara_web.ino`
2. **Board:** `AI Thinker ESP32-CAM`
3. **Port:** el COM de la base
4. **Upload Speed:** `115200`
5. Upload

### Línea de comandos
```powershell
esptool --port COM7 --baud 115200 write-flash -z `
  0x1000  bootloader.bin `
  0x8000  partitions.bin `
  0xe000  boot_app0.bin `
  0x10000 app.bin
```

---

## Cómo usar

1. Conéctate a la red **`BRAZO-ESP32CAM`** (clave `123456789`)
2. Abre `http://192.168.4.1`
3. Pulsa **"Iniciar video"** para ver la cámara
4. Activa un servo con **"Activar"** (va a su centro de 90°)
5. Mueve el deslizador para controlarlo

---

## Calibración

Antes de montar los brazos, ajusta en `brazo_camara_web.ino`:

```cpp
Joint joints[] = {
    {"Base",     0, 30, 150, 90, 1000, 2000, 90, 90, false},
    //            ch  min max home minUs maxUs
};
```

| Campo | Significado |
|---|---|
| `channel` | Canal del PCA9685 |
| `minAngle` / `maxAngle` | Límites de seguridad (evitan choques mecánicos) |
| `home` | Ángulo de reposo |
| `minPulseUs` / `maxPulseUs` | Ancho de pulso real del servo (1000–2000 µs típico) |

> Empieza con límites **estrechos** (30°–150°) y amplía poco a poco.

---

## Seguridad

- **Heartbeat:** si el navegador se desconecta, a los **10 s** se cortan los pulsos.
- **Desactivar ≠ frenar:** sin pulsos el servo no tiene fuerza; el brazo puede caer.
- **Interruptor físico** para la fuente de 5 V de los servos.
- **Sujeta el brazo** al activar cada servo por primera vez.

---

## Solución de problemas

| Síntoma | Causa | Solución |
|---|---|---|
| `PCA9685 no detectado` | I2C mal cableado | Revisar SDA=13, SCL=14, GND común |
| Los servos tiemblan | Fuente débil | Fuente 5 V ≥ 3 A + condensador |
| Se reinicia al mover servos | Caída de tensión | Fuente separada, GND común |
| No hay video | Cámara/PSRAM | Revisar cable flex, alimentación |
| No aparece la red | AP no creado | Reiniciar, revisar monitor serie |
| `Angulo fuera de limites` | Valor fuera de min/max | Ajustar límites o ángulo |

---

## Estructura del proyecto

```
brazo_camara_web/
├── brazo_camara_web.ino   programa principal (WiFi, API, lógica)
├── camara.h               cámara OV2640 + stream MJPEG (puerto 81)
├── servos.h               driver PCA9685 por I2C (sin librerías)
├── pagina.h               interfaz web embebida (HTML/CSS/JS)
└── README.md              este documento
```

---

## Notas técnicas

- **Sin librerías externas:** el PCA9685 se controla con `Wire` directamente,
  así no depende de `Adafruit_PWMServoDriver`.
- **Dos servidores HTTP:** `WebServer` en el puerto 80 (API/UI) y
  `esp_http_server` en el 81 (video), para que el stream no bloquee los controles.
- **Interpolación:** los servos se mueven 1° cada 20 ms (50°/s) para evitar
  golpes mecánicos.
- **Brownout desactivado:** evita reinicios por los picos de corriente del WiFi.
