#include <Arduino.h>

#include "INA.hpp"
#include "SD.hpp"
#include "status.hpp"


void setup() {

    iniciarLEDs();

    bool inaOK = iniciarINA();
    bool sdOK  = iniciarSD();

    if (inaOK && sdOK) {
        ledSistemaVerde();
    }
    else if (inaOK || sdOK) {
        ledSistemaAmarelo();
    }
    else {
        ledSistemaVermelho();
    }

    if (inaOK && sdOK) {
        criarTaskSD();
        criarTaskINA();
    }
}


void loop() {
    delay(1000);
}