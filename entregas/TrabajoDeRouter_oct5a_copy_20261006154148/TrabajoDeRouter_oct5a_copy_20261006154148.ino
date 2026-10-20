#include <Arduino.h>
#include <WiFi.h>            // solo para WiFiClient
#include <PubSubClient.h>
#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <esp_mesh.h>

// ---------- Configuración ----------
static const uint8_t MESH_ID[6] = {0x77,0x77,0x77,0x77,0x77,0x77};
#define MESH_PASSWORD "password_mesh123"
#define ROUTER_SSID   "IngComputacion"
#define ROUTER_PASS   "IngCom2026"
#define MQTT_HOST     "192.168.0.103"
#define MQTT_PORT     1883
static const uint8_t MAC_G5[6] = {0x3C,0xE9,0x0E,0x4C,0x8A,0x70};   // 08:B6:1F:81:12:94

typedef struct {
  char actuador[16];
  char valor[16];
} Mensaje;                                   // 32 bytes, idéntico en G5

static esp_netif_t* netif_sta = nullptr;
static volatile bool rootConIP = false;
WiFiClient espClient;
PubSubClient mqtt(espClient);

// ---------- Enviar a G5 por mesh ----------
void enviarAG5(const char* act, const char* val) {
  Mensaje m;
  memset(&m, 0, sizeof(m));
  strncpy(m.actuador, act, sizeof(m.actuador) - 1);
  strncpy(m.valor,    val, sizeof(m.valor) - 1);

  mesh_addr_t dest;
  memcpy(dest.addr, MAC_G5, 6);

  mesh_data_t d;
  d.data  = (uint8_t*)&m;
  d.size  = sizeof(m);
  d.proto = MESH_PROTO_BIN;
  d.tos   = MESH_TOS_P2P;

  esp_err_t r = esp_mesh_send(&dest, &d, MESH_DATA_P2P, NULL, 0);
  Serial.printf("[TX] %s = %s -> G5 (%s)\n", m.actuador, m.valor, esp_err_to_name(r));
}

// ---------- MQTT -> mesh ----------
void onMqtt(char* topic, byte* payload, unsigned int len) {
  char val[16] = {0};
  memcpy(val, payload, len < 15 ? len : 15);
  Serial.printf("[MQTT] %s = %s\n", topic, val);

  bool esOn  = (strcasecmp(val, "ON")  == 0);
  bool esOff = (strcasecmp(val, "OFF") == 0);

  if (strcmp(topic, "casa/luz/") == 0) {
    if (esOn)       enviarAG5("luz", "1");
    else if (esOff) enviarAG5("luz", "0");
    else            enviarAG5("luz", val);        // ya viene 0/1
  } else if (strcmp(topic, "casa/ventilador/") == 0) {
    if (esOn)       enviarAG5("ventilador", "255");
    else if (esOff) enviarAG5("ventilador", "0");
    else            enviarAG5("ventilador", val); // número 0-255
  }
}

void conectarMqtt() {
  String id = "gateway-g3-" + String(random(0xffff), HEX);
  Serial.print("[MQTT] Conectando...");
  if (mqtt.connect(id.c_str())) {
    Serial.println(" ok");
    mqtt.subscribe("casa/luz/");
    mqtt.subscribe("casa/ventilador/");
  } else {
    Serial.printf(" fallo rc=%d\n", mqtt.state());
  }
}

// ---------- Eventos ----------
static void ipHandler(void*, esp_event_base_t, int32_t, void* data) {
  auto* e = (ip_event_got_ip_t*)data;
  uint8_t canal; wifi_second_chan_t s;
  esp_wifi_get_channel(&canal, &s);
  Serial.printf("[IP] Root con IP " IPSTR " | canal %d\n", IP2STR(&e->ip_info.ip), canal);
  rootConIP = true;
}

static void meshHandler(void*, esp_event_base_t, int32_t id, void* data) {
  switch (id) {
    case MESH_EVENT_STARTED:
      Serial.println("[MESH] Iniciado, buscando router..."); break;
    case MESH_EVENT_PARENT_CONNECTED: {
      auto* c = (mesh_event_connected_t*)data;
      Serial.printf("[MESH] Conectado. Capa %d | soy root: %d\n", c->self_layer, esp_mesh_is_root());
      if (esp_mesh_is_root()) {          // el root pide IP al router
        esp_netif_dhcpc_stop(netif_sta);
        esp_netif_dhcpc_start(netif_sta);
      }
      break; }
    case MESH_EVENT_PARENT_DISCONNECTED: {
      auto* d = (mesh_event_disconnected_t*)data;
      Serial.printf("[MESH][WARN] Desconectado. Motivo: %d\n", d->reason);
      rootConIP = false;
      break; }
    case MESH_EVENT_CHILD_CONNECTED: {
      auto* c = (mesh_event_child_connected_t*)data;
      Serial.printf("[MESH] HIJO CONECTADO: %02X:%02X:%02X:%02X:%02X:%02X\n",
        c->mac[0], c->mac[1], c->mac[2], c->mac[3], c->mac[4], c->mac[5]);
      break; }
    case MESH_EVENT_CHILD_DISCONNECTED:
      Serial.println("[MESH] Hijo desconectado"); break;
    default: break;
  }
}

// ---------- Inicio del mesh ----------
void iniciarMesh() {
  nvs_flash_init();
  ESP_ERROR_CHECK(esp_netif_init());
  esp_event_loop_create_default();                       // puede devolver "ya creado": se ignora
  ESP_ERROR_CHECK(esp_netif_create_default_wifi_mesh_netifs(&netif_sta, NULL));

  wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ipHandler, NULL));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_ERROR_CHECK(esp_mesh_init());
  ESP_ERROR_CHECK(esp_event_handler_register(MESH_EVENT, ESP_EVENT_ANY_ID, &meshHandler, NULL));

  mesh_cfg_t cfg = MESH_INIT_CONFIG_DEFAULT();
  memcpy((uint8_t*)&cfg.mesh_id, MESH_ID, 6);
  cfg.channel = 0;                                       // 0 = adopta el canal del router
  cfg.router.ssid_len = strlen(ROUTER_SSID);
  memcpy(cfg.router.ssid, ROUTER_SSID, cfg.router.ssid_len);
  memcpy(cfg.router.password, ROUTER_PASS, strlen(ROUTER_PASS));
  cfg.mesh_ap.max_connection = 6;
  cfg.mesh_ap.nonmesh_max_connection = 0;
  memcpy(cfg.mesh_ap.password, MESH_PASSWORD, strlen(MESH_PASSWORD));

  ESP_ERROR_CHECK(esp_mesh_set_ap_authmode(WIFI_AUTH_WPA2_PSK));
  ESP_ERROR_CHECK(esp_mesh_set_max_layer(6));
  ESP_ERROR_CHECK(esp_mesh_set_config(&cfg));

  // Raíz fija (G5 debe poner fix_root(true) también)
  Serial.printf("[MESH] fix_root: %s\n",  esp_err_to_name(esp_mesh_fix_root(true)));
  Serial.printf("[MESH] set_type: %s\n",  esp_err_to_name(esp_mesh_set_type(MESH_ROOT)));

  ESP_ERROR_CHECK(esp_mesh_start());
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== GATEWAY G3 (ROOT ESP-WIFI-MESH) ===");

  iniciarMesh();

  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  Serial.printf("MAC STA de G3: %02X:%02X:%02X:%02X:%02X:%02X  <- pasarla a G5\n",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqtt);
}

void loop() {

  static uint32_t hb = 0;
if (millis() - hb > 5000) {
  hb = millis();
  Serial.printf("[ESTADO] root=%d conIP=%d mqtt=%d\n",
                esp_mesh_is_root(), rootConIP, mqtt.connected());
}

  // Pruebas por Serial, sin broker: v/x = ventilador ON/OFF, l/k = luz ON/OFF
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'v') enviarAG5("ventilador", "ON");
    if (c == 'x') enviarAG5("ventilador", "OFF");
    if (c == 'l') enviarAG5("luz", "ON");
    if (c == 'k') enviarAG5("luz", "OFF");
  }

  static uint32_t t = 0;
  if (rootConIP) {
    if (!mqtt.connected()) {
      if (millis() - t > 5000) { t = millis(); conectarMqtt(); }
    } else {
      mqtt.loop();
    }
  }
  delay(10);
}