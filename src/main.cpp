#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Endpoints para Binance - USD
const String tickerUSDUrl = "https://api.binance.com/api/v3/ticker/24hr?symbol=BTCUSDT";
const String klineUSDUrl  = "https://api.binance.com/api/v3/klines?symbol=BTCUSDT&interval=1h&limit=1";

// Endpoints para Binance - BRL (verifique se esse par está disponível)
const String tickerBRLUrl = "https://api.binance.com/api/v3/ticker/24hr?symbol=BTCBRL";
const String klineBRLUrl  = "https://api.binance.com/api/v3/klines?symbol=BTCBRL&interval=1h&limit=1";

// Variáveis globais para registrar a compra
float purchasePrice = -1.0;   // Se for -1, nenhuma compra foi registrada
float lastPriceUSD = 0.0;       // Guarda o último preço USD obtido

unsigned long previousMillis = 0;
const long interval = 30000;    // Atualiza a cada 30 segundos

// Variáveis para mensagem de confirmação não bloqueante
unsigned long purchaseMsgTime = 0;
bool showPurchaseMsg = false;

void connectWiFi() {
  WiFi.begin(ssid, password);
  StickCP2.Lcd.println("Conectando WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    StickCP2.Lcd.print(".");
  }
  StickCP2.Lcd.println("\nWiFi conectado!");
}

bool fetchTicker(const String &url, float &lastPrice, float &change24h) {
  HTTPClient http;
  http.begin(url);
  http.setTimeout(10000);
  int httpCode = http.GET();
  if(httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    lastPrice = atof(doc["lastPrice"]);
    change24h = atof(doc["priceChangePercent"]);
    http.end();
    return true;
  } else {
    http.end();
    return false;
  }
}

bool fetchKline(const String &url, float &openPrice) {
  HTTPClient http;
  http.begin(url);
  http.setTimeout(10000);
  int httpCode = http.GET();
  if(httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    if(doc.is<JsonArray>()){
      JsonArray arr = doc.as<JsonArray>();
      if(arr.size() > 0){
        JsonArray firstCandle = arr[0].as<JsonArray>();
        openPrice = atof(firstCandle[1]);
      }
    }
    http.end();
    return true;
  } else {
    http.end();
    return false;
  }
}

void fetchAndDisplayBTC() {
  float usdPrice = 0.0, change24hUSD = 0.0, openPriceUSD = 0.0, change1hUSD = 0.0;
  float brlPrice = 0.0, change24hBRL = 0.0, openPriceBRL = 0.0, change1hBRL = 0.0;
  
  bool tickerUSDFetched = fetchTicker(tickerUSDUrl, usdPrice, change24hUSD);
  bool klineUSDFetched = fetchKline(klineUSDUrl, openPriceUSD);
  if(klineUSDFetched && openPriceUSD > 0) {
    change1hUSD = ((usdPrice - openPriceUSD) / openPriceUSD) * 100.0;
  }
  
  bool tickerBRLFetched = fetchTicker(tickerBRLUrl, brlPrice, change24hBRL);
  bool klineBRLFetched = fetchKline(klineBRLUrl, openPriceBRL);
  if(klineBRLFetched && openPriceBRL > 0) {
    change1hBRL = ((brlPrice - openPriceBRL) / openPriceBRL) * 100.0;
  }
  
  lastPriceUSD = usdPrice; // Atualiza o último preço USD

  // Define cores com base nas variações
  uint16_t color1hUSD = (change1hUSD >= 0 ? GREEN : RED);
  uint16_t color24hUSD = (change24hUSD >= 0 ? GREEN : RED);
  uint16_t priceColorUSD = (change24hUSD >= 0 ? GREEN : RED);
  
  uint16_t color1hBRL = (change1hBRL >= 0 ? GREEN : RED);
  uint16_t color24hBRL = (change24hBRL >= 0 ? GREEN : RED);
  uint16_t priceColorBRL = (change24hBRL >= 0 ? GREEN : RED);
  
  // Se houve compra registrada, calcula a variação
  bool showPurchase = (purchasePrice > 0);
  float purchaseVariation = 0.0;
  uint16_t purchaseColor = WHITE;
  if(showPurchase) {
    purchaseVariation = ((usdPrice - purchasePrice) / purchasePrice) * 100.0;
    purchaseColor = (purchaseVariation >= 0 ? GREEN : RED);
  }
  
  // Atualiza o display
  StickCP2.Lcd.fillScreen(BLACK);
  
  // Título
  StickCP2.Lcd.setTextSize(1);
  StickCP2.Lcd.setTextColor(WHITE);
  StickCP2.Lcd.setCursor(10, 0);
  StickCP2.Lcd.println("BTC (Binance)");
  
  // Preços
  StickCP2.Lcd.setTextSize(2);
  StickCP2.Lcd.setTextColor(priceColorUSD);
  StickCP2.Lcd.setCursor(10, 10);
  StickCP2.Lcd.printf("USD: $%.2f", usdPrice);
  
  StickCP2.Lcd.setTextSize(2);
  StickCP2.Lcd.setTextColor(priceColorBRL);
  StickCP2.Lcd.setCursor(10, 35);
  StickCP2.Lcd.printf("BRL: R$%.2f", brlPrice);
  
  // Variações: exibe 1h e 24h lado a lado
  StickCP2.Lcd.setTextSize(1);
  // Linha de 1h
  StickCP2.Lcd.setCursor(10, 60);
  StickCP2.Lcd.setTextColor(color1hUSD);
  StickCP2.Lcd.printf("1h USD: %.2f%%", change1hUSD);
  StickCP2.Lcd.setCursor(80, 60);
  StickCP2.Lcd.setTextColor(color1hBRL);
  StickCP2.Lcd.printf("1h BRL: %.2f%%", change1hBRL);
  // Linha de 24h
  StickCP2.Lcd.setCursor(10, 75);
  StickCP2.Lcd.setTextColor(color24hUSD);
  StickCP2.Lcd.printf("24h USD: %.2f%%", change24hUSD);
  StickCP2.Lcd.setCursor(80, 75);
  StickCP2.Lcd.setTextColor(color24hBRL);
  StickCP2.Lcd.printf("24h BRL: %.2f%%", change24hBRL);
  
  // Exibe dados de compra se registrados
  if(showPurchase) {
    StickCP2.Lcd.setTextSize(1);
    StickCP2.Lcd.setTextColor(WHITE);
    StickCP2.Lcd.setCursor(10, 90);
    StickCP2.Lcd.printf("Comprado: $%.2f", purchasePrice);
    StickCP2.Lcd.setCursor(10, 105);
    StickCP2.Lcd.setTextColor(purchaseColor);
    StickCP2.Lcd.printf("Var: %.2f%%", purchaseVariation);
  }
}

void setup() {
  Serial.begin(115200);
  StickCP2.begin();
  StickCP2.Lcd.setRotation(1);
  StickCP2.Lcd.fillScreen(BLACK);
  StickCP2.Lcd.setTextSize(1);
  connectWiFi();
}

void loop() {
  StickCP2.update();
  
  // Se o botão A for pressionado, registra o preço atual (USD) como compra e inicia a mensagem de confirmação
  if (StickCP2.BtnA.wasPressed()) {
    purchasePrice = lastPriceUSD;
    Serial.printf("Botão A pressionado. Preço registrado: $%.2f\n", purchasePrice);
    purchaseMsgTime = millis();
    showPurchaseMsg = true;
  }
  
  // Exibe a mensagem de confirmação por 2 segundos e impede atualização do display nesse período
  if (showPurchaseMsg) {
    StickCP2.Lcd.fillRect(0, 145, 160, 16, BLACK);
    StickCP2.Lcd.setTextSize(1);
    StickCP2.Lcd.setTextColor(WHITE);
    StickCP2.Lcd.setCursor(10, 145);
    StickCP2.Lcd.printf("Comprado: $%.2f", purchasePrice);
    if (millis() - purchaseMsgTime >= 2000) {
      showPurchaseMsg = false;
    }
  }
  
  // Atualiza o display a cada 30 segundos, desde que não esteja exibindo a mensagem de compra
  unsigned long currentMillis = millis();
  if (!showPurchaseMsg && (currentMillis - previousMillis >= interval)) {
    previousMillis = currentMillis;
    fetchAndDisplayBTC();
  }
}
