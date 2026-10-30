# Bitácora Técnica de Laboratorio - Versión 1 (v1)
## Proyecto: Brazo Robótico Articulado de 6 GDL

**Institución:** Universidad Nacional de San Antonio Abad del Cusco (UNSAAC)  
**Asignatura:** Robótica  
**Docente:** Ing. Jose Mauro Pillco Quispe  
**Estudiantes:**  
* Sandro Alexander Salluca Chile  
* Noe Franklin Choquenaira Quispe  
* Flor Deliz Ccasa Ccahuana  
* Luis Alejandro Ramos Aguirre  
**Fecha:** Octubre de 2026  

---

## 1. Requerimientos Dictados por Cátedra
* **Sustitución de Actuadores:** El docente solicitó reemplazar los servomotores estándar por servomotores digitales de alto par torsor para contrarrestar los momentos de inercia y peso de la estructura metálica.
* **Configuración Total de 6 Servomotores:**
  1. Base: Rotación azimuthal sobre rodamiento.
  2. Hombro: Servomotor de 45~kg (`D2450ST 45KG Digital Servo DC 4.8-7.4V`) / Servo de 40~kg (`DSSERVO 40Kg`).
  3. Codo: Servomotor de 40~kg (`DSSERVO 40Kg Digital Servo`).
  4. Muñeca 1 (Pitch): Servomotor de 35~kg (`YSD035 35Kg Digital 4.8V-6V / 500-2500 µs`).
  5. Muñeca 2 (Roll): Servomotor en bracket ortogonal de aluminio.
  6. Pinza (Gripper): Servomotor para mordazas prensiles con engranaje sincronizado.

---

## 2. Estado Real de Avance en Laboratorio (Informe v1)
* **Reconocimiento del Microcontrolador:** 
  * Dispositivo ESP32-CAM verificado exitosamente en Windows bajo el puerto serial `COM7`.
  * Identificador de SoC: `ESP32-D0WDQ6-V3` con dirección MAC `2c:bc:bb:84:a0:7c`.
* **Estado del Cableado Eléctrico:**
  * **Pendiente físico:** A la fecha de este informe v1, el módulo ESP32-CAM **no se ha conectado físicamente** con el controlador PCA9685 ni con la fuente de alimentación externa de los servomotores.
  * Se diseñó el esquemático de interconexión para el bus $I^2C$ en GPIO 13 (SDA) y GPIO 14 (SCL).
* **Incidencia Mecánica Detectada:**
  * Al intentar posicionar el manipulador en posición erguida, la articulación del hombro no presenta rigidez mecánica; el brazo se desliza y cae hacia adelante por acción de la gravedad.
  * **Causa Raíz:** En el servomotor negro de 45~kg (`D2450ST`), falta el perno de apriete central axial M3 en el servocuerno circular de aluminio, lo que provoca que el estriado del eje de salida patine dentro del bracket sin transmitir el par motor.
  * **Acción Correctiva (Siguiente Clase):** Colocar el tornillo M3 axial de fijación con arandela de presión, ajustar los cuatro pernos perimetrales y calibrar el punto neutro ($90^\circ$) antes del cableado.
