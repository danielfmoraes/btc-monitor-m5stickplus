#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

#define ALERT_BUZZER_PIN 2
#define BTN_A_BUTTON M5.BtnA
#define BTN_B_BUTTON M5.BtnB

float lastPriceUSD = 0;
float buyPrice = 0;
bool hasBought = false;
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 30000;

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

void drawBatteryIcon() {
  float voltage = M5.Power.getBatteryVoltage();
  int percent = M5.Power.getBatteryLevel();
  M5.Lcd.setCursor(130, 2);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.printf("%d%%", percent);
}

bool fetchTicker(const String &symbol, float &priceUSD, float &priceBRL) {
  HTTPClient http;
  http.begin("https://api.binance.com/api/v3/ticker/price?symbol=" + symbol + "USDT");
  if (http.GET() == 200) {
    JsonDocument doc;
    deserializeJson(doc, http.getString());
    priceUSD = doc["price"].as<float>();
  } else {
    http.end();
    return false;
  }
  http.end();

  http.begin("https://api.binance.com/api/v3/ticker/price?symbol=" + symbol + "BRL");
  if (http.GET() == 200) {
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

bool fetchKline(const String &symbol, float &variation1h) {
  HTTPClient http;
  http.begin("https://api.binance.com/api/v3/klines?symbol=" + symbol + "USDT&interval=1h&limit=2");
  if (http.GET() == 200) {
    JsonDocument doc;
    deserializeJson(doc, http.getString());
    float previous = atof(doc[0][4]);
    float current = atof(doc[1][4]);
    variation1h = ((current - previous) / previous) * 100.0;
    return true;
  }
  http.end();
  return false;
}

void drawBTCGraph() {
  HTTPClient http;
  http.begin("https://api.binance.com/api/v3/klines?symbol=BTCUSDT&interval=1h&limit=24");
  if (http.GET() == 200) {
    JsonDocument doc;
    deserializeJson(doc, http.getString());
    std::vector<float> prices;
    float minPrice = 999999.0;
    float maxPrice = 0.0;

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
    M5.Lcd.setCursor(0, 115);
    M5.Lcd.printf("$%.0f", minPrice);

    int xOffset = 30;
    int graphHeight = 90;
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
  float priceUSD = 0, priceBRL = 0, variation1h = 0;
  if (!fetchTicker("BTC", priceUSD, priceBRL)) return;
  if (!fetchKline("BTC", variation1h)) return;

  float variation24h = (lastPriceUSD > 0) ? ((priceUSD - lastPriceUSD) / lastPriceUSD) * 100.0 : 0;
  lastPriceUSD = priceUSD;

  M5.Lcd.fillScreen(BLACK);
  drawBatteryIcon();

  uint16_t priceColor = WHITE;
  if (variation24h >= 0.5) priceColor = GREEN;
  else if (variation24h <= -0.5) priceColor = RED;

  M5.Lcd.setTextColor(priceColor, BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 20);
  M5.Lcd.printf("USD: %.2f", priceUSD);
  M5.Lcd.setCursor(10, 50);
  M5.Lcd.printf("BRL: %.2f", priceBRL);

  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(10, 80);
  M5.Lcd.printf("1h: %.2f%%", variation1h);
  M5.Lcd.setCursor(10, 95);
  M5.Lcd.printf("24h: %.2f%%", variation24h);

  if (hasBought) {
    float buyVariation = ((priceUSD - buyPrice) / buyPrice) * 100.0;
    M5.Lcd.setCursor(10, 115);
    M5.Lcd.printf("Comprado: $%.2f", buyPrice);
    M5.Lcd.setCursor(10, 130);
    M5.Lcd.setTextColor((buyVariation > 0 ? GREEN : RED), BLACK);
    M5.Lcd.printf("Variacao: %.2f%%", buyVariation);
  }
}

void setup() {
  M5.begin();
  M5.Lcd.setRotation(3);  // Modo retrato
  M5.Lcd.setTextSize(1);
  pinMode(ALERT_BUZZER_PIN, OUTPUT);
  connectToWiFi();
  showPrices();
}

void loop() {
  M5.update();

  if (BTN_A_BUTTON.wasPressed()) {
    if (!hasBought) {
      buyPrice = lastPriceUSD;
      hasBought = true;
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
