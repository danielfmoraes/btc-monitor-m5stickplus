#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Inclui o parâmetro para 1h change
const String url = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd,brl&include_24hr_change=true&include_1hr_change=true";

void connectWiFi() {
  WiFi.begin(ssid, password);
  M5.Lcd.println("Conectando WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    M5.Lcd.print(".");
  }
  M5.Lcd.println("\nWiFi conectado!");
}

void fetchAndDisplayBTC() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(url);
    http.setTimeout(10000); // Timeout de 10 segundos
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);

      float usd = doc["bitcoin"]["usd"];
      float brl = doc["bitcoin"]["brl"];
      float change24h = doc["bitcoin"]["usd_24h_change"];
      float change1h = doc["bitcoin"]["usd_1h_change"];

      // Define cores separadas para 1h e 24h
      uint16_t color24h = WHITE;
      if (change24h > 5) color24h = BLUE;
      else if (change24h < -5) color24h = RED;

      uint16_t color1h = WHITE;
      if (change1h > 5) color1h = BLUE;
      else if (change1h < -5) color1h = RED;

      // Atualiza o display
      M5.Lcd.fillScreen(BLACK);

      // Título
      M5.Lcd.setTextSize(1);
      M5.Lcd.setTextColor(WHITE);
      M5.Lcd.setCursor(10, 5);
      M5.Lcd.println("BTC Price");

      // Valores USD e BRL
      M5.Lcd.setTextSize(2);
      M5.Lcd.setTextColor(WHITE);
      M5.Lcd.setCursor(10, 20);
      M5.Lcd.printf("USD: $%.2f", usd);
      M5.Lcd.setCursor(10, 45);
      M5.Lcd.printf("BRL: R$%.2f", brl);

      // Variação de 1h e 24h com fonte menor
      M5.Lcd.setTextSize(1);
      M5.Lcd.setCursor(10, 70);
      M5.Lcd.setTextColor(color1h);
      M5.Lcd.printf("1h: %.2f%%", change1h);
      M5.Lcd.setCursor(10, 85);
      M5.Lcd.setTextColor(color24h);
      M5.Lcd.printf("24h: %.2f%%", change24h);

    } else {
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setTextSize(1);
      M5.Lcd.setTextColor(RED);
      M5.Lcd.setCursor(10, 10);
      M5.Lcd.println("Erro na requisicao.");
      M5.Lcd.printf("Codigo: %d", httpCode);
    }

    http.end();
  } else {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(RED);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.println("Sem WiFi.");
  }
}

void setup() {
  M5.begin();
  M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(1);
  connectWiFi();
}

void loop() {
  fetchAndDisplayBTC();
  delay(30000); // Atualiza a cada 300 segundos para evitar erro 429
}
