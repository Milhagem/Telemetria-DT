#include <Arduino.h>

#include "INA.hpp"
#include "SD.hpp"
#include "status.hpp"


void setup() {

    iniciarLEDs();

    bool inaOK = iniciarINA();
    bool sdOK  = iniciarSD();

    if (inaOK && sdOK) {
        ledVerde();
    }
    else if (inaOK || sdOK) {
        ledAmarelo();
    }
    else {
        ledVermelho();
    }

    if (inaOK && sdOK) {
        criarTaskSD();
        criarTaskINA();
    }
}


void loop() {
    delay(1000);
}