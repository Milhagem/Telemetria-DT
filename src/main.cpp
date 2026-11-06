#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA228.h>

constexpr int PIN_SDA = 8;
constexpr int PIN_SCL = 9;

Adafruit_INA228 ina228;

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("Iniciando INA228...");

    Wire.begin(PIN_SDA, PIN_SCL);

    if (!ina228.begin()) {
        Serial.println("INA228 nao encontrado.");

        while (true) {
            delay(1000);
        }
    }

    Serial.println("INA228 encontrado!");

    // Faixa ±40,96 mV
    ina228.setADCRange(1);

    // Shunt = 1 mOhm
    // Corrente maxima = 30 A
    ina228.setShunt(0.00105f, 30.0f);

    // Tempo de conversao
    ina228.setCurrentConversionTime(INA228_TIME_540_us);
    ina228.setVoltageConversionTime(INA228_TIME_540_us);

    // Averaging para teste de bancada
    ina228.setAveragingCount(INA228_COUNT_16);

    // Zera energia acumulada ao iniciar
    ina228.resetAccumulators();

    Serial.println("INA228 configurado.");
}

void loop() {

    float corrente_A = ina228.getCurrent_mA() / 1000.0f;
    float tensao_V   = ina228.getBusVoltage_V();
    float shunt_mV   = ina228.getShuntVoltage_mV();
    float potencia_W = ina228.getPower_mW() / 1000.0f;
    float energia_J  = ina228.readEnergy();

    Serial.print("Corrente: ");
    Serial.print(corrente_A, 6);
    Serial.print(" A | ");

    Serial.print("Tensao: ");
    Serial.print(tensao_V, 6);
    Serial.print(" V | ");

    Serial.print("Shunt: ");
    Serial.print(shunt_mV, 6);
    Serial.print(" mV | ");

    Serial.print("Potencia: ");
    Serial.print(potencia_W, 6);
    Serial.print(" W | ");

    Serial.print("Energia: ");
    Serial.print(energia_J, 6);
    Serial.println(" J");

    delay(250);
}