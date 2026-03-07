// =================================================================================
// ===      GUARDIAN VISION  - AVATAR VIVO & CLIMA GRATUITO (ODS 3)          ===
// ===                                  ===
// =================================================================================

#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DHT.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h> // módulo não esta funcionando
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> 
#include "time.h"

// --- Configurações de Wi-Fi e ThingSpeak ---
const char* ssid = xxxxxxxxxxxxxxxxxx
const char* password = xxxxxxxxxxxxxxxxxxxxxxxxx
String writeAPIKey = xxxxxxxxxxxxxxxxxxxxx

// --- Configuração de Localização (Para o Open-Meteo Gratuito) ---
String latitude = "-23.xxxxx";  
String longitude = "-xx.xxx"; 

// --- Configurações de Pinos e Display (I2C Primário) ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
const int I2C_SDA_PIN = 21;
const int I2C_SCL_PIN = 22;

// --- Configuração do BME280 (I2C Secundário) ---
const int BME_SDA_PIN = 25;
const int BME_SCL_PIN = 26;

// --- Sensores ---
#define DHT_PIN 23
#define DHT_TYPE DHT22
#define GAS_SENSOR_PIN 32 

// --- Objetos ---
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);

// Criação do barramento I2C secundário e objeto BME280
TwoWire I2CBME = TwoWire(1); 
Adafruit_BME280 bme; // OBJETO BME280

// --- Variáveis de Controle de Tempo ---
unsigned long t_leitura = 0, t_envio = 0, t_clima = 0, t_troca_tela = 0;
unsigned long t_piscar = 0, t_falar = 0;
const long INT_LEITURA = 2000;
const long INT_ENVIO = 20000; // Envia pro ThingSpeak a cada 20 segundos
const long INT_CLIMA = 600000; 

// --- Variáveis de Leitura ---
float temperatura = 0.0, umidade = 0.0, indiceCalor = 0.0;
float valorGasSuavizado = 0.0;
float pressao = 0.0; // VARIÁVEL DA PRESSÃO DO BME280
const float FILTRO_ALFA = 0.15; 

String climaDescricao = "Buscando...";
float tempExterna = 0.0;
bool vaiChover = false;

bool erroDHT = false;
int telaAtual = 0;
const int NUM_TELAS = 3;
bool estaPiscando = false;
String falaAtual = "Estou acordando...";

void setup() {
  Serial.begin(115200);
  
  // Inicializa I2C Primário (OLED)
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  dht.begin();
  
  // Inicializa I2C Secundário (BME280) e o sensor
  I2CBME.begin(BME_SDA_PIN, BME_SCL_PIN);
  
  // O BME280 exige que passemos o endereço (geralmente 0x76) e o barramento I2C que criamos
  if (!bme.begin(0x76, &I2CBME) && !bme.begin(0x77, &I2CBME)) { 
    Serial.println(F("Nao foi possivel encontrar um sensor BME280 valido, verifique a fiacao!"));
  } else {
    Serial.println(F("BME280 Encontrado com sucesso!"));
  }
  
  analogReadResolution(12);
  analogSetPinAttenuation(GAS_SENSOR_PIN, ADC_11db);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("Falha OLED")); while(1); 
  }

  // Animação de Acordar
  display.clearDisplay();
  for(int i=1; i<=10; i++) {
    display.clearDisplay();
    display.fillRoundRect(20, 26 - i, 24, i*2, 5, SSD1306_WHITE);
    display.fillRoundRect(84, 26 - i, 24, i*2, 5, SSD1306_WHITE);
    display.display();
    delay(150);
  }
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(18, 55); display.println("Ola, humano! ^^");
  display.display();
  delay(3000); 

  // Conexão Wi-Fi
  WiFi.begin(ssid, password);
  display.clearDisplay();
  display.setCursor(10, 30);
  display.print("Conectando WiFi...");
  display.display();
  
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    tentativas++;
  }

  server.on("/", HTTP_GET, handleRoot);
  server.begin();
}

void loop() {
  server.handleClient(); 
  
  // Busca do clima
  static bool primeiraBusca = true;
  if (WiFi.status() == WL_CONNECTED && (primeiraBusca || millis() - t_clima >= INT_CLIMA)) {
    buscarPrevisaoTempoGratuita();
    t_clima = millis();
    primeiraBusca = false;
  }
  
  // Controle de Vida: Piscar
  if (millis() - t_piscar > random(2000, 6000)) {
    estaPiscando = true;
    atualizarDisplay(); 
    delay(random(150, 300)); 
    estaPiscando = false;
    t_piscar = millis();
  }

  // Controle de Vida: Falar
  if (millis() - t_falar >= 6000) {
    gerarFalaDoAvatar();
    t_falar = millis();
    atualizarDisplay();
  }

  // Leitura de Sensores
  if (millis() - t_leitura >= INT_LEITURA) { 
    lerSensores(); 
    atualizarDisplay(); 
    t_leitura = millis(); 
  }

  // ENVIO PARA O THINGSPEAK
  if (WiFi.status() == WL_CONNECTED && millis() - t_envio >= INT_ENVIO) {
    enviarThingSpeak();
    t_envio = millis();
  }
  
  // Tempo de Telas
  unsigned long tempoDeExibicao = (telaAtual == 0) ? 15000 : 5000;
  if (millis() - t_troca_tela >= tempoDeExibicao) { 
    telaAtual = (telaAtual + 1) % NUM_TELAS; 
    atualizarDisplay(); 
    t_troca_tela = millis(); 
  }
}

void enviarThingSpeak() {
  HTTPClient http;
  String url = "http://api.thingspeak.com/update?api_key=" + writeAPIKey + 
               "&field1=" + String(temperatura) + 
               "&field2=" + String(umidade) + 
               "&field3=" + String(valorGasSuavizado) +
               "&field4=" + String(pressao);
  
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode > 0) {
    Serial.println("Dados enviados ao ThingSpeak com sucesso!");
  } else {
    Serial.println("Erro ao enviar para ThingSpeak.");
  }
  http.end();
}

void buscarPrevisaoTempoGratuita() {
  if(WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = "http://api.open-meteo.com/v1/forecast?latitude=" + latitude + "&longitude=" + longitude + "&current_weather=true";
  
  http.begin(url);
  http.setTimeout(8000); 
  http.addHeader("User-Agent", "ESP32_Guardian"); 
  
  int httpCode = http.GET();
  if (httpCode == 200 || httpCode == 201) {
    String payload = http.getString();
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      tempExterna = doc["current_weather"]["temperature"];
      int codigoClima = doc["current_weather"]["weathercode"];
      
      vaiChover = false;
      if (codigoClima == 0) climaDescricao = "Ceu Limpo";
      else if (codigoClima >= 1 && codigoClima <= 3) climaDescricao = "Nublado";
      else if (codigoClima == 45 || codigoClima == 48) climaDescricao = "Neblina";
      else if (codigoClima >= 51 && codigoClima <= 67) { climaDescricao = "Chuva!"; vaiChover = true; }
      else if (codigoClima >= 80 && codigoClima <= 82) { climaDescricao = "Pancadas!"; vaiChover = true; }
      else if (codigoClima >= 95) { climaDescricao = "Tempestade!"; vaiChover = true; }
      else climaDescricao = "Instavel";
    }
  }
  http.end();
}

void lerSensores() {
  // Leitura DHT
  float temp_lida = dht.readTemperature();
  float umid_lida = dht.readHumidity();

  if (isnan(temp_lida)) erroDHT = true;
  else {
    erroDHT = false;
    temperatura = temp_lida;
    umidade = umid_lida;
    indiceCalor = dht.computeHeatIndex(temperatura, umidade, false);
  }
  
  // Leitura Gás
  int rawGas = analogRead(GAS_SENSOR_PIN);
  if (valorGasSuavizado == 0) valorGasSuavizado = rawGas; 
  else valorGasSuavizado = (FILTRO_ALFA * rawGas) + ((1.0 - FILTRO_ALFA) * valorGasSuavizado);

  // Leitura BME280 (Pressão em hPa) - COMANDO ATUALIZADO
  float leituraPressao = bme.readPressure() / 100.0F; 
  if (!isnan(leituraPressao) && leituraPressao > 0) {
    pressao = leituraPressao;
  }
}

void gerarFalaDoAvatar() {
  int sorteio = random(0, 3); 
  if ((vaiChover || umidade > 80.0) && valorGasSuavizado > 400 && valorGasSuavizado < 1000) {
    String falas[] = {"Cheiro de chuva!", "Sinto o petricor!", "Vem agua por ai..."};
    falaAtual = falas[sorteio];
  } else if (valorGasSuavizado >= 1200) {
    String falas[] = {"Cof! Cof!", "Ar poluido!", "Abra a janela..."};
    falaAtual = falas[sorteio];
  } else if (indiceCalor >= 31.0) {
    String falas[] = {"Que calor...", "Beba agua!", "To derretendo.."};
    falaAtual = falas[sorteio];
  } else if (temperatura < 18.0 && temperatura > 0) {
    String falas[] = {"Brrr.. frio!", "Cade o casaco?", "To congelando"};
    falaAtual = falas[sorteio];
  } else {
    String falas[] = {"Tudo otimo ^^", "Ar purinho!", "Estou de olho!"};
    falaAtual = falas[sorteio];
  }
}

void atualizarDisplay() {
  display.clearDisplay();
  switch (telaAtual) {
    case 0: drawScreenAvatar(); break;    
    case 1: drawScreenSensores(); break;  
    case 2: drawScreenClima(); break;     
  }
  display.display();
}

void drawScreenAvatar() {
  int xEsq = 32, xDir = 96, yOlho = 26, raio = 18;
  if (valorGasSuavizado >= 1200) {
    display.drawLine(xEsq - 10, yOlho - 10, xEsq + 10, yOlho + 10, SSD1306_WHITE);
    display.drawLine(xEsq - 10, yOlho + 10, xEsq + 10, yOlho - 10, SSD1306_WHITE);
    display.drawLine(xDir - 10, yOlho - 10, xDir + 10, yOlho + 10, SSD1306_WHITE);
    display.drawLine(xDir - 10, yOlho + 10, xDir + 10, yOlho - 10, SSD1306_WHITE);
    display.drawLine(55, 48, 73, 48, SSD1306_WHITE);
  } else if ((vaiChover || umidade > 80.0) && valorGasSuavizado > 400 && valorGasSuavizado < 1000) {
    if(!estaPiscando) {
      display.fillCircle(xEsq, yOlho, raio, SSD1306_WHITE);
      display.fillCircle(xDir, yOlho, raio, SSD1306_WHITE);
      display.fillCircle(xEsq, yOlho - 8, 5, SSD1306_BLACK); 
      display.fillCircle(xDir, yOlho - 8, 5, SSD1306_BLACK);
      display.drawCircle(64, 46, 3, SSD1306_WHITE);
    } else {
      display.drawFastHLine(xEsq - 14, yOlho, 28, SSD1306_WHITE);
      display.drawFastHLine(xDir - 14, yOlho, 28, SSD1306_WHITE);
    }
  } else if (indiceCalor >= 31.0) {
    if(!estaPiscando) {
      display.fillCircle(xEsq, yOlho, raio, SSD1306_WHITE);
      display.fillCircle(xDir, yOlho, raio, SSD1306_WHITE);
      display.fillRect(xEsq - raio, yOlho - raio, raio*2 + 2, raio + 4, SSD1306_BLACK); 
      display.fillRect(xDir - raio, yOlho - raio, raio*2 + 2, raio + 4, SSD1306_BLACK); 
      display.fillCircle(xEsq, yOlho + 5, 4, SSD1306_BLACK); 
      display.fillCircle(xDir, yOlho + 5, 4, SSD1306_BLACK);
      display.drawCircle(118, 15, 4, SSD1306_WHITE);
      display.drawLine(118, 10, 118, 19, SSD1306_WHITE);
    } else {
      display.drawLine(xEsq - raio, yOlho + 5, xEsq + raio, yOlho + 5, SSD1306_WHITE);
      display.drawLine(xDir - raio, yOlho + 5, xDir + raio, yOlho + 5, SSD1306_WHITE);
    }
  } else if (temperatura < 18.0 && temperatura > 0) {
    if(!estaPiscando) {
      display.fillCircle(xEsq, yOlho, raio - 2, SSD1306_WHITE);
      display.fillCircle(xDir, yOlho, raio - 2, SSD1306_WHITE);
      display.fillCircle(xEsq, yOlho, 3, SSD1306_BLACK); 
      display.fillCircle(xDir, yOlho, 3, SSD1306_BLACK);
    } else {
      display.drawLine(xEsq - raio, yOlho, xEsq + raio, yOlho, SSD1306_WHITE);
      display.drawLine(xDir - raio, yOlho, xDir + raio, yOlho, SSD1306_WHITE);
    }
    display.drawLine(55, 48, 60, 46, SSD1306_WHITE);
    display.drawLine(60, 46, 65, 50, SSD1306_WHITE);
    display.drawLine(65, 50, 70, 46, SSD1306_WHITE);
    display.drawLine(70, 46, 75, 48, SSD1306_WHITE);
  } else {
    if(!estaPiscando) {
      display.fillCircle(xEsq, yOlho, raio, SSD1306_WHITE);
      display.fillCircle(xDir, yOlho, raio, SSD1306_WHITE);
      int offsetPupilaX = (millis() / 3000) % 3 - 1; 
      display.fillCircle(xEsq + (offsetPupilaX * 4), yOlho, 5, SSD1306_BLACK); 
      display.fillCircle(xDir + (offsetPupilaX * 4), yOlho, 5, SSD1306_BLACK);
      display.fillCircle(xEsq - 5 + (offsetPupilaX * 4), yOlho - 5, 2, SSD1306_WHITE);
      display.fillCircle(xDir - 5 + (offsetPupilaX * 4), yOlho - 5, 2, SSD1306_WHITE);
      display.drawLine(58, 45, 64, 49, SSD1306_WHITE);
      display.drawLine(64, 49, 70, 45, SSD1306_WHITE);
    } else {
      display.drawFastHLine(xEsq - 14, yOlho, 28, SSD1306_WHITE);
      display.drawFastHLine(xDir - 14, yOlho, 28, SSD1306_WHITE);
    }
  }

  int tamanhoTexto = falaAtual.length() * 6; 
  int xTexto = (128 - tamanhoTexto) / 2;
  display.setTextSize(1);
  display.setCursor(xTexto > 0 ? xTexto : 0, 56); 
  display.println(falaAtual);

  if (vaiChover) {
    display.setCursor(0, 0);
    display.print("! CHUVA !");
  }
}

void drawScreenSensores() {
  display.setTextSize(1); display.setCursor(0, 0); display.println("Sensor Interno (Casa)");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  display.setCursor(0, 18); display.print("Temp: "); display.print(temperatura, 1); display.println(" C");
  display.setCursor(0, 30); display.print("Umid: "); display.print((int)umidade); display.println(" %");
  display.setCursor(0, 42); display.print("Sens: "); display.print(indiceCalor, 1); display.println(" C");
  display.setCursor(0, 54); display.print("Pres: "); display.print(pressao, 1); display.println(" hPa");
}

void drawScreenClima() {
  display.setTextSize(1); display.setCursor(0, 0); display.print("Clima na Rua");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  if(tempExterna == 0.0) {
    display.setCursor(0, 25); display.print("Buscando dados...");
  } else {
    display.setCursor(0, 18); display.println(climaDescricao); 
    display.setCursor(0, 32); display.print("Temperatura: "); display.print(tempExterna, 1); display.println(" C");
  }
  if(vaiChover) {
    display.setCursor(0, 46); display.print("Cuidado com a Dengue!");
  }
}

void handleRoot() {
  String html = "<html><head><title>Guardian</title></head><body><h1>Guardian Ativo</h1></body></html>";
  server.send(200, "text/html", html);
}
