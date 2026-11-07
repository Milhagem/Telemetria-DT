#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA228.h>
#include <esp_timer.h>

#include "INA.hpp"
#include "dados.hpp"
#include "queues.hpp"

constexpr uint8_t ALERT_PIN = 7;
constexpr uint8_t SDA_PIN = 8;
constexpr uint8_t SCL_PIN = 9;

Adafruit_INA228 ina228;  //cria um objeto da biblioteca do INA
TaskHandle_t handleINA = nullptr; //identificação da Task


// INICIALIZAÇÃO DO INA228
bool iniciarINA() {

    // Endereço I2C do INA228
    constexpr uint8_t ENDERECO_INA = 0x40;

    // Valor do shunt
    constexpr float VALOR_SHUNT_OHM = 0.001f;

    // Corrente máxima configurada
    constexpr float CORRENTE_MAX_A = 30.0f;

    //Faixa do ADC do shunt
    // 0 = ±163,84 mV
    // 1 = ±40,96 mV
    constexpr uint8_t ADC_RANGE = 1;

    //Tempo de conversão
    // 540 us para shunt + 540 us para VBUS
    // ≈ 926 ciclos/s
    constexpr auto TEMPO_CONVERSAO = INA228_TIME_540_us;

    //media entre as amostras(não usamos pq queremos mais definição)
    constexpr auto AVERAGING = INA228_COUNT_1;

    //Modo de conversão contínua de shunt + VBUS
    constexpr auto MODO_CONVERSAO = INA228_MODE_CONT_BUS_SHUNT;

    // Inicializa o barramento I2C
    if (!Wire.begin(SDA_PIN, SCL_PIN, 400000)) { //seta os pinos para o I²C, e fixa em 400kHz
        return false;
    }

    // Inicializa o INA228 no endereço configurado
    if (!ina228.begin(ENDERECO_INA)) {
        return false;
    }

    // Configura faixa do ADC
    ina228.setADCRange(ADC_RANGE);

    ina228.setShunt(VALOR_SHUNT_OHM, CORRENTE_MAX_A);

    ina228.setCurrentConversionTime(TEMPO_CONVERSAO);

    ina228.setVoltageConversionTime(TEMPO_CONVERSAO);

    ina228.setAveragingCount(AVERAGING);

    ina228.setMode(MODO_CONVERSAO);

    ina228.resetAccumulators();

    return true;
}

void IRAM_ATTR alertaINA() {

    if (handleINA == nullptr) { //retorna se a task está vazia/n criada
        return;
    }

    BaseType_t taskAcordada = pdFALSE; //variável para ver se a Task está acordada

    //função do FreeRTOS e serve para enviar uma notificação para uma Task
    vTaskNotifyGiveFromISR(handleINA, &taskAcordada);

    if (taskAcordada == pdTRUE) { //serve para dar prioridade à taskINA, para q n ocorra um atraso entre o sinal de ALERT, e a leitura dos dados
        portYIELD_FROM_ISR();
    }
}

void taskINA(void *parameter) {

    Amostra amostra; //struct de amostras

    while (true) {

        // Desliga a task enquanto a notificação do  vTaskNotifyGiveFromISRn chega
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Marca o instante em que a amostra ficou disponível para o ESP
        amostra.tempo_us = esp_timer_get_time();

        // Lê os dados que o INA228 já converteu
        amostra.corrente_A =
            ina228.getCurrent_mA() / 1000.0f;

        amostra.tensao_V =
            ina228.getBusVoltage_V();

        amostra.potencia_W =
            ina228.getPower_mW() / 1000.0f;

        amostra.energia_J =
            ina228.readEnergy();

        // Envia uma cópia da amostra para a fila do SD
        if (xQueueSend(queueSD, &amostra, 0) != pdPASS) {}// futuramente incrementar contador de amostras perdidas
    }
}


// Função q implementa a estrutura xTaskCreate do FreeRTOS, a qual cria a task
void criarTaskINA() {

    BaseType_t resultado = xTaskCreate(
        taskINA,       // Função que a Task vai executar
        "Task INA",    // Nome da Task
        4096,          // Stack
        nullptr,       // Parâmetro passado para a Task
        3,             // Prioridade
        &handleINA     // Guarda o handle da Task
    );

    if (resultado != pdPASS) {
        return;
    }

        pinMode(ALERT_PIN, INPUT);

    attachInterrupt(digitalPinToInterrupt(ALERT_PIN), alertaINA, FALLING); //recebe o sinal do pino alert, e se ele desce para GND/FALLING, roda a função alertaINA
}
