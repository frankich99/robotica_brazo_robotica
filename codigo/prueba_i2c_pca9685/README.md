# Scanner I2C para ESP32-CAM (Verificación del PCA9685)

Este script permite comprobar el conexionado físico del bus $I^2C$ entre el microcontrolador **ESP32-CAM** y el módulo de modulación PWM **PCA9685** antes de energizar los servomotores.

### Asignación de Pines:
* **SDA:** GPIO 13
* **SCL:** GPIO 14
* **Dirección esperada:** `0x40` (y dirección de llamada global `0x70`).

### Procedimiento:
1. Conectar GND común entre el ESP32-CAM y el PCA9685.
2. Alimentar el pin VCC del PCA9685 con 3.3V o 5V desde el ESP32.
3. Abrir el monitor serial a 115200 baudios.
4. Si la terminal imprime `0x40 [*** DRIVER PWM PCA9685 CONFIRMADO ***]`, la capa física de comunicaciones está lista para recibir comandos angulares.
