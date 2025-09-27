# Proyecto A4 – Sistemas en Tiempo Real

Este proyecto implementa la **Actividad 4: Sensores en tiempo real** de la asignatura *Sistemas en tiempo real*.  
Consiste en:

- Un **ESP32 con FreeRTOS** que lee un sensor ultrasónico HC-SR04 cada 100 ms y envía las mediciones por puerto serie en formato JSON.
- Una **interfaz en Python** (middleware) que recibe esos datos, los muestra en una GUI y manipula una geometría (círculo) en función de la distancia.

---

## 📦 Requisitos

### Hardware
- ESP32-WROOM-32 (o compatible).
- Sensor ultrasónico HC-SR04.
- Resistencias: 330 Ω × 3 (para el divisor resistivo 5 V → 3.3 V).
- Protoboard y cables Dupont.

### Software necesario
- [Arduino IDE](https://www.arduino.cc/en/software) (≥ 2.x).
- **ESP32 Board Package** en Arduino IDE:
  1. Abrir *Archivo → Preferencias*.
  2. En “Gestor de URLs adicionales de tarjetas” agregar:  
     ```
     https://dl.espressif.com/dl/package_esp32_index.json
     ```
  3. Ir a *Herramientas → Placa → Gestor de tarjetas* y buscar **ESP32** (Instalar).
- [Python 3.9+](https://www.python.org/downloads/).
- Pip (gestor de paquetes de Python, normalmente incluido en Python 3).
- Git (opcional, para clonar el repositorio).

---

## ⚡ Montaje del circuito

1. Conectar el **HC-SR04**:
   - VCC → 5V del ESP32  
   - GND → GND del ESP32  
   - TRIG → GPIO5  
   - ECHO → **Divisor resistivo** → GPIO18  

2. **Divisor resistivo para ECHO**:
   - ECHO → R1 = 330 Ω → nodo intermedio  
   - Nodo → R2+R3 (330 Ω + 330 Ω en serie = 660 Ω) → GND  
   - Nodo → GPIO18  

Esto convierte la señal de 5 V en ≈3.3 V segura para el ESP32.

---

## 🖥️ Código Arduino (ESP32)

1. Abrir `arduino/esp32_ultrasonic.ino` en Arduino IDE.
2. Seleccionar placa **ESP32 Dev Module** (o la que corresponda a tu ESP32).
3. Seleccionar el puerto COM correcto.
4. Subir el código al ESP32.

El ESP32 comenzará a imprimir por Serial líneas JSON tipo:

```json
{"distance_cm":42.37}
```

---

## 🖼️ Interfaz Python (GUI)

### Crear entorno virtual (recomendado)

```bash
cd python
python -m venv .venv
# Activar:
# Windows PowerShell:
.\.venv\Scripts\Activate.ps1
# Linux / macOS:
source .venv/bin/activate
```

### Instalar dependencias

```bash
pip install --upgrade pip
pip install pyserial
```

### Ejecutar

```bash
python middleware_gui.py
```

1. Selecciona el puerto COM donde está el ESP32 (ej. `COM3` en Windows o `/dev/ttyUSB0` en Linux).
2. Haz clic en **Conectar**.
3. La GUI mostrará:
   - La distancia en cm.
   - Un círculo cuyo radio cambia con la distancia.
   - Log de mensajes recibidos.

---

## 📁 Estructura del repo

```
/
├── arduino/
│   └── esp32_ultrasonic.ino     # Código para el ESP32 con FreeRTOS
├── python/
│   ├── middleware_gui.py        # GUI en Python (Tkinter + pyserial)
│   └── requirements.txt         # Dependencias del proyecto
└── README.md
```

---

## 🛠️ Troubleshooting

- **Error “Acceso denegado” en COM3:** Cierra el Monitor Serie de Arduino IDE; solo un programa puede usar el puerto a la vez.
- **Valores null:** Revisa el divisor resistivo (330 Ω arriba, 660 Ω abajo).
- **La GUI no abre:** En Linux instala Tkinter:  
  ```bash
  sudo apt-get install python3-tk
  ```
- **Múltiples COM disponibles:** Desconecta y reconecta el ESP32 para identificar cuál aparece en la lista.

---

## ✨ Próximos pasos

Este proyecto es base para la **Actividad 5**: control de motor DC con comandos desde Python.  
Se podrá ampliar la GUI para enviar JSON con velocidad y dirección al ESP32.

---
