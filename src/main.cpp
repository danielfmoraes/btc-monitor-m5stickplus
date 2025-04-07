#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include <float.h>


#define ALERT_BUZZER_PIN 2
#define BTN_A_BUTTON M5.BtnA
#define BTN_B_BUTTON M5.BtnB

float lastPriceUSD = 0;
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 30000;

float lastPurchaseUSD = 0;

void connectToWiFi() {
  M5.Lcd.setCursor(10, 20);
  M5.Lcd.print("Conectando ao WiFi...");

  for (int i = 0; i < wifiNetworkCount; ++i) {
    WiFi.begin(wifiNetworks[i].ssid, wifiNetworks[i].password);
    if (WiFi.waitForConnectResult() == WL_CONNECTED) {
      M5.Lcd.setCursor(10, 40);
      M5.Lcd.printf("Conectado a %s", wifiNetworks[i].ssid);
      return;
    }
  }

  M5.Lcd.setCursor(10, 60);
  M5.Lcd.println("Falha no WiFi.");
}

bool fetchTicker(const String &symbol, float &priceUSD, float &priceBRL) {
  HTTPClient http;
  http.begin("https://api.binance.com/api/v3/ticker/price?symbol=" + symbol + "USDT");
  int httpCode = http.GET();
  if (httpCode == 200) {
    JsonDocument doc;
    deserializeJson(doc, http.getString());
    priceUSD = doc["price"].as<float>();
  } else {
    http.end();
    return false;
  }
  http.end();

  http.begin("https://api.binance.com/api/v3/ticker/price?symbol=" + symbol + "BRL");
  httpCode = http.GET();
  if (httpCode == 200) {
    JsonDocument doc;
    deserializeJson(doc, http.getString());
    priceBRL = doc["price"].as<float>();
  } else {
    http.end();
    return false;
  }
  http.end();

  return true;
}

bool fetchKline(const String &symbol, float &variation1h, float &variation24h) {
  HTTPClient http;
  http.begin("https://api.binance.com/api/v3/klines?symbol=" + symbol + "USDT&interval=1h&limit=25");
  int httpCode = http.GET();
  if (httpCode == 200) {
    JsonDocument doc;
    deserializeJson(doc, http.getString());

    float price1hAgo = atof(doc[23][4]);
    float priceNow = atof(doc[24][4]);
    variation1h = ((priceNow - price1hAgo) / price1hAgo) * 100.0;

    float price24hAgo = atof(doc[0][4]);
    variation24h = ((priceNow - price24hAgo) / price24hAgo) * 100.0;
    return true;
  }
  http.end();
  return false;
}

void drawBatteryIcon() {
  int level = M5.Power.getBatteryLevel();
  M5.Lcd.setCursor(190, 5);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(1);
  M5.Lcd.printf("%d%%", level);
}

void drawBTCGraph() {
  HTTPClient http;
  http.begin("https://api.binance.com/api/v3/klines?symbol=BTCUSDT&interval=1h&limit=24");
  int httpCode = http.GET();
  if (httpCode == 200) {
    JsonDocument doc;
    deserializeJson(doc, http.getString());

    std::vector<float> prices;
    float minPrice = FLT_MAX;
    float maxPrice = 0;

    for (JsonArray candle : doc.as<JsonArray>()) {
      float closePrice = atof(candle[4]);
      prices.push_back(closePrice);
      if (closePrice < minPrice) minPrice = closePrice;
      if (closePrice > maxPrice) maxPrice = closePrice;
    }

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(5, 0);
    M5.Lcd.setTextSize(1);
    M5.Lcd.print("Grafico BTC/USD - 24h");

    M5.Lcd.setCursor(0, 20);
    M5.Lcd.printf("$%.0f", maxPrice);
    M5.Lcd.setCursor(0, 95);
    M5.Lcd.printf("$%.0f", minPrice);

    int xOffset = 30;
    int graphHeight = 80;
    int graphWidth = 100;
    int xStep = graphWidth / prices.size();
    int yStart = 20 + graphHeight;

    for (size_t i = 1; i < prices.size(); ++i) {
      int x0 = xOffset + (i - 1) * xStep;
      int y0 = yStart - ((prices[i - 1] - minPrice) / (maxPrice - minPrice)) * graphHeight;
      int x1 = xOffset + i * xStep;
      int y1 = yStart - ((prices[i] - minPrice) / (maxPrice - minPrice)) * graphHeight;
      M5.Lcd.drawLine(x0, y0, x1, y1, YELLOW);
    }
  }
  http.end();
}

void showPrices() {
  float priceUSD = 0, priceBRL = 0, variation1h = 0, variation24h = 0;
  if (!fetchTicker("BTC", priceUSD, priceBRL)) return;
  if (!fetchKline("BTC", variation1h, variation24h)) return;

  M5.Lcd.fillScreen(BLACK);
  drawBatteryIcon();

  // Definir cor com base na variação 24h
  if (variation24h > 5) {
    M5.Lcd.setTextColor(GREEN, BLACK);
    tone(ALERT_BUZZER_PIN, 1000, 200);
  } else if (variation24h < -5) {
    M5.Lcd.setTextColor(RED, BLACK);
    tone(ALERT_BUZZER_PIN, 500, 200);
  } else {
    M5.Lcd.setTextColor(WHITE, BLACK);
  }

  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 5);
  M5.Lcd.printf("USD: %.2f", priceUSD);
  M5.Lcd.setCursor(0, 30);
  M5.Lcd.printf("BRL: %.2f", priceBRL);

  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(0, 55);
  M5.Lcd.printf("1h: %.2f%%", variation1h);
  M5.Lcd.setCursor(0, 70);
  M5.Lcd.printf("24h: %.2f%%", variation24h);

  // Mostra a última compra (se houver)
  if (lastPurchaseUSD > 0) {
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.setCursor(0, 90);
    M5.Lcd.printf("Compra: $%.2f", lastPurchaseUSD);
  }
}

void setup() {
  M5.begin();
  M5.Lcd.setRotation(3);
  M5.Lcd.setTextSize(1);
  pinMode(ALERT_BUZZER_PIN, OUTPUT);
  connectToWiFi();
  showPrices();
}

void loop() {
  M5.update();

  if (BTN_A_BUTTON.wasPressed()) {
    float priceUSD = 0, priceBRL = 0;
    if (fetchTicker("BTC", priceUSD, priceBRL)) {
      lastPurchaseUSD = priceUSD;

      M5.Lcd.fillScreen(BLACK);
      drawBatteryIcon();
      M5.Lcd.setTextColor(GREEN, BLACK);
      M5.Lcd.setCursor(0, 30);
      M5.Lcd.setTextSize(2);
      M5.Lcd.printf("Compra!\n$%.2f", lastPurchaseUSD);
      delay(1500);
    }
    showPrices();
  }

  if (BTN_B_BUTTON.wasPressed()) {
    drawBTCGraph();
    delay(5000);
    showPrices();
  }

  if (millis() - lastUpdateTime >= updateInterval) {
    showPrices();
    lastUpdateTime = millis();
  }
}
