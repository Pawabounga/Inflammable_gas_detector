#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

/* ====== À PERSONNALISER ====== */
const char* WIFI_SSID     = "Topinambour";
const char* WIFI_PASSWORD = "abcdefghi";
/* ============================== */

#define PIN_COMMUT 5

// Si la carte "M5Stack Tab5" est bien sélectionnée, ces constantes existent.
// Elles pointent vers les lignes SDIO qui relient le P4 au C6 (Wi-Fi).
#ifndef BOARD_SDIO_ESP_HOSTED_CLK
  // Valeurs de secours si la carte n'est pas bien sélectionnée
  #define SDIO2_CLK GPIO_NUM_12
  #define SDIO2_CMD GPIO_NUM_13
  #define SDIO2_D0  GPIO_NUM_11
  #define SDIO2_D1  GPIO_NUM_10
  #define SDIO2_D2  GPIO_NUM_9
  #define SDIO2_D3  GPIO_NUM_8
  #define SDIO2_RST GPIO_NUM_15
#endif

// Serveur HTTP (on garde le port 5000 pour correspondre au client)
WebServer server(5000);

// Variables pour stocker les dernières données reçues
String firmwareVersion = "-";
String heatMode        = "-";
String ledStatus       = "-";
int validTags          = -1;
float temperature      = NAN;
float ntcVoltage       = NAN;
float mqVoltage        = NAN;
float gas_ppm          = NAN;
float gas_percent      = NAN;

void showInitialScreen() {
  M5.Display.setTextSize(2);
  M5.Display.clear();
  M5.Display.println("Tab5 Wi-Fi -> STA");
  M5.Display.println("");
}

void displayStatus(const char* line1 = nullptr) {
  M5.Display.setTextSize(2);

  // Si le texte a dépassé la hauteur de l’écran, on efface et on repart en haut
  if (M5.Display.getCursorY() > M5.Display.height() - 40) {
    M5.Display.clear();
    M5.Display.setCursor(0, 0);
  }

  // Réinitialisation d’affichage (toujours utile après réception)
  M5.Display.clear();
  M5.Display.setCursor(0, 0);

  if (line1) M5.Display.println(line1);
  M5.Display.println("---------------------");
  M5.Display.printf("IP : %s\n", WiFi.localIP().toString().c_str());
  M5.Display.printf("FW : %s\n", firmwareVersion.c_str());
  M5.Display.printf("Heat : %s\n", heatMode.c_str());
  M5.Display.printf("LED : %s\n", ledStatus.c_str());
  M5.Display.printf("Tags : %d\n", validTags);
  if (!isnan(temperature)) M5.Display.printf("T : %.2f C\n", temperature);
  if (!isnan(ntcVoltage))  M5.Display.printf("NTC : %.3f V\n", ntcVoltage);
  if (!isnan(mqVoltage))   M5.Display.printf("MQ : %.3f V\n", mqVoltage);
  if (!isnan(gas_ppm))     M5.Display.printf("Gaz : %.2f ppm\n", gas_ppm);
  if (!isnan(gas_percent)) M5.Display.printf(" -> %.4f %%\n", gas_percent);
}


void handlePostData() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Aucune donnee");
    return;
  }

  String body = server.arg("plain");
  Serial.println(">> Donnees recues :");
  Serial.println(body);

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.print("Erreur JSON: ");
    Serial.println(err.c_str());
    server.send(400, "text/plain", "Erreur JSON");
    return;
  }

  // Lecture des champs JSON envoyés par le client (ESP32 + Unit MQ)
  firmwareVersion = String((int)doc["firmwareVersion"]);
  const char* hm = doc["heatMode"] | "-";
  const char* ls = doc["ledStatus"] | "-";
  heatMode = String(hm);
  ledStatus = String(ls);
  validTags   = doc["validTags"] | -1;
  temperature = doc["temperature"] | NAN;
  ntcVoltage  = doc["ntcVoltage"] | NAN;
  mqVoltage   = doc["mqVoltage"] | NAN;

  // 🔹 Nouvelles valeurs de gaz (ppm et %)
  gas_ppm     = doc["gas_ppm"] | NAN;
  gas_percent = doc["gas_percent"] | NAN;

  // Log série complet
  Serial.println("------ Donnees decodees ------");
  Serial.printf("FW Version : %s\n", firmwareVersion.c_str());
  Serial.printf("Mode Chauffe : %s\n", heatMode.c_str());
  Serial.printf("LED : %s\n", ledStatus.c_str());
  Serial.printf("Tags Valides : %d\n", validTags);
  Serial.printf("Temperature : %.2f C\n", temperature);
  Serial.printf("NTC Voltage : %.3f V\n", ntcVoltage);
  Serial.printf("MQ Voltage  : %.3f V\n", mqVoltage);
  Serial.printf("Gaz : %.2f ppm (%.4f %%)\n", gas_ppm, gas_percent);
  Serial.println("-------------------------------");

  if(gas_ppm > 300){
    digitalWrite(PIN_COMMUT, HIGH);
  }
  else{
    digitalWrite(PIN_COMMUT, LOW);
  }
  
    // Répondre OK et mettre à jour l’affichage
  server.send(200, "text/plain", "OK");
  displayStatus("Donnees recues");
}

void setup() {
  pinMode(PIN_COMMUT, OUTPUT);

  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);

  M5.Display.setTextSize(2);
  M5.Display.clear();
  M5.Display.println("Tab5 Wi-Fi -> STA");

#ifdef BOARD_SDIO_ESP_HOSTED_CLK
  WiFi.setPins(BOARD_SDIO_ESP_HOSTED_CLK, BOARD_SDIO_ESP_HOSTED_CMD,
               BOARD_SDIO_ESP_HOSTED_D0,  BOARD_SDIO_ESP_HOSTED_D1,
               BOARD_SDIO_ESP_HOSTED_D2,  BOARD_SDIO_ESP_HOSTED_D3,
               BOARD_SDIO_ESP_HOSTED_RESET);
#else
  WiFi.setPins(SDIO2_CLK, SDIO2_CMD, SDIO2_D0, SDIO2_D1,
               SDIO2_D2, SDIO2_D3, SDIO2_RST);
#endif

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  M5.Display.print("Connexion a ");
  M5.Display.println(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    M5.Display.print(".");
    Serial.print(".");
  }

  M5.Display.println("\n Connecte !");
  M5.Display.print("IP: ");   M5.Display.println(WiFi.localIP());
  M5.Display.print("RSSI: "); M5.Display.println(WiFi.RSSI());
  Serial.println("\nConnecte au hotspot.");
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  server.on("/data", HTTP_POST, handlePostData);
  server.begin();
  Serial.println("Serveur HTTP demarre sur le port 5000");
  displayStatus("Serveur HTTP OK");
}

void loop() {
  server.handleClient();
  delay(10);
}
