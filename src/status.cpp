#include <Arduino.h>
#include "status.hpp"

constexpr uint8_t LED_R = 42;
constexpr uint8_t LED_G = 41;

void iniciarLEDs() {

    pinMode(LED_R, OUTPUT);
    pinMode(LED_G, OUTPUT);

    ledDesligado();
}

void ledVerde() {

    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, HIGH);
}

void ledVermelho() {

    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_G, LOW);
}

void ledAmarelo() {

    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_G, HIGH);
}

void ledDesligado() {

    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, LOW);
}