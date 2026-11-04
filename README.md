# Telemetria-DT
Repositório para desenvolver a telemetria do DT

Código I2CScanner para fazer uma varredura nas conexões SCL e SDA entre o ESP32 e o INA228

#include <Wire.h>

constexpr int PIN_SDA = 8;
constexpr int PIN_SCL = 9;

void setup() {
  Serial.begin(115200);
  delay(2000);

  // Inicia o I2C a 100 kHz
  if (!Wire.begin(PIN_SDA, PIN_SCL, 100000)) {
    Serial.println("Falha ao iniciar o I2C.");
    while (true) {
      delay(1000);
    }
  }

  Wire.setTimeOut(50);
  Serial.println("\nScanner I2C iniciado!");
}

void loop() {
  int encontrados = 0;

  Serial.println("\nProcurando dispositivos...");

  for (uint8_t endereco = 1; endereco < 127; endereco++) {
    Wire.beginTransmission(endereco);
    uint8_t erro = Wire.endTransmission();

    if (erro == 0) {
      Serial.printf("Dispositivo encontrado em 0x%02X\n", endereco);
      encontrados++;
    } else if (erro != 2) {
      Serial.printf("Erro %u no endereco 0x%02X\n", erro, endereco);
    }
  }

  Serial.printf("Varredura concluida: %d dispositivo(s).\n", encontrados);
  delay(3000);
}


