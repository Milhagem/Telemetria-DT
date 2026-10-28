#include <Arduino.h>
#include "INA.hpp"
#include "SD.hpp"

void setup() {
    Serial.begin(115200);

    iniciarINA();
    iniciarSD();

    criarTaskSD();
    criarTaskINA();
}

void loop() {
}