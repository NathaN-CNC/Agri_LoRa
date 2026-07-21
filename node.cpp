#include <Arduino.h>
#include <DHTesp.h>

#define NODE_ID 0x01
#define MSG_TYPE_FULL 0x03
#define DHT_PIN 15
#define SOIL_PIN 34
#define LED_STATUS 2
#define SLEEP_SECONDS 10
#define PUMP_PIN 5
#define RX_PIN 16
#define TX_PIN 17
HardwareSerial LoRaSerial(2);

// Limites do Fail-Safe (Modo de Sobrevivência local)
#define SOIL_CRITICAL_THRESHOLD 15 
#define FAIL_SAFE_TIMEOUT 30000 // Entra em emergência após 30s sem ouvir o Gateway

unsigned long last_gateway_comms = 0; // Registra a última comunicação bem sucedida
bool pump_state = false;

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

DHTesp dht;

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
  LoRaSerial.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);

  pinMode(LED_STATUS, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW); // Bomba inicia desligada
  
  dht.setup(DHT_PIN, DHTesp::DHT22);
  delay(2000); 
  
  // Define o boot como o momento zero da rede
  last_gateway_comms = millis();
}

void loop() {
  Serial.println("\n--- Enviando Telemetria ---");
  
  float temp = dht.getTemperature();
  float hum = dht.getHumidity();
  int soilRaw = analogRead(SOIL_PIN);
  uint16_t soil = map(soilRaw, 0, 4095, 0, 100);

  Packet pkt;
  pkt.node_id = NODE_ID;
  pkt.msg_type = MSG_TYPE_FULL;
  pkt.temperature = (int16_t)(temp * 10);
  pkt.humidity = (uint16_t)(hum * 10);
  pkt.soil_moisture = soil;
  pkt.light_lux = 350; 
  pkt.pressure = 10135;
  pkt.battery_mv = 4200;

  uint8_t len = sizeof(Packet);
  uint8_t crc = crc8((uint8_t*)&pkt, len);

  digitalWrite(LED_STATUS, HIGH);
  LoRaSerial.write(0x55); 
  LoRaSerial.write(0xAA); 
  LoRaSerial.write(len);  
  
  uint8_t *bytes = (uint8_t*)&pkt;
  for (uint8_t i = 0; i < len; i++) LoRaSerial.write(bytes[i]);
  LoRaSerial.write(crc);  
  digitalWrite(LED_STATUS, LOW);
  
  // === AGUARDANDO DOWNLINK DO GATEWAY ===
  bool ack_received = false;
  unsigned long start = millis();
  
  while (millis() - start < 3000) {
    if (LoRaSerial.available()) {
      String resp = LoRaSerial.readStringUntil('\n');
      resp.trim();
      
      if (resp.startsWith("ACK:")) {
        ack_received = true;
        last_gateway_comms = millis(); // Atualiza o relógio de segurança
        
        if (resp == "ACK:1") {
          digitalWrite(PUMP_PIN, HIGH);
          pump_state = true;
          Serial.println("Ordem do Cérebro (Gateway): BOMBA LIGADA");
        } else {
          digitalWrite(PUMP_PIN, LOW);
          pump_state = false;
          Serial.println("Ordem do Cérebro (Gateway): BOMBA DESLIGADA");
        }
        break;
      }
    }
    delay(10);
  }

  // === LÓGICA DE FAIL-SAFE (COMPUTAÇÃO DE BORDA) ===
  if (!ack_received) {
    Serial.println("Aviso: Falha no recebimento do Downlink.");
    
    // Verifica se a conexão caiu há muito tempo
    if (millis() - last_gateway_comms > FAIL_SAFE_TIMEOUT) {
      Serial.println("\n[!] ALERTA: CONEXÃO COM GATEWAY PERDIDA [!]");
      Serial.println(">>> Ativando Modo Sobrevivência (Edge Computing) <<<");
      
      if (soil < SOIL_CRITICAL_THRESHOLD) {
        digitalWrite(PUMP_PIN, HIGH);
        pump_state = true;
        Serial.println("Ação Local: Solo crítico detectado. Bomba LIGADA por segurança.");
      } else {
        digitalWrite(PUMP_PIN, LOW);
        pump_state = false;
        Serial.println("Ação Local: Solo estável. Bomba DESLIGADA.");
      }
    }
  }
  //Ao usar deep sleep na simulação pelo wokwi, reiniciava a CPU virtual do ESP32 de forma abrupta a cada ciclo. 
  //O componente do DHT22 e sua bib no Wokwi não lidava bem com esse desligamento contínuo e perdia a sincronização, retornando NaN a partir do segundo ciclo.
  //A solução foi mover a leitura para o loop()
  Serial.printf("Pausando simulação por %d segundos...\n", SLEEP_SECONDS);
  delay(SLEEP_SECONDS * 1000);
}
