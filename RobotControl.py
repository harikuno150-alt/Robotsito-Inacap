import re
import time
import serial
import ollama

PUERTO = "COM3"        # el COM del cable USB o del Bluetooth (HC-05/06)
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


def main():
    arduino = serial.Serial(PUERTO, BAUDIOS, timeout=1)
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