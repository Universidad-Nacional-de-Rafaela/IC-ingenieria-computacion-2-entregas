// Grupo 4 - Nodo publicador (ESP32 WROOM) con painlessMesh
// Potenciometro -> casa/ventilador/  (0 a 100)
// Boton (toggle) -> casa/luz/         (0 o 1)
//
// Librerias (Gestor de librerias): painlessMesh, ArduinoJson, TaskScheduler, AsyncTCP
// Si da errores de compilacion con ArduinoJson, instalar la version 6.x

#include <painlessMesh.h>
#include "secrets.h"   // copiar secrets.h.example a secrets.h y completar

// ===== ACORDAR CON EL GRUPO 1 (tienen que ser identicos) =====
#define MESH_PREFIX "GRUPO1_MESH"   // nombre de la mesh
// MESH_PASSWORD (contrasena de la mesh) esta en secrets.h, que NO se sube a GitHub
#define MESH_PORT     5555
#define MESH_CANAL    1              // canal del router (en el log era el 1)
// =============================================================

#define PIN_POT 34
#define PIN_BTN 25
#define PIN_LED 2    // LED integrado, solo para ver el toggle

#define TOPIC_LUZ        "casa/luz/"
#define TOPIC_VENTILADOR "casa/ventilador/"

const unsigned long TIEMPO_REBOTE = 50;  // ms

Scheduler userScheduler;
painlessMesh mesh;

int ultimoPot = -1;
int luz = 0;

int estadoBtn = 0;
int lecturaAnteriorBtn = 0;
unsigned long ultimoCambioBtn = 0;

void leerPot();
void leerBtn();
void refrescar();

Task tareaPot(100, TASK_FOREVER, &leerPot);
Task tareaBtn(20, TASK_FOREVER, &leerBtn);
Task tareaRefresco(5000, TASK_FOREVER, &refrescar);

// Arma {"t":"<topic>","v":<valor>} y lo manda a toda la mesh (el gateway lo recibe)
void publicar(const char* topic, int valor) {
  char msg[64];
  snprintf(msg, sizeof(msg), "{\"t\":\"%s\",\"v\":%d}", topic, valor);
  bool ok = mesh.sendBroadcast(msg);
  Serial.printf("TX %s (%s)\n", msg, ok ? "ok" : "error");
}

void leerPot() {
  long suma = 0;
  for (int i = 0; i < 10; i++) suma += analogRead(PIN_POT);
  int pot = map(suma / 10, 0, 4095, 0, 100);

  if (abs(pot - ultimoPot) >= 2) {   // solo si cambio de verdad
    ultimoPot = pot;
    publicar(TOPIC_VENTILADOR, pot);
  }
}

void leerBtn() {
  int lectura = !digitalRead(PIN_BTN);   // 1 = apretado
  if (lectura != lecturaAnteriorBtn) {
    ultimoCambioBtn = millis();
    lecturaAnteriorBtn = lectura;
  }
  if ((millis() - ultimoCambioBtn) > TIEMPO_REBOTE && lectura != estadoBtn) {
    estadoBtn = lectura;
    if (estadoBtn == 1) {                // solo al APRETAR
      luz = !luz;
      digitalWrite(PIN_LED, luz);
      publicar(TOPIC_LUZ, luz);
    }
  }
}

// Reenvia el estado actual cada 5 s, por si algun mensaje se perdio
void refrescar() {
  if (ultimoPot >= 0) publicar(TOPIC_VENTILADOR, ultimoPot);
  publicar(TOPIC_LUZ, luz);
}

void recibido(uint32_t desde, String &msg) {
  Serial.printf("RX de %u: %s\n", desde, msg.c_str());
}

void cambioConexiones() {
  Serial.printf("Nodos conectados en la mesh: %d\n", mesh.getNodeList().size());
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BTN, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_POT, ADC_11db);

  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, MESH_CANAL);
  mesh.setContainsRoot(true);   // hay un gateway (root) en la mesh; confirmar con el Grupo 1
  mesh.onReceive(&recibido);
  mesh.onChangedConnections(&cambioConexiones);

  userScheduler.addTask(tareaPot);
  userScheduler.addTask(tareaBtn);
  userScheduler.addTask(tareaRefresco);
  tareaPot.enable();
  tareaBtn.enable();
  tareaRefresco.enable();

  Serial.printf("Nodo listo. ID de este nodo: %u\n", mesh.getNodeId());
}

void loop() {
  mesh.update();   // no usar delay() aca: la mesh necesita correr libre
}
