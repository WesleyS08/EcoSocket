#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define TEMPOLEITURA 100
#define TEMPOESPERA 3000
#define INTERVALO_TESTE 1800000     // Intervalo de teste em milissegundos (30 minutos)
#define MAX_CONSECUTIVE_FAILURES 3  // Número máximo de falhas consecutivas antes de reiniciar o modem

const char* ssid = "Sua_Rede";             // Coloque o nome da sua rede Wi-Fi
const char* password = "Sua_Senha";         // Coloque a senha da sua rede Wi-Fi
const char* test_url = "https://www.google.com/";  // URL para testar a conexão
const char* tomada_ip = "192.168.1.100";           // Endereço IP da tomada inteligente
const int tomada_porta = 80;                       // Porta da tomada inteligente
const char* tomada_ligar = "/ligar";               // Endpoint para ligar a tomada inteligente
const char* tomada_desligar = "/desligar";         // Endpoint para desligar a tomada inteligente
const int ledPin = 2;                              // Pino do LED do ESP32

byte endereco;
byte codigoResultado = 0;
byte dispositivosEncontrados = 0;
byte consecutiveFailures = 0;        // Contador de falhas consecutivas
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Endereço I2C padrão para o LCD

void setup() {
  Serial.begin(115200);
  Wire.begin();
  while (!Serial)
    ;

  pinMode(ledPin, OUTPUT);
  lcd.init();       // Inicializa o LCD
  lcd.backlight();  // Liga a luz de fundo do LCD

  lcd.setCursor(0, 0);
  lcd.print("Conectando Wi-Fi");
  lcd.setCursor(2, 1);
  lcd.print("Aguarde ...");
  connectToWiFi();  // Conecta ao Wi-Fi

  scanI2c();

  testInternetConnection();  // Testa a conexão com a internet
}

void connectToWiFi() {
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(0, 1);  // Mover para a segunda linha do LCD
    lcd.print(".");
  }

  Serial.println("\nConectado ao WiFi!");
  lcd.clear();  // Limpa a tela do LCD
  lcd.setCursor(0, 0);
  lcd.print("WiFi Conectado");
  lcd.setCursor(0, 1);
  lcd.print(ssid);
  delay(10000);
  lcd.clear();
}

void testInternetConnection() {
  lcd.setCursor(0, 0);
  lcd.print("Testando Internet");

  HTTPClient http;
  http.begin(test_url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    // Código HTTP 200 significa que a conexão foi bem-sucedida
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("Conectado à internet");
      Serial.println("Internet OK");
      lcd.setCursor(0, 1);
      lcd.print("Internet OK");
      Serial.println("Internet OK");
      consecutiveFailures = 0;  // Resetar o contador de falhas consecutivas

      // Faz o LED piscar continuamente enquanto a internet estiver OK
      blinkLED(ledPin, 500);  // Pisca com intervalo de 500 ms
    }
  } else {
    Serial.println("Falha ao conectar");
    Serial.println("Internet Falhou");
    lcd.setCursor(0, 1);
    lcd.print("Internet Falhou");
    Serial.println("Internet Falhou");
    consecutiveFailures++;  // Incrementar o contador de falhas consecutivas

    // Se atingir o número máximo de falhas consecutivas, reiniciar o modem
    if (consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
      lcd.clear();  // Limpa a tela do LCD
      lcd.setCursor(0, 0);
      lcd.print("Reiniciando o modem...");
      delay(30000);  // Aguarda 30 segundos
      // Após 30 segundos, tenta reconectar ao Wi-Fi
      connectToWiFi();
    }
  }

  http.end();
}

void controlarTomada(const char* endpoint) {
  HTTPClient http;
  String url = "http://" + String(tomada_ip) + endpoint;

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    Serial.println("Comando enviado com sucesso para controlar a tomada!");
  } else {
    Serial.println("Falha ao enviar comando para controlar a tomada!");
  }

  http.end();
}

void scanI2c() {
  for (endereco = 1; endereco < 128; endereco++) {  // Endereço 0 é reservado
    Wire.beginTransmission(endereco);
    codigoResultado = Wire.endTransmission();
    if (codigoResultado == 0) {
      Serial.print("Dispositivo I2C detectado no endereço: 0x");
      Serial.println(endereco, HEX);
      dispositivosEncontrados++;
      delay(TEMPOESPERA);
    }
    delay(TEMPOLEITURA);
  }
  if (dispositivosEncontrados > 0) {
    Serial.print("Foi encontrado um total de: ");
    Serial.print(dispositivosEncontrados);
    Serial.println(" dispositivos");
  } else {
    Serial.println("Nenhum dispositivo foi encontrado !!!");
  }
}

void blinkLED(int pin, int interval) {
  while (true) {
    digitalWrite(pin, HIGH);
    delay(interval);
    digitalWrite(pin, LOW);
    delay(interval);
  }
}

void loop() {
  delay(INTERVALO_TESTE);  // Espera 30 minutos (em milissegundos)
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Testando Internet");
  testInternetConnection();  // Testa a conexão com a internet

  // Se a conexão com a internet falhou, desligar a tomada
  if (consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
    lcd.clear();  // Limpa a tela do LCD
    lcd.setCursor(0, 0);
    lcd.print("Desligando a tomada...");
    controlarTomada(tomada_desligar);
  } else {
    // Se a conexão com a internet está OK e foi restabelecida após uma falha, ligar a tomada
    if (consecutiveFailures > 0) {
      lcd.clear();  // Limpa a tela do LCD
      lcd.setCursor(0, 0);
      lcd.print("Testando Tomada");
      testTomada();  // Testa se a tomada está desligada
    }
  }
}

void testTomada() {
  // Envia uma solicitação para verificar o estado atual da tomada
  HTTPClient http;
  String url = "http://" + String(tomada_ip) + "/estado";  // Endpoint para verificar o estado da tomada

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    if (response == "ligado") {
      Serial.println("Tomada está ligada");
    } else {
      Serial.println("Tomada está desligada");
      lcd.clear();  // Limpa a tela do LCD
      lcd.setCursor(0, 1);
      lcd.print("Ligando a tomada...");
      controlarTomada(tomada_ligar);
    }
  } else {
    Serial.println("Falha ao verificar o estado da tomada!");
  }

  http.end();
}
