#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Endpoints para dados de BTC (USD e BRL) com variação de 1h e 24h
const String urlUSD = "https://api.coingecko.com/api/v3/coins/markets?vs_currency=usd&ids=bitcoin&price_change_percentage=1h,24h";
const String urlBRL = "https://api.coingecko.com/api/v3/coins/markets?vs_currency=brl&ids=bitcoin&price_change_percentage=1h,24h";

// Variáveis globais para registrar a compra
float purchasePrice = -1.0;   // Se for -1, ainda não foi registrada nenhuma compra
float lastUsdPrice = 0.0;       // Guarda o último preço em USD obtido

unsigned long previousMillis = 0;
const long interval = 30000;    // 30 segundos

void connectWiFi() {
  WiFi.begin(ssid, password);
  StickCP2.Lcd.println("Conectando WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    StickCP2.Lcd.print(".");
  }
  StickCP2.Lcd.println("\nWiFi conectado!");
}

void fetchAndDisplayBTC() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // Requisição para dados em USD
    http.begin(urlUSD);
    http.setTimeout(10000); // Timeout de 10 segundos
    int httpCodeUSD = http.GET();
    
    if (httpCodeUSD == 200) {
      String payloadUSD = http.getString();
      DynamicJsonDocument docUSD(2048);
      deserializeJson(docUSD, payloadUSD);
      // O retorno é um array; pegamos o primeiro objeto
      JsonObject btcUSD = docUSD[0];
      float usd = btcUSD["current_price"];
      float change1h_usd = btcUSD["price_change_percentage_1h_in_currency"];
      float change24h_usd = btcUSD["price_change_percentage_24h_in_currency"];
      lastUsdPrice = usd;  // Atualiza o último preço USD
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
        
        // Definição de cores para variações
        uint16_t color1h = WHITE;
        if (change1h_usd > 5) color1h = BLUE;
        else if (change1h_usd < -5) color1h = RED;
        
        uint16_t color24h = WHITE;
        if (change24h_usd > 5) color24h = BLUE;
        else if (change24h_usd < -5) color24h = RED;
        
        // Se houve compra, calcula a variação da compra
        bool showPurchase = (purchasePrice > 0);
        float purchaseVariation = 0.0;
        uint16_t purchaseColor = WHITE;
        if (showPurchase) {
          purchaseVariation = ((usd - purchasePrice) / purchasePrice) * 100.0;
          purchaseColor = (purchaseVariation >= 0 ? GREEN : RED);
        }
        
        // Atualiza o display
        StickCP2.Lcd.fillScreen(BLACK);
        
        // Título
        StickCP2.Lcd.setTextSize(1);
        StickCP2.Lcd.setTextColor(WHITE);
        StickCP2.Lcd.setCursor(10, 0);
        StickCP2.Lcd.println("BTC Price");
        
        // Preços
        StickCP2.Lcd.setTextSize(2);
        StickCP2.Lcd.setTextColor(WHITE);
        StickCP2.Lcd.setCursor(10, 10);
        StickCP2.Lcd.printf("USD: $%.2f", usd);
        StickCP2.Lcd.setCursor(10, 35);
        StickCP2.Lcd.printf("BRL: R$%.2f", brl);
        
        // Variações de 1h e 24h
        StickCP2.Lcd.setTextSize(1);
        StickCP2.Lcd.setCursor(10, 60);
        StickCP2.Lcd.setTextColor(color1h);
        StickCP2.Lcd.printf("1h: %.2f%%", change1h_usd);
        StickCP2.Lcd.setCursor(10, 75);
        StickCP2.Lcd.setTextColor(color24h);
        StickCP2.Lcd.printf("24h: %.2f%%", change24h_usd);
        
        // Se a compra foi registrada, exibe "Comprado" e a variação da compra
        if (showPurchase) {
          StickCP2.Lcd.setTextSize(1);
          StickCP2.Lcd.setTextColor(WHITE);
          StickCP2.Lcd.setCursor(10, 90);
          StickCP2.Lcd.printf("Comprado: $%.2f", purchasePrice);
          StickCP2.Lcd.setCursor(10, 105);
          StickCP2.Lcd.setTextColor(purchaseColor);
          StickCP2.Lcd.printf("Var: %.2f%%", purchaseVariation);
        }
        
      } else {
        http.end();
        StickCP2.Lcd.fillScreen(BLACK);
        StickCP2.Lcd.setTextSize(1);
        StickCP2.Lcd.setTextColor(RED);
        StickCP2.Lcd.setCursor(10, 10);
        StickCP2.Lcd.println("Erro na requisicao BRL.");
        StickCP2.Lcd.printf("Codigo: %d", httpCodeBRL);
      }
    } else {
      http.end();
      StickCP2.Lcd.fillScreen(BLACK);
      StickCP2.Lcd.setTextSize(1);
      StickCP2.Lcd.setTextColor(RED);
      StickCP2.Lcd.setCursor(10, 10);
      StickCP2.Lcd.println("Erro na requisicao USD.");
      StickCP2.Lcd.printf("Codigo: %d", httpCodeUSD);
    }
  } else {
    StickCP2.Lcd.fillScreen(BLACK);
    StickCP2.Lcd.setTextSize(1);
    StickCP2.Lcd.setTextColor(RED);
    StickCP2.Lcd.setCursor(10, 10);
    StickCP2.Lcd.println("Sem WiFi.");
  }
}

void setup() {
  Serial.begin(115200); // Para debug, se necessário
  StickCP2.begin();
  StickCP2.Lcd.setRotation(1);
  StickCP2.Lcd.fillScreen(BLACK);
  StickCP2.Lcd.setTextSize(1);
  connectWiFi();
}

void loop() {
  StickCP2.update(); // Atualiza o estado dos botões

  // Se o botão A for pressionado, registra o preço de compra (em USD) e mostra mensagem de confirmação
  if (StickCP2.BtnA.wasPressed()) {
    purchasePrice = lastUsdPrice;
    Serial.printf("Botão A pressionado. Preço registrado: $%.2f\n", purchasePrice);
    // Exibe mensagem de confirmação (por 3 segundos)
    StickCP2.Lcd.fillRect(0, 120, 160, 16, BLACK);
    StickCP2.Lcd.setTextSize(1);
    StickCP2.Lcd.setTextColor(WHITE);
    StickCP2.Lcd.setCursor(10, 120);
    StickCP2.Lcd.printf("Comprado: $%.2f", purchasePrice);
    delay(3000); // Pausa breve para o usuário ver a mensagem
  }
  
  // Atualiza o display a cada 30 segundos (não bloqueante)
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    fetchAndDisplayBTC();
  }
}
