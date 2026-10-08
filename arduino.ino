#include <Servo.h>   // viene con el Arduino IDE

/*
 Mini Tank Robot V2 - version avanzada (base: leccion 15)
 Comandos Bluetooth:
  F B L R S  -> control manual
  M          -> modo seguir luz (el robot se mueve)
  C          -> modo camara (robot quieto, la cabeza sigue la luz)
  A          -> modo esquivar obstaculos automatico
  N          -> volver a manual (detenido)
  +  -       -> subir / bajar velocidad
*/

unsigned char start01[] = {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
unsigned char front[] = {0x00,0x00,0x00,0x00,0x00,0x24,0x12,0x09,0x12,0x24,0x00,0x00,0x00,0x00,0x00,0x00};
unsigned char back[] = {0x00,0x00,0x00,0x00,0x00,0x24,0x48,0x90,0x48,0x24,0x00,0x00,0x00,0x00,0x00,0x00};
unsigned char left[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x44,0x28,0x10,0x44,0x28,0x10,0x44,0x28,0x10,0x00};
unsigned char right[] = {0x00,0x10,0x28,0x44,0x10,0x28,0x44,0x10,0x28,0x44,0x00,0x00,0x00,0x00,0x00,0x00};
unsigned char STOP01[] = {0x2E,0x2A,0x3A,0x00,0x02,0x3E,0x02,0x00,0x3E,0x22,0x3E,0x00,0x3E,0x0A,0x0E,0x00};
unsigned char clear[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

#define SCL_Pin  A5
#define SDA_Pin  A4

#define ML_Ctrl 13
#define ML_PWM 11
#define MR_Ctrl 12
#define MR_PWM 3

#define LUZ_IZQ A1
#define LUZ_DER A2

// CAMBIAR segun el manual de tu kit
#define TRIG_PIN 5     // CAMBIAR (Proyecto 5 u 11)
#define ECHO_PIN 4     // CAMBIAR (Proyecto 5 u 11)
#define SERVO_PIN 10   // CAMBIAR (Proyecto 4)

const int UMBRAL_LUZ = 650;
const int DIST_MIN = 15;                    // cm
const unsigned long INTERVALO_MEDIDA = 60;  // ms entre mediciones

// Ajustes del modo camara
const int CENTRO = 90;
const int ANGULO_MIN = 20;
const int ANGULO_MAX = 160;
const int ZONA_MUERTA = 40;    // diferencia minima de luz para moverse
const int PASO = 2;            // grados por ciclo
const bool INVERTIR = false;   // true si la cabeza gira al lado contrario

enum Modo { MANUAL, LUZ, AUTO, CAMARA };
Modo modo = MANUAL;

Servo cabeza;
int anguloCabeza = CENTRO;

char ultimoComando = 'S';
int velocidad = 200;              // 100 a 255
long distancia = 999;
unsigned long tMedida = 0;
char patronActual = 0;

void setup() {
  Serial.begin(9600);
  pinMode(SCL_Pin, OUTPUT);
  pinMode(SDA_Pin, OUTPUT);
  pinMode(ML_Ctrl, OUTPUT);
  pinMode(ML_PWM, OUTPUT);
  pinMode(MR_Ctrl, OUTPUT);
  pinMode(MR_PWM, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  cabeza.attach(SERVO_PIN);
  cabeza.write(CENTRO);

  matrix_display(clear);
  matrix_display(start01);
  delay(800);
  Car_Stop();
  mostrar(STOP01, 'S');
}

void loop() {
  leerBluetooth();
  actualizarDistancia();

  switch (modo) {
    case MANUAL: modoManual(); break;
    case LUZ:    modoLuz();    break;
    case AUTO:   modoAuto();   break;
    case CAMARA: modoCamara(); break;
  }
}

/************** Cambio de modo **************/
void cambiarModo(Modo nuevo) {
  if (modo == CAMARA && nuevo != CAMARA) {
    anguloCabeza = CENTRO;      // al salir, la cabeza vuelve al frente
    cabeza.write(CENTRO);
  }
  modo = nuevo;
}

/************** Comunicacion y sensores **************/
void leerBluetooth() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    switch (c) {
      case 'F': case 'B': case 'L': case 'R': case 'S':
        cambiarModo(MANUAL);
        ultimoComando = c;
        break;
      case 'M': cambiarModo(LUZ);  break;
      case 'A': cambiarModo(AUTO); break;
      case 'C':
        cambiarModo(CAMARA);
        Car_Stop();
        mostrar(STOP01, 'S');
        break;
      case 'N':
        cambiarModo(MANUAL);
        ultimoComando = 'S';
        break;
      case '+': velocidad = min(255, velocidad + 25); break;
      case '-': velocidad = max(100, velocidad - 25); break;
    }
  }
}

long medirDistancia() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duracion = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duracion == 0) return 999;
  return duracion / 58;
}

void actualizarDistancia() {
  if (modo == CAMARA) return;   // la cabeza esta girando: la medida no sirve
  if (millis() - tMedida >= INTERVALO_MEDIDA) {
    distancia = medirDistancia();
    tMedida = millis();
  }
}

/************** Modos de operacion **************/
void modoManual() {
  // El freno solo bloquea avanzar; retroceder y girar siguen permitidos
  if (ultimoComando == 'F' && distancia < DIST_MIN) {
    Car_Stop();
    mostrar(STOP01, 'S');
    return;
  }
  switch (ultimoComando) {
    case 'F': Car_front(); mostrar(front, 'F');  break;
    case 'B': Car_back();  mostrar(back, 'B');   break;
    case 'L': Car_left();  mostrar(left, 'L');   break;
    case 'R': Car_right(); mostrar(right, 'R');  break;
    default:  Car_Stop();  mostrar(STOP01, 'S'); break;
  }
}

void modoLuz() {
  if (distancia < DIST_MIN) {
    Car_Stop();
    mostrar(STOP01, 'S');
    return;
  }
  int izq = analogRead(LUZ_IZQ);
  int der = analogRead(LUZ_DER);

  if (izq > UMBRAL_LUZ && der > UMBRAL_LUZ) {
    Car_front();
    mostrar(front, 'F');
  } else if (izq > UMBRAL_LUZ && der <= UMBRAL_LUZ) {
    Car_left();
    mostrar(left, 'L');
  } else if (izq <= UMBRAL_LUZ && der > UMBRAL_LUZ) {
    Car_right();
    mostrar(right, 'R');
  } else {
    Car_Stop();
    mostrar(STOP01, 'S');
  }
}

void modoAuto() {
  if (distancia < DIST_MIN) {
    // Obstaculo: frena, retrocede un poco y gira
    Car_Stop();
    mostrar(STOP01, 'S');
    delay(150);
    Car_back();
    mostrar(back, 'B');
    delay(400);
    Car_right();
    mostrar(right, 'R');
    delay(450);
    Car_Stop();
  } else {
    Car_front();
    mostrar(front, 'F');
  }
}

void modoCamara() {
  Car_Stop();
  int izq = analogRead(LUZ_IZQ);
  int der = analogRead(LUZ_DER);
  int dif = izq - der;

  if (abs(dif) > ZONA_MUERTA) {
    int dir = (dif > 0) ? 1 : -1;
    if (INVERTIR) dir = -dir;
    anguloCabeza = constrain(anguloCabeza + dir * PASO, ANGULO_MIN, ANGULO_MAX);
    cabeza.write(anguloCabeza);
  }
  delay(20);
}

/************** Matriz LED **************/
// Solo redibuja si el patron cambio
void mostrar(unsigned char *patron, char id) {
  if (id != patronActual) {
    matrix_display(patron);
    patronActual = id;
  }
}

void matrix_display(unsigned char matrix_value[]) {
  IIC_start();
  IIC_send(0xc0);
  for (int i = 0; i < 16; i++) {
    IIC_send(matrix_value[i]);
  }
  IIC_end();

  IIC_start();
  IIC_send(0x8A);
  IIC_end();
}

void IIC_start() {
  digitalWrite(SCL_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(SDA_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(SDA_Pin, LOW);
  delayMicroseconds(3);
}

void IIC_send(unsigned char send_data) {
  for (char i = 0; i < 8; i++) {
    digitalWrite(SCL_Pin, LOW);
    delayMicroseconds(3);
    if (send_data & 0x01) {
      digitalWrite(SDA_Pin, HIGH);
    } else {
      digitalWrite(SDA_Pin, LOW);
    }
    delayMicroseconds(3);
    digitalWrite(SCL_Pin, HIGH);
    delayMicroseconds(3);
    send_data = send_data >> 1;
  }
}

void IIC_end() {
  digitalWrite(SCL_Pin, LOW);
  delayMicroseconds(3);
  digitalWrite(SDA_Pin, LOW);
  delayMicroseconds(3);
  digitalWrite(SCL_Pin, HIGH);
  delayMicroseconds(3);
  digitalWrite(SDA_Pin, HIGH);
  delayMicroseconds(3);
}

/************** Motores **************/
int velGiro() {
  return min(255, velocidad + 55);
}

void Car_front() {
  digitalWrite(MR_Ctrl, LOW);
  analogWrite(MR_PWM, velocidad);
  digitalWrite(ML_Ctrl, LOW);
  analogWrite(ML_PWM, velocidad);
}

void Car_back() {
  digitalWrite(MR_Ctrl, HIGH);
  analogWrite(MR_PWM, velocidad);
  digitalWrite(ML_Ctrl, HIGH);
  analogWrite(ML_PWM, velocidad);
}

void Car_left() {
  digitalWrite(MR_Ctrl, LOW);
  analogWrite(MR_PWM, velGiro());
  digitalWrite(ML_Ctrl, HIGH);
  analogWrite(ML_PWM, velGiro());
}

void Car_right() {
  digitalWrite(MR_Ctrl, HIGH);
  analogWrite(MR_PWM, velGiro());
  digitalWrite(ML_Ctrl, LOW);
  analogWrite(ML_PWM, velGiro());
}

void Car_Stop() {
  digitalWrite(MR_Ctrl, LOW);
  analogWrite(MR_PWM, 0);
  digitalWrite(ML_Ctrl, LOW);
  analogWrite(ML_PWM, 0);
}