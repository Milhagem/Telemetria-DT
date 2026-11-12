#include <Arduino.h>

#include "INA.hpp"
#include "SD.hpp"
#include "status.hpp"


void setup() {

    Serial.begin(115200);
    delay(500);

    iniciarLEDs();

    // Enquanto ainda não existe gravação confirmada
    ledAmarelo();


    // Inicializa os módulos
    bool inaOK = iniciarINA();
    bool sdOK  = iniciarSD();


    // Debug
    Serial.println(
        inaOK ? "INA228: OK" : "INA228: FALHA"
    );

    Serial.println(
        sdOK ? "SD: OK" : "SD: INDISPONIVEL"
    );


    /*
     * Cria TaskSD mesmo se o cartão estiver indisponível.
     * Assim ela pode detectar o cartão se ele for inserido depois.
     */
    bool taskSD_OK = criarTaskSD();


    if (!taskSD_OK) {

        Serial.println("TaskSD: FALHA");

        ledVermelho();

        return;
    }


    Serial.println("TaskSD: OK");


    /*
     * Sem INA não existe aquisição.
     * É uma falha crítica.
     */
    if (!inaOK) {

        ledVermelho();

        Serial.println("TaskINA: NAO INICIADA");

        return;
    }


    // Inicia aquisição do INA
    criarTaskINA();

    Serial.println("TaskINA: OK");

    Serial.println("=== SISTEMA INICIADO ===");
}


void loop() {

    // Aquisição e gravação são feitas pelas Tasks
    delay(1000);
}