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

// --- Função WiFi ---
void connectToWiFi() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(10, 20);
  M5.Lcd.print("Conectando WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED && counter < 20) {
    delay(500);
    M5.Lcd.print(".");
    counter++;
  }
  M5.Lcd.setCursor(10, 40);
  if(WiFi.status() == WL_CONNECTED) M5.Lcd.printf("Conectado: %s", WIFI_SSID);
  else M5.Lcd.println("Falha no WiFi");
}

// --- Buscar preço ---
bool fetchTicker(const String &symbol, float &priceUSD, float &priceBRL) {
  HTTPClient http;
  http.begin("https://api.binance.com/api/v3/ticker/price?symbol=" + symbol + "USDT");
  int httpCode = http.GET();
  if (httpCode == 200) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getString());
    priceUSD = doc["price"].as<float>();
  } else { http.end(); return false; }
  http.end();

  http.begin("https://api.binance.com/api/v3/ticker/price?symbol=" + symbol + "BRL");
  httpCode = http.GET();
  if (httpCode == 200) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, http.getString());
    priceBRL = doc["price"].as<float>();
  } else { http.end(); return false; }
  http.end();
  return true;
}

// --- Variação 1h ---
bool fetchKline(const String &symbol, float &variation1h) {
  HTTPClient http;
  http.begin("https://api.binance.com/api/v3/klines?symbol=" + symbol + "USDT&interval=1h&limit=2");
  int httpCode = http.GET();
  if (httpCode == 200) {
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, http.getString());
    float previous = atof(doc[0][4]);
    float current = atof(doc[1][4]);
    variation1h = ((current - previous) / previous) * 100.0;
    http.end();
    return true;
  }
  http.end();
  return false;
}

// --- Formatar em K ---
String formatK(float value) {
  if (value >= 1000) return String(value / 1000.0, 0) + "K";
  return String(value, 0);
}

// --- Desenhar gráfico mini ---
void drawMiniGraph(float prices[], int size, int x, int y, int w, int h) {
  float minP = 999999;
  float maxP = 0;
  for (int i = 0; i < size; i++) {
    if (prices[i] < minP) minP = prices[i];
    if (prices[i] > maxP) maxP = prices[i];
  }
  int stepX = w / size;
  for (int i = 1; i < size; i++) {
    int x0 = x + (i - 1) * stepX;
    int y0 = y + h - ((prices[i - 1] - minP) / (maxP - minP)) * h;
    int x1 = x + i * stepX;
    int y1 = y + h - ((prices[i] - minP) / (maxP - minP)) * h;
    M5.Lcd.drawLine(x0, y0, x1, y1, YELLOW);
  }
}

// --- Mostrar preços e gráfico compacto ---
void showPricesCompact() {
  float priceUSD = 0, priceBRL = 0, variation1h = 0;
  if (!fetchTicker("BTC", priceUSD, priceBRL)) return;
  if (!fetchKline("BTC", variation1h)) return;
  float variation24h = lastPriceUSD > 0 ? ((priceUSD - lastPriceUSD) / lastPriceUSD) * 100.0 : 0;
  lastPriceUSD = priceUSD;

  M5.Lcd.fillScreen(BLACK);
  // Topo: bateria e título
  float voltage = M5.Power.getBatteryVoltage();
  int batPct = map(voltage*1000, 3300, 4200, 0, 100);
  batPct = constrain(batPct,0,100);
  M5.Lcd.setCursor(5,0); M5.Lcd.setTextColor(WHITE); M5.Lcd.setTextSize(1);
  M5.Lcd.printf("Bat:%d%%", batPct);
  M5.Lcd.setCursor(60,0); M5.Lcd.print("BTC");

  // Valores
  M5.Lcd.setCursor(10,20); M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(variation24h>=0?GREEN:(variation24h<=-0.5?RED:WHITE), BLACK);
  M5.Lcd.printf("USD: %s", formatK(priceUSD).c_str());

  M5.Lcd.setCursor(10,50); M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.printf("BRL: %s", formatK(priceBRL).c_str());

  // Variações
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(10,80);
  M5.Lcd.setTextColor(variation1h>=0?GREEN:RED, BLACK);
  M5.Lcd.printf("1h: %+0.2f%%", variation1h);
  M5.Lcd.setCursor(70,80);
  M5.Lcd.setTextColor(variation24h>=0?GREEN:RED, BLACK);
  M5.Lcd.printf("24h: %+0.2f%%", variation24h);

  // Entrada
  if(purchasePriceUSD>0){
    float varEntry = ((priceUSD-purchasePriceUSD)/purchasePriceUSD)*100.0;
    M5.Lcd.setCursor(10,100); M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.printf("Entrada: %s", formatK(purchasePriceUSD).c_str());
    M5.Lcd.setCursor(10,115);
    M5.Lcd.setTextColor(varEntry>=0?GREEN:RED,BLACK);
    M5.Lcd.printf("Varia: %+0.2f%%", varEntry);
  }

  // Gráfico mini
  float prices[24];
  HTTPClient http;
  http.begin("https://api.binance.com/api/v3/klines?symbol=BTCUSDT&interval=1h&limit=24");
  int httpCode = http.GET();
  if(httpCode==200){
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, http.getString());
    for(int i=0;i<24;i++){
      prices[i] = atof(doc[i][4]);
    }
  }
  http.end();
  drawMiniGraph(prices,24,5,135,110,40);

  // Alertas
  if(variation1h>5||variation24h>5) tone(ALERT_BUZZER_PIN,1000,200);
  else if(variation1h<-5||variation24h<-5) tone(ALERT_BUZZER_PIN,500,200);
}

// --- Setup ---
void setup(){
  M5.begin();
  M5.Lcd.setRotation(0);
  pinMode(ALERT_BUZZER_PIN, OUTPUT);
  connectToWiFi();
  showPricesCompact();
}

// --- Loop ---
void loop(){
  M5.update();
  if(BTN_A_BUTTON.wasPressed()){
    float priceUSD=0, priceBRL=0;
    if(fetchTicker("BTC", priceUSD, priceBRL)){
      purchasePriceUSD=priceUSD;
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(10,60);
      M5.Lcd.setTextColor(GREEN,BLACK);
      M5.Lcd.setTextSize(2);
      M5.Lcd.printf("Compra: %s", formatK(purchasePriceUSD).c_str());
      delay(1500);
    }
  }
  if(BTN_B_BUTTON.wasPressed()){
    showPricesCompact(); // atualiza gráfico e valores juntos
  }
  if(millis()-lastUpdateTime>=updateInterval){
    showPricesCompact();
    lastUpdateTime=millis();
  }
}