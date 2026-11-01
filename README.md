# Brazo Robótico Articulado de 6 GDL - UNSAAC

Repositorio oficial del proyecto de desarrollo, calibración, control y documentación del manipulador robótico de 6 Grados de Libertad (6 GDL) para la asignatura de **Robótica** de la **Universidad Nacional de San Antonio Abad del Cusco (UNSAAC)**.

---

## 👥 Integrantes del Equipo
* **Sandro Alexander Salluca Chile**
* **Noe Franklin Choquenaira Quispe**
* **Flor Deliz Ccasa Ccahuana**

**Docente de Cátedra:** Ing. Jose Mauro Pillco Quispe  
**Semestre:** 2026-II  

---

## 🏗️ Estructura del Repositorio

El proyecto mantiene una estructura modular y organizada:

```
BRAZO ROBOTICO/
├── codigo/
│   ├── brazo_camara_web/        # Control integral: GUI Web interactiva + streaming MJPEG + 6 servos PCA9685
│   ├── camara_esp32_ap_stream/  # Servidor AP independiente de video streaming MJPEG para ESP32-CAM
│   ├── firmware_brazo_esp32/    # Firmware base con interfaz web y control I2C PCA9685
│   ├── prueba_camara_esp32/     # Script diagnóstico de video y captura OV2640
│   └── prueba_i2c_pca9685/      # Escáner I2C para validación del bus PCA9685 (GPIO 13/14)
├── img/                         # Registro fotográfico y evidencias de componentes
├── latex/                       # Documentación formal e informe técnico
│   ├── main.tex                 # Fuente del informe en LaTeX
│   ├── main.pdf                 # Documento final compilado
│   └── img/                     # Diagramas e imágenes referenciadas en el informe
└── proceso/                     # Bitácoras de laboratorio e incidencias técnicas
    └── bitacora_laboratorio_v1.md
```

---

## ⚙️ Especificaciones de Hardware

| Articulación | Actuador / Componente | Rango Torsor / Característica |
| :--- | :--- | :--- |
| **Microcontrolador** | ESP32-CAM (AI-Thinker) + Módulo MB | SoC Dual Core, Wi-Fi, Cámara OV2640 |
| **Driver PWM** | PCA9685 ($I^2C$, 16 canales) | Resolución de 12 bits, bus en GPIO 13/14 |
| **Base (Yaw)** | Rodamiento + Servomotor | Rotación azimuthal sobre plano horizontal |
| **Hombro (Pitch)** | Servo D2450ST 45 kg / DSSERVO 40 kg | Alto par torsor para compensar gravedad |
| **Codo (Pitch)** | Servo DSSERVO 40 kg Digital | Transmisión con bracket de aluminio |
| **Muñeca 1 (Pitch)** | Servo YSD035 35 kg Digital | Control angular de orientación de enlace |
| **Muñeca 2 (Roll)** | Servomotor digital bracket ortogonal | Rotación axial del efector final |
| **Gripper / Pinza** | Servomotor estándar / micro servo | Mecanismo prensil dentado simétrico |

---

## 🚀 Puesta en Marcha

1. **Configuración de Pines $I^2C$ en ESP32-CAM:**
   - **SDA:** GPIO 13
   - **SCL:** GPIO 14
2. **Alimentación:**
   - Lógica: 5 V vía programador micro-USB MB.
   - Potencia de Servomotores: Fuente externa regulada dedicada (5V - 7.4V, corriente nominal $\ge 10$ A).
   - **Importante:** Unificar la línea de tierra (GND) entre fuente externa, PCA9685 y ESP32.
3. **Carga de Firmware:**
   - Abrir `codigo/firmware_brazo_esp32/brazoR.ino` en Arduino IDE.
   - Configurar credenciales Wi-Fi (modo AP o modo STA).
   - Seleccionar placa `AI Thinker ESP32-CAM` y subir código.
