#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// ==========================================================
// FIREBASE
// ==========================================================

#include <WiFi.h>
#include <Firebase_ESP_Client.h>

#define WIFI_SSID "Microcontroladores"
#define WIFI_PASSWORD "raspy123"
#define DATABASE_URL "proyectoembebidos-393fb-default-rtdb.firebaseio.com"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ==========================================================
// LCD 16x2 I2C
// ==========================================================

LiquidCrystal_I2C lcd(0x27, 20, 4);

// ==========================================================
// BUZZER
// ==========================================================

#define PIN_BUZZER 4

// ==========================================================
// SENSORES IR
// ==========================================================

#define IR_010 5
#define IR_025 18
#define IR_005 19
#define IR_100 25
#define IR_050 26

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
  VER_AHORRO
};

Estado estadoActual = LOGIN;

void exito() {

  tone(PIN_BUZZER, 1500, 100);
  delay(120);
  tone(PIN_BUZZER, 2000, 120);
}

void errorSonido() {

  tone(PIN_BUZZER, 300, 400);
}

void victoria() {

  tone(PIN_BUZZER, 523, 100);
  delay(100);

  tone(PIN_BUZZER, 659, 100);
  delay(100);

  tone(PIN_BUZZER, 784, 100);
  delay(100);

  tone(PIN_BUZZER, 1047, 300);
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
  lcd.print("1.Cambiar Clave");

  lcd.setCursor(0,2);
  lcd.print("2.Cambiar Meta");

  lcd.setCursor(0,3);
  lcd.print("3.Ingresos D:Salir");
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

    avance =
      (saldos[usuarioActual] /
      metas[usuarioActual]) * 100;
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

void actualizarPantalla() {

  switch (estadoActual) {

    case LOGIN:
      mostrarLogin();
      break;

    case MENU:
      mostrarMenu();
      break;

    case CAMBIAR_META:
      mostrarMeta();
      break;

    case CAMBIAR_CLAVE:
      mostrarClave();
      break;

    case VER_AHORRO:
      mostrarAhorro();
      break;
  }
}

float detectarMoneda() {

  if (digitalRead(IR_005) == LOW) return 0.05;
  if (digitalRead(IR_010) == LOW) return 0.10;
  if (digitalRead(IR_025) == LOW) return 0.25;
  if (digitalRead(IR_050) == LOW) return 0.50;
  if (digitalRead(IR_100) == LOW) return 1.00;

  return 0;
}

void setup() {

  Serial.begin(115200);

  pinMode(PIN_BUZZER, OUTPUT);

  pinMode(IR_005, INPUT_PULLUP);
  pinMode(IR_010, INPUT_PULLUP);
  pinMode(IR_025, INPUT_PULLUP);
  pinMode(IR_050, INPUT_PULLUP);
  pinMode(IR_100, INPUT_PULLUP);

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

  // ======================================================
  // MANEJO DEL TECLADO
  // ======================================================

  if (tecla) {

    switch (estadoActual) {

      // --------------------------------------------------
      // LOGIN
      // --------------------------------------------------

      case LOGIN:

        if (tecla == 'A') {

          usuarioActual++;

          if (usuarioActual >= NUM_USUARIOS) {
            usuarioActual = 0;
          }

          buffer = "";
        }

        else if (tecla == '*') {

          buffer = "";
        }

        else if (tecla >= '0' && tecla <= '9') {

          buffer += tecla;
        }

        else if (tecla == 'B') {

          if (buffer == claves[usuarioActual]) {

            buffer = "";

            exito();

            estadoActual = MENU; 

            String uidDuenio = uids[usuarioActual];

            if (uidDuenio != "") {

              if (Firebase.RTDB.getFloat(
                    &fbdo,
                    "/usuarios/" + uidDuenio + "/montoActual")) {

                saldos[usuarioActual] = fbdo.floatData();
              }

              if (Firebase.RTDB.getFloat(
                    &fbdo,
                    "/usuarios/" + uidDuenio + "/meta")) {

                if (fbdo.floatData() > 0) {

                  metas[usuarioActual] = fbdo.floatData();
                }
              }
            }

            
          }

          else {

            buffer = "";
            errorSonido();
          }
        }

        break;

      // --------------------------------------------------
      // MENU
      // --------------------------------------------------

      case MENU:

        if (tecla == '1') {

          buffer = "";
          estadoActual = CAMBIAR_CLAVE;
        }

        else if (tecla == '2') {

          buffer = "";
          estadoActual = CAMBIAR_META;
        }

        else if (tecla == '3') {

          estadoActual = VER_AHORRO;
        }

        else if (tecla == 'D') {

          buffer = "";
          estadoActual = LOGIN;
        }

        break;

      // --------------------------------------------------
      // CAMBIAR META
      // --------------------------------------------------

      case CAMBIAR_META:

        if (tecla >= '0' && tecla <= '9') {

          buffer += tecla;
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

              Firebase.RTDB.setFloat(
                &fbdo,
                "/usuarios/" + uidDuenio + "/meta",
                metas[usuarioActual]
              );
            }
          }

          buffer = "";

          estadoActual = MENU;
        }

        else if (tecla == 'D') {

          buffer = "";
          estadoActual = MENU;
        }

        break;

      // --------------------------------------------------
      // CAMBIAR CLAVE
      // --------------------------------------------------

      case CAMBIAR_CLAVE:

        if (tecla >= '0' && tecla <= '9') {

          buffer += tecla;
        }

        else if (tecla == '*') {

          buffer = "";
        }

        else if (tecla == 'B') {

          if (buffer.length() > 0) {

            claves[usuarioActual] = buffer;
          }

          buffer = "";
          estadoActual = MENU;
        }

        else if (tecla == 'D') {

          buffer = "";
          estadoActual = MENU;
        }

        break;

      // --------------------------------------------------
      // VER INGRESOS
      // --------------------------------------------------

      case VER_AHORRO:

        if (tecla == 'D') {

          estadoActual = MENU;
        }

        break;
    }

    actualizarPantalla();
  }

  // ======================================================
  // DETECCION DE MONEDAS
  // ======================================================

  if (estadoActual == VER_AHORRO) {

    float valor = detectarMoneda();

    if (valor > 0) {

      String uidDuenio = uids[usuarioActual];

      if (uidDuenio != "") {

        String rutaSaldo =
          "/usuarios/" + uidDuenio + "/montoActual";

        saldos[usuarioActual] += valor;

        Firebase.RTDB.setFloat(
          &fbdo,
          rutaSaldo,
          saldos[usuarioActual]
        );
      }

      else {

        saldos[usuarioActual] += valor;
      }

    

      int avance = 0;

      if (metas[usuarioActual] > 0) {

        avance =
          (saldos[usuarioActual] /
          metas[usuarioActual]) * 100;
      }

      if (avance > 100) {
        avance = 100;
      }

      if (avance >= 100 &&
          !metasCumplidas[usuarioActual]) {

        metasCumplidas[usuarioActual] = true;

        victoria();
      }

      else {

        exito();
      }

      actualizarPantalla();

      // ==========================================
      // ANTI REBOTE DE SENSORES
      // ==========================================

      unsigned long tiempoInicio = millis();

      while (

        (
          digitalRead(IR_005) == LOW ||
          digitalRead(IR_010) == LOW ||
          digitalRead(IR_025) == LOW ||
          digitalRead(IR_050) == LOW ||
          digitalRead(IR_100) == LOW
        )

        &&

        (millis() - tiempoInicio < 100)

      ) {

        if (keypad.getKey() == 'D') {

          estadoActual = MENU;
          actualizarPantalla();
          return;
        }

        delay(10);
      }

      
    }
  }
}


