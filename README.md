# Robotcillo: Mini Tank Robot controlado con IA

Controla un Mini Tank Robot V2 (Arduino) escribiendo órdenes en lenguaje natural desde el PC. Un modelo local (Ollama + llama3) traduce frases como *"avanza 2 segundos y gira a la derecha"* a comandos de una letra, y un script de Python los envía al Arduino por el puerto serie.

```
Tú escribes  ->  Python + Ollama (llama3)  ->  puerto serie (USB/Bluetooth)  ->  Arduino  ->  motores
"avanza 2s"          "F 2"                         'F' ... 'S'
```

---

## 1. Qué necesitas

**Hardware**
- Mini Tank Robot V2 con Arduino (base: lección 15) y baterías cargadas
- Cable USB **de datos** (no solo de carga) o módulo Bluetooth HC-05/HC-06 emparejado
- Sensor ultrasónico, servo de la cabeza y 2 sensores de luz (los del kit)

**Software (Windows)**
- [Arduino IDE](https://www.arduino.cc/en/software)
- [Python 3.x](https://www.python.org/downloads/)
- [Ollama](https://ollama.com)

---

## 2. Comandos del robot

| Comando | Acción |
|---|---|
| `F` `B` `L` `R` | Avanzar, retroceder, izquierda, derecha |
| `S` | Detenerse |
| `M` | Modo seguir la luz (el robot se mueve) |
| `C` | Modo cámara (robot quieto, la cabeza sigue la luz) |
| `A` | Modo esquivar obstáculos automático |
| `N` | Volver a manual (detenido) |
| `+` / `-` | Subir / bajar velocidad (pasos de 25, entre 100 y 255) |

En modo manual, `F` se bloquea solo si hay un obstáculo a menos de 15 cm. Retroceder y girar siguen permitidos.

---

## 3. Paso a paso

### Paso 1: Cargar el sketch en el Arduino

1. Abre el sketch del robot en el Arduino IDE.
2. Revisa los pines marcados con `CAMBIAR` según el manual de tu kit:
   ```cpp
   #define TRIG_PIN 5     // CAMBIAR
   #define ECHO_PIN 4     // CAMBIAR
   #define SERVO_PIN 10   // CAMBIAR
   ```
3. Conecta el Arduino por USB. Si tienes el módulo Bluetooth puesto, **desconéctalo mientras subes el código**: comparte los pines 0 y 1 con el USB.
4. En **Herramientas → Placa** y **Herramientas → Puerto** elige los correctos y pulsa **Subir**.
5. Cuando termine, **cierra el Monitor Serie** si lo abriste. Si queda abierto, ocupa el puerto y Python no podrá usarlo.

### Paso 2: Instalar Ollama y el modelo

1. Instala Ollama desde ollama.com y deja la app corriendo.
2. En una terminal descarga el modelo (unos 4.7 GB):
   ```
   ollama pull llama3
   ```
3. Comprueba que aparece:
   ```
   ollama list
   ```

### Paso 3: Instalar las librerías de Python

```
pip install pyserial ollama
```

> **Ojo con varias instalaciones de Python.** Si tienes más de una, instala los paquetes en la que usa tu editor. Para asegurarte, usa `python -m pip install ...` con el mismo `python` con el que ejecutas el script.

### Paso 4: Encontrar el puerto COM del robot

```
python -m serial.tools.list_ports
```

- **USB:** aparecerá algo como `COM5 - Arduino Uno` o `USB-SERIAL CH340`.
- **Bluetooth:** empareja el módulo en *Configuración → Bluetooth* (clave habitual `1234` o `0000`). Luego, en *Más opciones de Bluetooth → Puertos COM*, usa el puerto **saliente** que Windows le asignó.

Si aparece `no ports found`, ve a la sección de problemas más abajo.

### Paso 5: Crear el script

Guarda esto como `robotcillo.py`. Si `PUERTO` no existe, el script te muestra los puertos disponibles y te deja elegir.

```python
import re
import time
import serial
from serial.tools import list_ports
import ollama

PUERTO = "COM3"        # cámbialo por tu puerto
BAUDIOS = 9600         # igual que Serial.begin(9600) en el Arduino
MODELO = "llama3"

SISTEMA = """Eres el control de un robot tanque. Traduce lo que diga el usuario
a comandos. Responde SOLO con comandos, uno por linea, sin explicaciones.

Comandos:
F = avanzar
B = retroceder
L = girar a la izquierda
R = girar a la derecha
S = detenerse
M = modo seguir la luz
C = modo camara (robot quieto, la cabeza sigue la luz)
A = modo esquivar obstaculos automatico
N = volver a manual (detenido)
+ = subir velocidad
- = bajar velocidad

Para F, B, L, R puedes agregar los segundos: "F 2" avanza 2 segundos y luego se detiene.
Sin segundos, el movimiento continua hasta recibir S.

Ejemplos:
"avanza dos segundos y gira a la derecha" ->
F 2
R 1
"detente" ->
S
"ve mas rapido" ->
+
Si no entiendes, responde: S"""

PATRON = re.compile(r"^([FBLRSMCAN+\-])(?:\s+(\d+(?:\.\d+)?))?$")


def parsear(texto):
    """Devuelve lista de (comando, segundos) solo con lineas validas."""
    pasos = []
    for linea in texto.upper().splitlines():
        m = PATRON.match(linea.strip())
        if m:
            seg = float(m.group(2)) if m.group(2) else None
            pasos.append((m.group(1), seg))
    return pasos


def enviar(arduino, cmd):
    arduino.write(cmd.encode())
    arduino.flush()


def ejecutar(arduino, pasos):
    for cmd, seg in pasos:
        print(f"  -> {cmd}" + (f" por {seg}s" if seg else ""))
        enviar(arduino, cmd)
        if seg and cmd in "FBLR":
            time.sleep(min(seg, 10))   # tope de seguridad: 10 s
            enviar(arduino, "S")
        else:
            time.sleep(0.1)


def elegir_puerto():
    puertos = list(list_ports.comports())
    if not puertos:
        print("No hay puertos COM. Revisa el cable USB (que sea de datos) o el Bluetooth.")
        raise SystemExit
    if any(p.device == PUERTO for p in puertos):
        return PUERTO
    print(f"{PUERTO} no existe. Puertos disponibles:")
    for i, p in enumerate(puertos):
        print(f"  [{i}] {p.device} - {p.description}")
    return puertos[int(input("Elige el numero: "))].device


def main():
    arduino = serial.Serial(elegir_puerto(), BAUDIOS, timeout=1)
    time.sleep(2)  # el Arduino se reinicia al abrir el puerto

    print("Robot listo. Escribe 'salir' para terminar.")
    try:
        while True:
            texto = input("Tu: ").strip()
            if texto.lower() in ("salir", "exit"):
                break

            respuesta = ollama.chat(
                model=MODELO,
                messages=[
                    {"role": "system", "content": SISTEMA},
                    {"role": "user", "content": texto},
                ],
                options={"temperature": 0},
            )
            bruto = respuesta["message"]["content"].strip()
            print("IA:", bruto.replace("\n", " | "))

            pasos = parsear(bruto)
            if not pasos:
                print("  (sin comando valido, no se envia nada)")
                continue
            ejecutar(arduino, pasos)
    finally:
        enviar(arduino, "S")   # por seguridad, detener al salir
        arduino.close()


if __name__ == "__main__":
    main()
```

### Paso 6: Ejecutar y probar

1. Enciende el robot (interruptor de baterías) y conéctalo.
2. Ejecuta:
   ```
   python robotcillo.py
   ```
3. Prueba con frases como:
   - `avanza 2 segundos`
   - `gira a la derecha 1 segundo y avanza 3`
   - `sigue la luz`
   - `ve más rápido`
   - `detente`

Verás la respuesta del modelo (`IA: F 2`) y luego el comando enviado. La primera respuesta tarda unos segundos porque Ollama carga el modelo en memoria.

---

## 4. Cómo funciona

- El **Arduino** lee un carácter a la vez por `Serial` y cambia de modo o ejecuta el movimiento.
- El **prompt del sistema** obliga al modelo a responder solo con comandos. Con `temperature: 0` las respuestas son estables.
- Python **filtra** la salida del modelo: solo se envían líneas que coincidan con el patrón `LETRA [segundos]`. Cualquier texto extra se ignora.
- Las **duraciones** las maneja Python: manda el movimiento, espera y manda `S`. El Arduino no tiene temporizador para el modo manual.
- Al abrir el puerto, el Arduino se reinicia, por eso el script espera 2 segundos antes de enviar nada.

---

## 5. Solución de problemas

| Error o síntoma | Causa y solución |
|---|---|
| `model 'llama3' not found (404)` | El modelo no está descargado. Ejecuta `ollama pull llama3` y comprueba con `ollama list`. |
| `No module named 'serial'` | `pyserial` no está instalado en ese Python. Usa `python -m pip install pyserial ollama` con el mismo `python` que ejecuta el script. |
| `could not open port 'COM3': FileNotFoundError` | Ese puerto no existe. Ejecuta `python -m serial.tools.list_ports` y cambia `PUERTO`, o deja que el script te muestre la lista. |
| `no ports found` | Windows no ve el Arduino. Prueba otro cable USB (de datos), otro puerto del PC, enciende el robot, y mira el Administrador de dispositivos. Un triángulo amarillo en "Otros dispositivos" suele indicar un clon con chip CH340 que necesita su driver. |
| `Acceso denegado` al abrir el puerto | Otro programa lo ocupa. Cierra el Monitor Serie del Arduino IDE. |
| Con Bluetooth no hay COM | El módulo no está emparejado, o usas el puerto entrante. Empareja y usa el COM **saliente**. |
| El robot no se mueve pero `IA:` muestra un comando | Revisa baterías e interruptor. Si va por Bluetooth, comprueba que el módulo esté bien conectado a los pines 0 y 1. |
| El modelo responde con frases en vez de comandos | Es normal de vez en cuando: el script las ignora. Si pasa mucho, prueba otro modelo o reformula la orden. |

---

## 6. Seguridad

- El script limita cada movimiento a **10 segundos** como máximo.
- Al salir, o si el programa falla, envía `S` para detener el robot.
- Un movimiento sin duración (por ejemplo `F`) continúa hasta recibir `S`. Si se cae la conexión a mitad de movimiento, el robot sigue avanzando; solo el freno por distancia (15 cm) lo protege hacia adelante.
- Prueba siempre en un espacio libre y con el robot a mano.