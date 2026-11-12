#include <Arduino.h>

#include "INA.hpp"
#include "SD.hpp"
#include "status.hpp"


void setup() {

    Serial.begin(115200);
    delay(500);

    iniciarLEDs();

    // Estado inicial: ainda não há gravação confirmada
    ledAmarelo();


    // Inicializa os módulos
    bool inaOK = iniciarINA();
    bool sdOK  = iniciarSD();

    Serial.println(inaOK ? "INA228: OK" : "INA228: FALHA");

    Serial.println(sdOK ? "SD: OK" : "SD: INDISPONIVEL");


    // Falha do INA é crítica
    if (!inaOK) {
        ledVermelho();
        Serial.println("TaskINA: NAO INICIADA");
        return;
    }

    bool taskSD_OK = criarTaskSD();

    if (!taskSD_OK) {

        ledVermelho();

        Serial.println("TaskSD: FALHA");

        return;
    }

    Serial.println("TaskSD: OK");


    // Inicia aquisição do INA228
    criarTaskINA();

    Serial.println("TaskINA: OK");

    Serial.println("=== SISTEMA INICIADO ===");
}


void loop() {

    // Aquisição e gravação são feitas pelas Tasks
    delay(1000);
}