# Proyecto A4 – Sistemas en Tiempo Real

Este proyecto implementa la **Actividad 4: Sensores en tiempo real** de la asignatura *Sistemas en tiempo real*.  

Incluye:
- **ESP32 + FreeRTOS** leyendo un sensor ultrasónico HC-SR04 cada 100 ms y enviando los datos por puerto serie en JSON.
- **Middleware en Python** con GUI (Tkinter) que muestra la distancia y manipula una geometría en tiempo real.

---

## 🖥️ 1. Preparación del entorno en una máquina nueva

### 1.1 Instalar Git
1. Descarga desde [https://git-scm.com/downloads](https://git-scm.com/downloads).
2. Instálalo con las opciones por defecto.
3. Abre una terminal (PowerShell en Windows o Terminal en Linux/macOS).
4. Verifica:
   ```bash
   git --version
   ```

### 1.2 Clonar el repositorio
En la carpeta de tu preferencia:
```bash
git clone https://github.com/tu-usuario/proyecto-a4.git
cd proyecto-a4
```

---

## 🔧 2. Instalar Arduino IDE y configurar ESP32

### 2.1 Instalar Arduino IDE
- Descarga de [Arduino IDE](https://www.arduino.cc/en/software) (versión 2.x recomendada).
- Instala con opciones por defecto.

### 2.2 Instalar soporte para ESP32
1. Abre Arduino IDE.
2. Ve a **Archivo → Preferencias**.
3. En “Gestor de URLs adicionales de tarjetas” agrega:
   ```
   https://dl.espressif.com/dl/package_esp32_index.json
   ```
4. Luego abre **Herramientas → Placa → Gestor de tarjetas**.
5. Busca **esp32** e instala el paquete de Espressif.

### 2.3 Seleccionar placa y puerto(Si arduino no reconoce el ESP32 conectado descargar el driver e instalar https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers?tab=downloads)
Una vez descargado el driver buscar el archivo "silabser.inf", click derecho y seleccionar la opcion "Instalar"

- Conecta tu ESP32 por USB.
- En Arduino IDE:  
  - **Herramientas → Placa → ESP32 Arduino → ESP32 Dev Module**.  
  - **Herramientas → Puerto → COMX** (Windows) o `/dev/ttyUSB0` (Linux) o `/dev/cu.SLAB_USBtoUART` (macOS).

### 2.4 Subir el código de Arduino
1. Abre el archivo:
   ```
   arduino/esp32_ultrasonic.ino
   ```
2. Haz clic en **Verificar** y luego **Subir**.
3. El ESP32 empezará a enviar mensajes JSON como:
   ```json
   {"distance_cm":42.37}
   ```

---

## ⚡ 3. Montaje del circuito

### 3.1 Conexiones HC-SR04
- VCC → 5V del ESP32  
- GND → GND del ESP32  
- TRIG → GPIO5  
- ECHO → **Divisor resistivo** → GPIO18  

### 3.2 Divisor resistivo
- ECHO → R1 = 330 Ω → Nodo intermedio  
- Nodo → (R2 + R3 en serie = 660 Ω) → GND  
- Nodo → GPIO18  

Esto reduce la señal de 5 V a ≈3.3 V.

---

## 🐍 4. Instalar Python e interfaz gráfica

### 4.1 Instalar Python 3
- Descarga de [Python.org](https://www.python.org/downloads/) (3.9+).
- En Windows: marcar **“Add Python to PATH”** durante la instalación.
- Verifica en terminal:
  ```bash
  python --version
  ```
  o
  ```bash
  python3 --version
  ```

### 4.2 Crear entorno virtual
En la carpeta `python/` del repo:
```bash
cd python
python -m venv .venv
```

Activar el entorno:
- **Windows PowerShell**
  ```powershell
  .\.venv\Scripts\Activate.ps1
  ```
- **Linux/macOS**
  ```bash
  source .venv/bin/activate
  ```

### 4.3 Instalar dependencias
```bash
pip install --upgrade pip
pip install -r requirements.txt
```

Si no tienes `requirements.txt`, instala manualmente:
```bash
pip install pyserial
```

En Linux puede que necesites:
```bash
sudo apt-get install python3-tk
```

### 4.4 Ejecutar la interfaz
```bash
python middleware_gui.py
```

Pasos en la GUI:
1. Selecciona el puerto COM del ESP32 (`COM3` en Windows, `/dev/ttyUSB0` en Linux).
2. Haz clic en **Conectar**.
3. Verás:
   - Distancia en cm.
   - Un círculo cuyo radio cambia con la distancia.
   - Logs de datos recibidos.

---

## 📁 Estructura del repo

```
/
├── arduino/
│   └── esp32_ultrasonic.ino     # Código para ESP32 con FreeRTOS
├── python/
│   ├── middleware_gui.py        # GUI en Python
│   └── requirements.txt         # Dependencias de Python
└── README.md
```

---

## 🛠️ Troubleshooting

- **Error "Acceso denegado" en COM**  
  Cierra el *Monitor Serie* de Arduino IDE; solo un programa puede usar el puerto a la vez.
- **Valores null en JSON**  
  Revisa el divisor resistivo (330 Ω arriba, 660 Ω abajo).
- **La GUI no abre**  
  Instala Tkinter (`sudo apt-get install python3-tk` en Linux).
- **No ves el puerto**  
  Reinstala drivers del ESP32 (CH340 o CP2102, según tu placa).

---

## ✨ Próximos pasos

Este proyecto es la base para la **Actividad 5**:  
Control de un motor DC con comandos desde Python. La GUI podrá ampliarse con sliders y botones para enviar JSON al ESP32.

---
