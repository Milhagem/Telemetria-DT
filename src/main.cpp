#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("ESP32 funcionando!");
}

void loop() {
    Serial.println("Teste de funcionamento");
    delay(1000);
}