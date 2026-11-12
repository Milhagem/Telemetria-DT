#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA228.h>
#include <esp_timer.h>

#include "INA.hpp"
#include "dados.hpp"
#include "queues.hpp"

constexpr uint8_t ALERT_PIN = 7;
constexpr uint8_t SDA_PIN   = 8;
constexpr uint8_t SCL_PIN   = 9;

Adafruit_INA228 ina228;
TaskHandle_t handleINA = nullptr;


// Inicializa e configura o INA228
bool iniciarINA() {

    constexpr uint8_t ENDERECO_INA = 0x40;

    constexpr float VALOR_SHUNT_OHM = 0.00105f;
    constexpr float CORRENTE_MAX_A  = 30.0f;

    constexpr uint8_t ADC_RANGE = 1;
    constexpr auto TEMPO_CONVERSAO = INA228_TIME_540_us;
    constexpr auto AVERAGING = INA228_COUNT_1; //sem média entre as amostras
    constexpr auto MODO_CONVERSAO = INA228_MODE_CONT_BUS_SHUNT; //Conversão contínua de VBUS + shunt


    // Inicializa I2C em 400 kHz
    if (!Wire.begin(SDA_PIN, SCL_PIN, 400000)) {
        return false;
    }

    // Inicializa INA228
    if (!ina228.begin(ENDERECO_INA)) {
        return false;
    }

    // Configurações do INA228
    ina228.setADCRange(ADC_RANGE);

    ina228.setShunt(VALOR_SHUNT_OHM, CORRENTE_MAX_A);

    ina228.setCurrentConversionTime(TEMPO_CONVERSAO);

    ina228.setVoltageConversionTime(TEMPO_CONVERSAO);

    ina228.setAveragingCount(AVERAGING);

    ina228.setMode(MODO_CONVERSAO);

    // ALERT ativo em LOW e modo transparente
    ina228.setAlertLatch(INA228_ALERT_LATCH_TRANSPARENT);

    ina228.setAlertPolarity(INA228_ALERT_POLARITY_NORMAL);

    // Zera energia acumulada
    ina228.resetAccumulators();

    return true;
}


// ISR chamada quando ALERT vai de HIGH para LOW
void IRAM_ATTR alertaINA() {

    if (handleINA == nullptr) {
        return;
    }

    BaseType_t taskAcordada = pdFALSE;

    //Acorda a TaskINA
    vTaskNotifyGiveFromISR(handleINA, &taskAcordada);

    // Se necessário, troca imediatamente para a TaskINA
    if (taskAcordada == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}


// Task responsável pela aquisição
void taskINA(void *parameter) {

    Amostra amostra;

    while (true) {

        // Espera uma nova conversão do INA228
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Marca o instante da amostra
        amostra.tempo_us = esp_timer_get_time();

        // Limpa CNVRF e libera o ALERT para a próxima conversão
        ina228.conversionReady();

        amostra.corrente_A = ina228.getCurrent_mA() / 1000.0f;

        amostra.tensao_V = ina228.getBusVoltage_V();

        amostra.potencia_W = ina228.getPower_mW() / 1000.0f;

        amostra.energia_J = ina228.readEnergy();

        // Envia a amostra para a queue do SD
        if (xQueueSend(queueSD, &amostra, 0) != pdPASS
        ) {
            // Futuramente: contador de amostras perdidas
        }
    }
}


// Cria a TaskINA e configura a interrupção do ALERT
void criarTaskINA() {

    BaseType_t resultado = xTaskCreate(
        taskINA,
        "Task INA",
        4096,
        nullptr,
        3,
        &handleINA
    );

    if (resultado != pdPASS) {
        return;
    }

    pinMode(ALERT_PIN, INPUT);

    // Detecta HIGH -> LOW no ALERT
    attachInterrupt(
        digitalPinToInterrupt(ALERT_PIN),
        alertaINA,
        FALLING
    );

    /*
     * Limpa um possível CNVRF que já tenha sido
     * gerado antes da interrupção ser configurada.
     */
    ina228.conversionReady();
}