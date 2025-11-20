#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <M5Unified.h>
#include "m5_unit_mq.hpp"

/* ======= Configuration WiFi ======= */
const char* WIFI_SSID     = "Topinambour";
const char* WIFI_PASSWORD = "abcdefghi";

/* ======= Configuration Serveur ======= */
const char* serverUrl = "http://172.20.10.3:5000/data"; // IP de ton serveur

/* ======= Capteur ======= */
#define I2C_SDA   (21)
#define I2C_SCL   (22)
#define I2C_SPEED (400000)
#define HIGH_LEVEL_TIME (30)
#define LOW_LEVEL_TIME  (5)

M5UnitMQ unitMQ;

/* Variables capteur */
static led_status_t ledStatus = LED_WORK_STATUS_OFF;
static heat_mode_t heatMode = HEAT_MODE_PIN_SWITCH;
static mq_adc_valid_tags_t validTags = VALID_TAG_INVALID;
static uint16_t mqAdc12bit = 0, ntcAdc12bit = 0, ntcResistance = 0, referenceVoltage = 0, mqVoltage = 0, ntcVoltage = 0;
float temperature = 0.0f;
static uint8_t firmwareVersion = 0;

/* Variables gaz */
float gas_ppm = 0.0f;
float gas_percent = 0.0f;

/* Constantes pour le calcul PPM */
const float RL = 10000.0;   // résistance de charge en ohms
const float VC = 5.0;       // tension d’alim capteur
const float R0 = 10000.0;   // résistance en air propre (à calibrer)
const float A = 1000.0;     // constante empirique MQ-2 (pour CH4)
const float B = 2.2;        // constante empirique MQ-2 (pour CH4)

void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Connexion Wi-Fi à %s", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\n✅ Wi-Fi connecté !");
  Serial.println(WiFi.localIP());
}

void sendToServer() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<512> doc;
  doc["firmwareVersion"] = firmwareVersion;
  doc["heatMode"] = (heatMode == HEAT_MODE_CONTINUOUS ? "CONTINUOUS" : "SWITCH");
  doc["ledStatus"] = (ledStatus == LED_WORK_STATUS_ON ? "ON" : "OFF");
  doc["validTags"] = validTags;
  doc["temperature"] = temperature;
  doc["ntcVoltage"] = ntcVoltage;
  doc["mqVoltage"] = mqVoltage;
  doc["gas_ppm"] = gas_ppm;
  doc["gas_percent"] = gas_percent;

  String jsonData;
  serializeJson(doc, jsonData);

  int httpCode = http.POST(jsonData);
  Serial.println("Trame envoyée :");
  Serial.println(jsonData);

  if (httpCode > 0)
    Serial.printf("Réponse serveur : %d\n", httpCode);
  else
    Serial.printf("Erreur HTTP : %s\n", http.errorToString(httpCode).c_str());

  http.end();
}

void setup() {
  Serial.begin(115200);
  M5.begin();

  connectWiFi();

  while (!unitMQ.begin(&Wire, UNIT_MQ_I2C_BASE_ADDR, I2C_SDA, I2C_SCL, I2C_SPEED)) {
    Serial.println("[ERREUR] Capteur non détecté, nouvelle tentative...");
    delay(1000);
  }

  unitMQ.setHeatMode(HEAT_MODE_PIN_SWITCH);
  unitMQ.setPulseTime(HIGH_LEVEL_TIME, LOW_LEVEL_TIME);
  unitMQ.setLEDState(LED_WORK_STATUS_ON);
}

void loop() {
  heatMode = unitMQ.getHeatMode();
  ledStatus = unitMQ.getLEDState();
  validTags = unitMQ.getValidTags();
  firmwareVersion = unitMQ.getFirmwareVersion();

  ntcAdc12bit = unitMQ.getNTCADC12bit();
  referenceVoltage = unitMQ.getReferenceVoltage();
  ntcVoltage = unitMQ.getNTCVoltage();
  ntcResistance = unitMQ.getNTCResistance();
  temperature = unitMQ.getNTCTemperature(ntcResistance);

  if (validTags == VALID_TAG_VALID) {
    mqAdc12bit = unitMQ.getMQADC12bit();
    mqVoltage  = unitMQ.getMQVoltage();

    // --- Calcul du PPM estimé pour CH4 ---
    float Vout = mqVoltage / 1000.0; // mv -> V
    float Rs = RL * (VC - Vout) / Vout;
    gas_ppm = A * pow((Rs / R0), -B);
    gas_percent = gas_ppm / 10000.0; // conversion ppm -> %
  }

  sendToServer();
  delay(5000);
}
