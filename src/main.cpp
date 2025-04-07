#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

#define ALERT_BUZZER_PIN 2
#define BTN_A_BUTTON M5.BtnA
#define BTN_B_BUTTON M5.BtnB

float lastPriceUSD = 0;
float purchasePriceUSD = -1;
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

bool fetchKline(const String &symbol, float &variation1h) {
  HTTPClient http;
  http.begin("https://api.binance.com/api/v3/klines?symbol=" + symbol + "USDT&interval=1h&limit=2");
  int httpCode = http.GET();
  if (httpCode == 200) {
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
  int httpCode = http.GET();
  if (httpCode == 200) {
    JsonDocument doc;
    deserializeJson(doc, http.getString());
    std::vector<float> prices;
    float minPrice = 999999.0;
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

void drawBatteryIcon() {
  float voltage = M5.Power.getBatteryVoltage();
  int percent = map(voltage * 1000, 3300, 4200, 0, 100);
  if (percent > 100) percent = 100;
  if (percent < 0) percent = 0;

  M5.Lcd.setCursor(10, 5);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(1);
  M5.Lcd.printf("Bat: %d%%", percent);

  M5.Lcd.setCursor(90, 5);
  M5.Lcd.print("BTC (Binance)");
}

String formatK(float value) {
  if (value >= 1000) return String(value / 1000.0, 1) + "k";
  return String(value, 2);
}

void showPrices() {
  float priceUSD = 0, priceBRL = 0, variation1h = 0;
  if (!fetchTicker("BTC", priceUSD, priceBRL)) return;
  if (!fetchKline("BTC", variation1h)) return;
  float variation24h = ((priceUSD - lastPriceUSD) / lastPriceUSD) * 100.0;
  if (lastPriceUSD == 0) variation24h = 0;
  lastPriceUSD = priceUSD;

  M5.Lcd.fillScreen(BLACK);
  drawBatteryIcon();

  // Preço atual
  M5.Lcd.setCursor(10, 25);
  M5.Lcd.setTextSize(2);
  if (variation24h >= 0.5) M5.Lcd.setTextColor(GREEN, BLACK);
  else if (variation24h <= -0.5) M5.Lcd.setTextColor(RED, BLACK);
  else M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.printf("USD: %.2f", priceUSD);

  // Preço em BRL
  M5.Lcd.setCursor(10, 50);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.printf("BRL: %.2f", priceBRL);

  // Variação 1h
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(10, 75);
  if (variation1h >= 0) {
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.printf("1h: +%.2f%%", variation1h);
  } else {
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.printf("1h: %.2f%%", variation1h);
  }

  // Variação 24h
  M5.Lcd.setCursor(70, 75);
  if (variation24h >= 0) {
    M5.Lcd.setTextColor(GREEN, BLACK);
    M5.Lcd.printf("24h: +%.2f%%", variation24h);
  } else {
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.printf("24h: %.2f%%", variation24h);
  }

  // Preço de compra reduzido + variação desde a compra
  if (purchasePriceUSD > 0) {
    float variationSincePurchase = ((priceUSD - purchasePriceUSD) / purchasePriceUSD) * 100.0;

    M5.Lcd.setCursor(10, 95);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.printf("Entrada: %s", formatK(purchasePriceUSD).c_str());

    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(10, 115);
    if (variationSincePurchase >= 0) {
      M5.Lcd.setTextColor(GREEN, BLACK);
      M5.Lcd.printf("Varia: +%.2f%%", variationSincePurchase);
    } else {
      M5.Lcd.setTextColor(RED, BLACK);
      M5.Lcd.printf("Varia: %.2f%%", variationSincePurchase);
    }
  }

  // Alerta sonoro
  if (variation1h > 5 || variation24h > 5) {
    tone(ALERT_BUZZER_PIN, 1000, 200);
  } else if (variation1h < -5 || variation24h < -5) {
    tone(ALERT_BUZZER_PIN, 500, 200);
  }
}

void setup() {
  M5.begin();
  M5.Lcd.setRotation(3); // vertical
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
      purchasePriceUSD = priceUSD;
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(10, 40);
      M5.Lcd.setTextColor(GREEN, BLACK);
      M5.Lcd.setTextSize(2);
      M5.Lcd.printf("Compra: %.2f", purchasePriceUSD);
      delay(1500);
    }
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
