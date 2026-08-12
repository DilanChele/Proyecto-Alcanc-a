#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <ESP32Servo.h>

// ==========================================================
// FIREBASE
// ==========================================================

#include <WiFi.h>
#include <Firebase_ESP_Client.h>

#define WIFI_SSID "XTRIM_ALMEIDA"
#define WIFI_PASSWORD "edith195210"
#define DATABASE_URL "proyectoembebidos-393fb-default-rtdb.firebaseio.com"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ==========================================================
// LCD 16x2 I2C
// ==========================================================

LiquidCrystal_I2C lcd(0x27, 20, 4);

// ==========================================================
// BUZZER aislando el canal para no interferir con servos
// ==========================================================

#define PIN_BUZZER 4
#define CANAL_BUZZER 15 // Usamos el último canal para evitar conflictos

void sonarBuzzer(int frecuencia, int duracion) {
  ledcAttachPin(PIN_BUZZER, CANAL_BUZZER);
  ledcWriteTone(CANAL_BUZZER, frecuencia);
  delay(duracion);
  ledcWriteTone(CANAL_BUZZER, 0); // Apagar
}

// ==========================================================
// SENSORES IR (Monedas: 0.10, 0.25, 0.50, 1.00)
// ==========================================================

#define IR_010 5
#define IR_025 18
#define IR_050 19 
#define IR_100 25

// ==========================================================
// SERVOMOTORES (Estructura Física)
// ==========================================================

#define PIN_SERVO_1 14 // Servo 1 (Moneda $0.25)
#define PIN_SERVO_2 2  // Servo 2 (Moneda $1 - Lógica invertida)
#define PIN_SERVO_3 12 // Servo 3 (Moneda $0.50)
#define PIN_SERVO_4 26 // Servo 4 (Moneda $0.10)

Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;

// ==========================================================
// CONFIGURACIÓN DE MULTA POR RETIRO TEMPRANO
// ==========================================================
const float PORCENTAJE_MULTA = 0.05; 

const byte FILAS = 4;
const byte COLUMNAS = 4;

char teclas[FILAS][COLUMNAS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte pinesFilas[FILAS] = {15,13,23,27};
byte pinesColumnas[COLUMNAS] = {16,17,32,33};

Keypad keypad = Keypad(
  makeKeymap(teclas),
  pinesFilas,
  pinesColumnas,
  FILAS,
  COLUMNAS
);

const int NUM_USUARIOS = 4;

String nombres[NUM_USUARIOS] = {
  "Papa",
  "Mama",
  "Hermano",
  "Nino"
};

String claves[NUM_USUARIOS] = {
  "1234",
  "5678",
  "1111",
  "2222"
};

String uids[NUM_USUARIOS] = {
  "eDsWkr3PUIawexLQsWkt4ljRCb63",
  "kDlgwwKSG9clQP7EVNRpkUscZOS2",
  "e3NfWto8z5QDQDOzPcT1aPBS2i02",
  "vADl0tkWcGUPdh6OSw6QHf7Nxb03"
};

float saldos[NUM_USUARIOS] = {0,0,0,0};
float metas[NUM_USUARIOS]  = {20,20,20,20};
float penalizaciones[NUM_USUARIOS] = {0,0,0,0}; 

bool metasCumplidas[NUM_USUARIOS] = {
  false,false,false,false
};

int usuarioActual = 0;
String buffer = "";

enum Estado {
  LOGIN,
  MENU,
  CAMBIAR_META,
  CAMBIAR_CLAVE,
  VER_AHORRO,
  RETIRAR_DINERO 
};

Estado estadoActual = LOGIN;

void exito() {
  sonarBuzzer(1500, 100);
  delay(20);
  sonarBuzzer(2000, 120);
}

void errorSonido() {
  sonarBuzzer(300, 400);
}

void victoria() {
  sonarBuzzer(523, 100);
  sonarBuzzer(659, 100);
  sonarBuzzer(784, 100);
  sonarBuzzer(1047, 300);
}

void mostrarLogin() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("SISTEMA AHORRO");
  lcd.setCursor(0,1);
  lcd.print("Usuario:");
  lcd.print(nombres[usuarioActual]);
  lcd.setCursor(0,2);
  lcd.print("Clave:");
  for(int i = 0; i < buffer.length(); i++) {
    lcd.print("*");
  }
  lcd.setCursor(0,3);
  lcd.print("A:Cambiar B:Entrar");
}

void mostrarMenu() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Usuario:");
  lcd.print(nombres[usuarioActual]);
  lcd.setCursor(0,1);
  lcd.print("1.Clave 2.Meta");
  lcd.setCursor(0,2);
  lcd.print("3.Ahorro 4.Retirar");
  lcd.setCursor(0,3);
  lcd.print("D:Salir");
}

void mostrarMeta() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("CAMBIAR META");
  lcd.setCursor(0,1);
  lcd.print("Actual: $");
  lcd.print(metas[usuarioActual],2);
  lcd.setCursor(0,2);
  lcd.print("Nueva Meta:");
  lcd.setCursor(0,3);
  lcd.print("$");
  lcd.print(buffer);
}

void mostrarClave() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("CAMBIAR CLAVE");
  lcd.setCursor(0,1);
  lcd.print("Nueva:");
  lcd.setCursor(0,2);
  for(int i = 0; i < buffer.length(); i++) {
    lcd.print("*");
  }
}

void mostrarAhorro() {
  lcd.clear();
  int avance = 0;
  if (metas[usuarioActual] > 0) {
    avance = (saldos[usuarioActual] / metas[usuarioActual]) * 100;
  }
  if (avance > 100) {
    avance = 100;
  }
  lcd.setCursor(0,0);
  lcd.print("Usuario:");
  lcd.print(nombres[usuarioActual]);
  lcd.setCursor(0,1);
  lcd.print("Ingreso:$");
  lcd.print(saldos[usuarioActual],2);
  lcd.setCursor(0,2);
  lcd.print("Meta:$");
  lcd.print(metas[usuarioActual],2);
  lcd.setCursor(0,3);
  lcd.print("Avance:");
  lcd.print(avance);
  lcd.print("%");
}

void mostrarRetiro() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("RETIRO (Disp:$");
  lcd.print(saldos[usuarioActual], 2);
  lcd.print(")");
  lcd.setCursor(0,1);
  if (saldos[usuarioActual] >= metas[usuarioActual]) {
    lcd.print("Meta OK: Sin Multa");
  } else {
    lcd.print("Multa x Retiro: 5%");
  }
  lcd.setCursor(0,2);
  lcd.print("Monto a sacar:");
  lcd.setCursor(0,3);
  lcd.print("$");
  lcd.print(buffer);
}

void actualizarPantalla() {
  switch (estadoActual) {
    case LOGIN: mostrarLogin(); break;
    case MENU: mostrarMenu(); break;
    case CAMBIAR_META: mostrarMeta(); break;
    case CAMBIAR_CLAVE: mostrarClave(); break;
    case VER_AHORRO: mostrarAhorro(); break;
    case RETIRAR_DINERO: mostrarRetiro(); break;
  }
}

float detectarMoneda() {
  if (digitalRead(IR_010) == LOW) return 0.10;
  if (digitalRead(IR_025) == LOW) return 0.25;
  if (digitalRead(IR_050) == LOW) return 0.50;
  if (digitalRead(IR_100) == LOW) return 1.00;
  return 0;
}

void setup() {
  Serial.begin(115200);

  // Configurar pines de sensores
  pinMode(IR_010, INPUT_PULLUP);
  pinMode(IR_025, INPUT_PULLUP);
  pinMode(IR_050, INPUT_PULLUP);
  pinMode(IR_100, INPUT_PULLUP);

  // ======================================================
  // CONFIGURACIÓN DE SERVOMOTORES
  // ======================================================
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo3.setPeriodHertz(50);
  servo4.setPeriodHertz(50);

  // ATTACH
  servo1.attach(PIN_SERVO_1, 500, 2400);
  servo2.attach(PIN_SERVO_2, 500, 2400);
  servo3.attach(PIN_SERVO_3, 500, 2400);
  servo4.attach(PIN_SERVO_4, 500, 2400);

  // WRITE (Servo 2 invertido con 90 en reposo)
  servo1.write(0);
  servo2.write(90); // Posición de reposo invertida para el Pin 2
  servo3.write(0);
  servo4.write(0);

  // ======================================================

  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("Conectando WiFi");
  lcd.setCursor(0,1);
  lcd.print("Espere...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }

  config.database_url = DATABASE_URL;
  config.signer.test_mode = true;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  actualizarPantalla();
}

void loop() {
  char tecla = keypad.getKey();

  if (tecla) {
    switch (estadoActual) {

      case LOGIN:
        if (tecla == 'A') {
          usuarioActual++;
          if (usuarioActual >= NUM_USUARIOS) usuarioActual = 0;
          buffer = "";
        }
        else if (tecla == '*') buffer = "";
        else if (tecla >= '0' && tecla <= '9') buffer += tecla;
        else if (tecla == 'B') {
          if (buffer == claves[usuarioActual]) {
            
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("Cargando perfil...");
            lcd.setCursor(0,1);
            lcd.print("Por favor espere");

            String uidDuenio = uids[usuarioActual];
            if (uidDuenio != "") {
              
              if (Firebase.RTDB.get(&fbdo, "/usuarios/" + uidDuenio + "/montoActual")) {
                if (fbdo.dataType() == "int") saldos[usuarioActual] = (float)fbdo.intData();
                else saldos[usuarioActual] = fbdo.floatData();
              }
              
              if (Firebase.RTDB.get(&fbdo, "/usuarios/" + uidDuenio + "/meta")) {
                float metaBD = 0;
                if (fbdo.dataType() == "int") metaBD = (float)fbdo.intData();
                else metaBD = fbdo.floatData();
                if (metaBD > 0) metas[usuarioActual] = metaBD;
              }

              if (Firebase.RTDB.get(&fbdo, "/usuarios/" + uidDuenio + "/penalizacionesPorRetiro")) {
                if (fbdo.dataType() == "int") penalizaciones[usuarioActual] = (float)fbdo.intData();
                else penalizaciones[usuarioActual] = fbdo.floatData();
              }
            }

            buffer = "";
            exito();
            estadoActual = MENU; 
          } else {
            buffer = "";
            errorSonido();
          }
        }
        break;

      case MENU:
        if (tecla == '1') { buffer = ""; estadoActual = CAMBIAR_CLAVE; }
        else if (tecla == '2') { buffer = ""; estadoActual = CAMBIAR_META; }
        else if (tecla == '3') { estadoActual = VER_AHORRO; }
        else if (tecla == '4') { buffer = ""; estadoActual = RETIRAR_DINERO; }
        else if (tecla == 'D') { buffer = ""; estadoActual = LOGIN; }
        break;

      case CAMBIAR_META:
        if (tecla >= '0' && tecla <= '9') {
          buffer += tecla;
        }
        else if (tecla == 'C') { // TECLA 'C' COMO PUNTO DECIMAL
          if (buffer.indexOf('.') == -1) { 
            if (buffer.length() == 0) buffer = "0.";
            else buffer += ".";
          }
        }
        else if (tecla == '*') {
          buffer = "";
        }
        else if (tecla == 'B') {
          if (buffer.length() > 0) {
            metas[usuarioActual] = buffer.toFloat();
            metasCumplidas[usuarioActual] = false;
            String uidDuenio = uids[usuarioActual];
            if (uidDuenio != "") {
              Firebase.RTDB.setFloatAsync(&fbdo, "/usuarios/" + uidDuenio + "/meta", metas[usuarioActual]);
            }
          }
          buffer = "";
          estadoActual = MENU;
        }
        else if (tecla == 'D') { buffer = ""; estadoActual = MENU; }
        break;

      case CAMBIAR_CLAVE:
        if (tecla >= '0' && tecla <= '9') buffer += tecla;
        else if (tecla == '*') buffer = "";
        else if (tecla == 'B') {
          if (buffer.length() > 0) claves[usuarioActual] = buffer;
          buffer = "";
          estadoActual = MENU;
        }
        else if (tecla == 'D') { buffer = ""; estadoActual = MENU; }
        break;

      case VER_AHORRO:
        if (tecla == 'D') estadoActual = MENU;
        break;

      // --------------------------------------------------
      // RETIRAR DINERO FÍSICAMENTE Y GUARDAR EN BD
      // --------------------------------------------------
      case RETIRAR_DINERO:
        if (tecla >= '0' && tecla <= '9') {
          buffer += tecla;
        }
        else if (tecla == 'C') { // TECLA 'C' COMO PUNTO DECIMAL
          if (buffer.indexOf('.') == -1) {
            if (buffer.length() == 0) buffer = "0.";
            else buffer += ".";
          }
        }
        else if (tecla == '*') {
          buffer = ""; 
        }
        else if (tecla == 'B') { 
          if (buffer.length() > 0) {
            float montoARetirar = buffer.toFloat();
            float multaAplicada = 0.0;

            if (saldos[usuarioActual] < metas[usuarioActual]) {
              multaAplicada = montoARetirar * PORCENTAJE_MULTA;
            }

            float descuentoTotal = montoARetirar + multaAplicada;

            if (montoARetirar > 0 && saldos[usuarioActual] >= descuentoTotal) {
              
              saldos[usuarioActual] -= descuentoTotal;
              
              if (multaAplicada > 0) {
                penalizaciones[usuarioActual] += multaAplicada; 
              }

              if (saldos[usuarioActual] < metas[usuarioActual]) {
                metasCumplidas[usuarioActual] = false;
              }

              lcd.clear();
              lcd.setCursor(0,0);
              lcd.print("Entregando dinero...");
              
              int centavosFaltantes = (montoARetirar * 100.0) + 0.5;

              // Expulsa monedas de $1.00 (Servo 2 / Pin 2) - LÓGICA INVERTIDA
              while (centavosFaltantes >= 100) {
                servo2.write(0); delay(500); servo2.write(90); delay(500); 
                centavosFaltantes -= 100;
              }
              // Expulsa monedas de $0.50 (Servo 3 / Pin 12)
              while (centavosFaltantes >= 50) {
                servo3.write(90); delay(500); servo3.write(0); delay(500);
                centavosFaltantes -= 50;
              }
              // Expulsa monedas de $0.25 (Servo 1 / Pin 14)
              while (centavosFaltantes >= 25) {
                servo1.write(90); delay(500); servo1.write(0); delay(500);
                centavosFaltantes -= 25;
              }
              // Expulsa monedas de $0.10 (Servo 4 / Pin 26)
              while (centavosFaltantes >= 10) {
                servo4.write(90); delay(500); servo4.write(0); delay(500);
                centavosFaltantes -= 10;
              }

              exito(); 

              String uidDuenio = uids[usuarioActual];
              if (uidDuenio != "") {
                Firebase.RTDB.setFloat(&fbdo, "/usuarios/" + uidDuenio + "/montoActual", saldos[usuarioActual]);
                
                if (multaAplicada > 0) {
                  Firebase.RTDB.setFloat(&fbdo, "/usuarios/" + uidDuenio + "/penalizacionesPorRetiro", penalizaciones[usuarioActual]);
                }
              }

              buffer = "";
              estadoActual = MENU;

            } else {
              errorSonido(); 
              buffer = "";
            }
          } else {
            estadoActual = MENU;
          }
        }
        else if (tecla == 'D') {
          buffer = "";
          estadoActual = MENU; 
        }
        break;
    }

    actualizarPantalla();
  }

  // ======================================================
  // DETECCION DE MONEDAS AL INGRESAR 
  // ======================================================

  if (estadoActual == VER_AHORRO) {
    float valor = detectarMoneda();

    if (valor > 0) {
      
      saldos[usuarioActual] += valor;

      int avance = 0;
      if (metas[usuarioActual] > 0) {
        avance = (saldos[usuarioActual] / metas[usuarioActual]) * 100;
      }
      if (avance > 100) avance = 100;

      if (avance >= 100 && !metasCumplidas[usuarioActual]) {
        metasCumplidas[usuarioActual] = true;
        victoria();
      } else {
        exito();
      }

      actualizarPantalla();

      String uidDuenio = uids[usuarioActual];
      if (uidDuenio != "") {
        String rutaSaldo = "/usuarios/" + uidDuenio + "/montoActual";
        Firebase.RTDB.setFloat(&fbdo, rutaSaldo, saldos[usuarioActual]);
      }

      unsigned long tiempoInicio = millis();

      while (
        (
          digitalRead(IR_010) == LOW ||
          digitalRead(IR_025) == LOW ||
          digitalRead(IR_050) == LOW ||
          digitalRead(IR_100) == LOW
        )
        &&
        (millis() - tiempoInicio < 2000) 
      ) {
        if (keypad.getKey() == 'D') {
          estadoActual = MENU;
          actualizarPantalla();
          return;
        }
        delay(10);
      }
      
      delay(250); 
    }
  }
}