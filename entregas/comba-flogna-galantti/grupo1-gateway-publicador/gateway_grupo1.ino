#include <WiFi.h>
#include <PubSubClient.h>
#include <painlessMesh.h>
#include <ArduinoJson.h>

// =====================================================
// CONFIGURACIÓN DE LA RED WIFI
// =====================================================

#define WIFI_SSID     "UNRaf_Libre"
#define WIFI_PASSWORD "unraf2021"


// =====================================================
// CONFIGURACIÓN MQTT
// =====================================================

#define MQTT_SERVER "10.103.11.11"
#define MQTT_PORT   1883


// =====================================================
// CONFIGURACIÓN MESH
// IMPORTANTE:
// ESTOS DATOS TIENEN QUE SER IGUALES EN GRUPO 1 Y GRUPO 4
// =====================================================

#define MESH_PREFIX   "GRUPO1_MESH"
#define MESH_PASSWORD "grupo12026"
#define MESH_PORT     5555
#define MESH_CANAL    1


// =====================================================
// OBJETOS
// =====================================================

Scheduler userScheduler;

painlessMesh mesh;

WiFiClient espClient;
PubSubClient mqttClient(espClient);


// =====================================================
// CONTROL DE RECONEXIÓN MQTT
// =====================================================

unsigned long ultimoIntentoMQTT = 0;

const unsigned long intervaloMQTT = 5000;


// =====================================================
// FUNCIÓN QUE SE EJECUTA CUANDO LLEGA UN MENSAJE
// DESDE EL GRUPO 4 POR MESH
// =====================================================

void mensajeRecibido(uint32_t from, String &msg) {

  Serial.println();
  Serial.println("======================================");

  Serial.print("Mensaje recibido por Mesh desde nodo: ");
  Serial.println(from);

  Serial.print("Mensaje: ");
  Serial.println(msg);


  // ---------------------------------------------------
  // Interpretar el mensaje JSON
  // ---------------------------------------------------

  StaticJsonDocument<256> documento;

  DeserializationError error = deserializeJson(documento, msg);

  if (error) {

    Serial.print("Error al interpretar JSON: ");
    Serial.println(error.c_str());

    Serial.println("======================================");

    return;
  }


  // ---------------------------------------------------
  // Comprobar que existan los campos t y v
  // ---------------------------------------------------

  if (!documento.containsKey("t") ||
      !documento.containsKey("v")) {

    Serial.println("ERROR: faltan los campos t o v");

    Serial.println("======================================");

    return;
  }


  // ---------------------------------------------------
  // Obtener el topic enviado por Grupo 4
  // ---------------------------------------------------

  const char* topic = documento["t"];


  // ---------------------------------------------------
  // Convertir el valor recibido a texto
  // ---------------------------------------------------

  String valor;

  if (documento["v"].is<int>()) {

    valor = String(documento["v"].as<int>());

  } else if (documento["v"].is<float>()) {

    valor = String(documento["v"].as<float>());

  } else {

    valor = documento["v"].as<String>();
  }


  Serial.print("Topic recibido: ");
  Serial.println(topic);

  Serial.print("Valor recibido: ");
  Serial.println(valor);


  // ---------------------------------------------------
  // Publicar en MQTT
  // ---------------------------------------------------

  if (mqttClient.connected()) {

    bool publicado =
      mqttClient.publish(topic, valor.c_str());


    if (publicado) {

      Serial.println("Publicado correctamente en MQTT");

    } else {

      Serial.println("ERROR al publicar en MQTT");
    }

  } else {

    Serial.println("MQTT no esta conectado");

    Serial.println("El mensaje NO pudo publicarse");
  }


  Serial.println("======================================");
}


// =====================================================
// CUANDO CAMBIA LA CONEXIÓN DE LA MESH
// =====================================================

void conexionesCambiaron() {

  Serial.println();
  Serial.println("Cambio en la red Mesh");

  Serial.print("Nodos conectados: ");
  Serial.println(mesh.getNodeList().size());
}


// =====================================================
// CONECTAR MQTT
// =====================================================

void conectarMQTT() {

  // Primero verificamos que el ESP32 tenga conexión WiFi
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }


  // Si ya está conectado a MQTT no hacemos nada
  if (mqttClient.connected()) {
    return;
  }


  // Evitar intentar conectarse constantemente
  if (millis() - ultimoIntentoMQTT < intervaloMQTT) {
    return;
  }

  ultimoIntentoMQTT = millis();


  Serial.println();

  Serial.print("Conectando al broker MQTT ");
  Serial.print(MQTT_SERVER);
  Serial.print(":");
  Serial.print(MQTT_PORT);
  Serial.print(" ... ");


  if (mqttClient.connect("Grupo1_Gateway")) {

    Serial.println("CONECTADO");

    Serial.print("IP del Gateway: ");
    Serial.println(WiFi.localIP());

  } else {

    Serial.print("ERROR. Codigo MQTT: ");
    Serial.println(mqttClient.state());
  }
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  delay(1000);


  Serial.println();
  Serial.println("======================================");
  Serial.println("       GATEWAY - GRUPO 1");
  Serial.println("======================================");


  // ---------------------------------------------------
  // INICIAR MESH
  // ---------------------------------------------------

  Serial.println();
  Serial.println("Iniciando red Mesh...");


  mesh.setDebugMsgTypes(ERROR | STARTUP);

  mesh.init(
    MESH_PREFIX,
    MESH_PASSWORD,
    &userScheduler,
    MESH_PORT,
    WIFI_AP_STA,
    MESH_CANAL
  );


  // ---------------------------------------------------
  // Conectar la Mesh también a UNRaf_Libre
  // ---------------------------------------------------

  mesh.stationManual(
    WIFI_SSID,
    WIFI_PASSWORD
  );


  // Nuestra gateway es la raíz de esta Mesh
  mesh.setRoot(true);

  mesh.setContainsRoot(true);


  // ---------------------------------------------------
  // CALLBACKS MESH
  // ---------------------------------------------------

  mesh.onReceive(&mensajeRecibido);

  mesh.onChangedConnections(&conexionesCambiaron);


  Serial.println("Mesh iniciada");

  Serial.print("Nombre Mesh: ");
  Serial.println(MESH_PREFIX);

  Serial.print("Puerto Mesh: ");
  Serial.println(MESH_PORT);

  Serial.print("Canal Mesh: ");
  Serial.println(MESH_CANAL);


  // ---------------------------------------------------
  // CONFIGURAR MQTT
  // ---------------------------------------------------

  mqttClient.setServer(
    MQTT_SERVER,
    MQTT_PORT
  );


  Serial.println();
  Serial.println("Esperando conexion...");
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  // MUY IMPORTANTE:
  // mantener funcionando la red Mesh
  mesh.update();


  // Intentar conectar MQTT cuando haya WiFi
  conectarMQTT();


  // Mantener activa la conexión MQTT
  if (mqttClient.connected()) {

    mqttClient.loop();
  }
}