#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Endpoints para obter dados de BTC usando o endpoint /coins/markets
const String urlUSD = "https://api.coingecko.com/api/v3/coins/markets?vs_currency=usd&ids=bitcoin&price_change_percentage=1h,24h";
const String urlBRL = "https://api.coingecko.com/api/v3/coins/markets?vs_currency=brl&ids=bitcoin&price_change_percentage=1h,24h";

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
    
    // Requisição para dados em USD
    http.begin(urlUSD);
    http.setTimeout(10000); // 10 segundos de timeout
    int httpCodeUSD = http.GET();

    if (httpCodeUSD == 200) {
      String payloadUSD = http.getString();
      DynamicJsonDocument docUSD(2048);
      deserializeJson(docUSD, payloadUSD);
      // O resultado é um array, pegamos o primeiro objeto
      JsonObject btcUSD = docUSD[0];
      float usd = btcUSD["current_price"];
      float change1h_usd = btcUSD["price_change_percentage_1h_in_currency"];
      float change24h_usd = btcUSD["price_change_percentage_24h_in_currency"];
      http.end();
      
      // Requisição para dados em BRL
      http.begin(urlBRL);
      http.setTimeout(10000);
      int httpCodeBRL = http.GET();
      if (httpCodeBRL == 200) {
        String payloadBRL = http.getString();
        DynamicJsonDocument docBRL(2048);
        deserializeJson(docBRL, payloadBRL);
        JsonObject btcBRL = docBRL[0];
        float brl = btcBRL["current_price"];
        http.end();
        
        // Define as cores com base nas variações (usando os dados em USD)
        uint16_t color1h = WHITE;
        if (change1h_usd > 5) color1h = BLUE;
        else if (change1h_usd < -5) color1h = RED;
        
        uint16_t color24h = WHITE;
        if (change24h_usd > 5) color24h = BLUE;
        else if (change24h_usd < -5) color24h = RED;
        
        // Atualiza o display
        M5.Lcd.fillScreen(BLACK);
        
        // Título
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(WHITE);
        M5.Lcd.setCursor(10, 0);
        M5.Lcd.println("BTC Price");
        
        // Preços
        M5.Lcd.setTextSize(2);
        M5.Lcd.setCursor(10, 10);
        M5.Lcd.printf("USD: $%.2f", usd);
        M5.Lcd.setCursor(10, 35);
        M5.Lcd.printf("BRL: R$%.2f", brl);
        
        // Variações de 1h e 24h com fonte menor
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(10, 60);
        M5.Lcd.setTextColor(color1h);
        M5.Lcd.printf("1h: %.2f%%", change1h_usd);
        M5.Lcd.setCursor(10, 75);
        M5.Lcd.setTextColor(color24h);
        M5.Lcd.printf("24h: %.2f%%", change24h_usd);
      } else {
        http.end();
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(RED);
        M5.Lcd.setCursor(10, 10);
        M5.Lcd.println("Erro na requisicao BRL.");
        M5.Lcd.printf("Codigo: %d", httpCodeBRL);
      }
    } else {
      http.end();
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setTextSize(1);
      M5.Lcd.setTextColor(RED);
      M5.Lcd.setCursor(10, 10);
      M5.Lcd.println("Erro na requisicao USD.");
      M5.Lcd.printf("Codigo: %d", httpCodeUSD);
    }
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
  delay(10000); // Atualiza a cada 10 segundos
}
