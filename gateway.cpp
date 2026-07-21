#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define RX_PIN 16
#define TX_PIN 17
HardwareSerial LoRaSerial(2);
#define LED_RX 4

// Regras de Negócio de cultivo
#define SOIL_DRY_THRESHOLD 30
#define TEMP_SAFE_LIMIT 35.0

#pragma pack(push, 1)
struct Packet {
  uint8_t node_id;
  uint8_t msg_type;
  int16_t temperature;
  uint16_t humidity;
  uint16_t soil_moisture;
  uint16_t light_lux;
  uint16_t pressure;
  uint16_t battery_mv;
};
#pragma pack(pop)

uint8_t crc8(const uint8_t *data, uint8_t len) {
  uint8_t crc = 0xFF;
  while (len--) {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8; i++)
      crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
  }
  return crc;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  LoRaSerial.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);

  Wire.begin();
  delay(100);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("Erro OLED!");
    for (;;) { delay(100); }
  }

  pinMode(LED_RX, OUTPUT);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Gateway Inteligente");
  display.display();
}

void processPacket(const uint8_t *payload) {
  Packet *pkt = (Packet*)payload;
  float temp = pkt->temperature / 10.0;
  float hum = pkt->humidity / 10.0;
  uint16_t soil = pkt->soil_moisture;

  digitalWrite(LED_RX, HIGH);

  bool turn_on_pump = false;
  if (soil < SOIL_DRY_THRESHOLD && temp < TEMP_SAFE_LIMIT) {
    turn_on_pump = true;
  }

  // Imprime no console para debug
  Serial.printf("\n--- Pacote Recebido do Node %02X ---\n", pkt->node_id);
  Serial.printf("Temp: %.1fC | Umi: %.0f%% | Solo: %u%%\n", temp, hum, soil);
  
  if (turn_on_pump) {
    LoRaSerial.println("ACK:1");
    Serial.println("=> DECISÃO CENTRAL: Comando LIGAR BOMBA enviado.");
  } else {
    LoRaSerial.println("ACK:0");
    Serial.println("=> DECISÃO CENTRAL: Comando DESLIGAR BOMBA enviado.");
  }


  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.printf("NODE %02X | PUMP:%s", pkt->node_id, turn_on_pump ? "ON" : "OFF");

  display.setTextSize(2);
  display.setCursor(0, 12);
  display.printf("%.1fC", temp);

  display.setTextSize(1);
  display.setCursor(70, 12);
  display.printf("H:%.0f%%", hum);

  // Restaurando Luminosidade, Pressão e Bateria
  display.setCursor(0, 30);
  display.printf("Soil:%u%% L:%ulux", soil, pkt->light_lux);

  display.setCursor(0, 42);
  display.printf("P:%.0fhPa", pkt->pressure / 10.0);

  display.setCursor(0, 55);
  display.printf("Batt:%.2fV", pkt->battery_mv / 1000.0);

  display.display();
  delay(50);
  digitalWrite(LED_RX, LOW);
}

void loop() {
  static uint8_t buffer[64]; 
  static uint8_t expectedLen = 0;
  static uint8_t idx = 0;
  static enum { WAIT_SYNC1, WAIT_SYNC2, READ_LEN, READ_PAYLOAD } state = WAIT_SYNC1;
  static unsigned long lastByteTime = 0;

  while (LoRaSerial.available()) {
    uint8_t b = LoRaSerial.read();
    lastByteTime = millis();

    switch (state) {
      case WAIT_SYNC1: 
        if (b == 0x55) state = WAIT_SYNC2; break;
      case WAIT_SYNC2: 
        if (b == 0xAA) state = READ_LEN; else if (b != 0x55) state = WAIT_SYNC1; break;
      case READ_LEN:   
        expectedLen = b;
        if (expectedLen > sizeof(buffer) - 1) state = WAIT_SYNC1;
        else { idx = 0; state = READ_PAYLOAD; }
        break;
      case READ_PAYLOAD: 
        buffer[idx++] = b;
        if (idx >= expectedLen + 1) {
          if (crc8(buffer, expectedLen) == buffer[idx - 1]) processPacket(buffer);
          state = WAIT_SYNC1;
        }
        break;
    }
  }

  if (state != WAIT_SYNC1 && (millis() - lastByteTime > 500)) state = WAIT_SYNC1;
  delay(1); 
}
