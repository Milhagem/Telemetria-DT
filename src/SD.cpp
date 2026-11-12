#include "SD.hpp"

#include <SD.h>
#include <SPI.h>
#include <string.h>

#include "dados.hpp"
#include "queues.hpp"
#include "status.hpp"

#define SD_DEBUG 1

#if SD_DEBUG
    #define DBG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
    #define DBG_PRINTF(...) do {} while (0)
#endif



static constexpr const char* PASTA_LOG = "/telemetria";

static constexpr uint8_t PIN_SD_CS   = 10;
static constexpr uint8_t PIN_SD_SCK  = 12;
static constexpr uint8_t PIN_SD_MISO = 13;
static constexpr uint8_t PIN_SD_MOSI = 11;

static constexpr uint32_t SD_FREQ_HZ         = 10000000;
static constexpr uint32_t INTERVALO_FLUSH_MS = 1000;
static constexpr uint32_t INTERVALO_RETRY_MS = 2000;

static constexpr UBaseType_t TAM_QUEUE_SD = 1024;
static constexpr uint8_t PRIORIDADE_TASK_SD = 2;
static constexpr uint32_t STACK_TASK_SD = 6144;

static constexpr size_t TAM_AMOSTRA = 24;
static constexpr size_t AMOSTRAS_POR_BLOCO = 256;
static constexpr size_t TAM_BUFFER = TAM_AMOSTRA * AMOSTRAS_POR_BLOCO;

static_assert(
    sizeof(uint64_t) + 4 * sizeof(float) == TAM_AMOSTRA,
    "Amostra deve ter 24 bytes"
);


// ---------------- ESTADO ----------------

static SPIClass spiSD(FSPI);
static File arquivo;

static bool spiIniciado = false;
static bool cartaoOk = false;
static bool gravacaoConfirmada = false;

static uint8_t buffer[TAM_BUFFER];
static size_t usado = 0;


// ---------------- SERIALIZAÇÃO ----------------

static void serializarAmostra(const Amostra& a, uint8_t* destino) {

    memcpy(destino,      &a.tempo_us,    8);
    memcpy(destino + 8,  &a.corrente_A,  4);
    memcpy(destino + 12, &a.tensao_V,    4);
    memcpy(destino + 16, &a.potencia_W,  4);
    memcpy(destino + 20, &a.energia_J,   4);
}


// ---------------- ARQUIVO ----------------

static bool abrirNovoArquivo() {

    char nome[64];

    for (uint16_t i = 0; i < 1000; i++) {

        snprintf(
            nome,
            sizeof(nome),
            "%s/log_%03u.bin",
            PASTA_LOG,
            i
        );

        if (!SD.exists(nome)) {

            arquivo = SD.open(nome, FILE_WRITE);

            if (!arquivo)
                return false;

            DBG_PRINTF("[SD] gravando em %s\n", nome);

            return true;
        }
    }

    return false;
}

static bool montarCartao() {

    if (!spiIniciado) {

        spiSD.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

        spiIniciado = true;
    }

    if (!SD.begin(PIN_SD_CS, spiSD, SD_FREQ_HZ)) {
        cartaoOk = false;
        return false;
    }

    if (!SD.exists(PASTA_LOG) && !SD.mkdir(PASTA_LOG)) {

        DBG_PRINTF("[SD] falha ao criar pasta\n");

        SD.end();
        cartaoOk = false;

        return false;
    }

    if (!abrirNovoArquivo()) {

        DBG_PRINTF("[SD] falha ao criar arquivo\n");

        SD.end();
        cartaoOk = false;

        return false;
    }

    usado = 0;
    cartaoOk = true;
    gravacaoConfirmada = false;
    ledAmarelo();
    return true;
}

bool iniciarSD() {

    if (queueSD == nullptr) {

        queueSD = xQueueCreate(TAM_QUEUE_SD, sizeof(Amostra));

        if (queueSD == nullptr) {

            DBG_PRINTF("[SD] falha ao criar queueSD\n");
            return false;
        }
    }

    if (!montarCartao()) {

        DBG_PRINTF("[SD] cartao indisponivel\n");
        ledAmarelo();

        return false;
    }

    return true;
}


static void marcarFalhaCartao() {

    cartaoOk = false;
    gravacaoConfirmada = false;

    arquivo.close();
    SD.end();

    usado = 0;

    ledAmarelo();

    DBG_PRINTF("[SD] cartao indisponivel\n");
}


// ---------------- GRAVAÇÃO ----------------

static bool descarregarBuffer() {

    if (usado == 0)
        return true;

    if (!cartaoOk)
        return false;

    size_t escrito = arquivo.write(buffer, usado);

    if (escrito != usado) {

        DBG_PRINTF("[SD] erro de escrita\n");
        marcarFalhaCartao();
        return false;
    }

    usado = 0;

    // Verde somente depois de uma escrita real
    if (!gravacaoConfirmada) {

        gravacaoConfirmada = true;
        ledVerde();
        DBG_PRINTF("[SD] gravacao confirmada\n");
    }

    return true;
}

//Task SD
static void taskSD(void* parametro) {

    Amostra amostra;
    TickType_t ultimoFlush = xTaskGetTickCount();
    TickType_t ultimoRetry = 0;

    while (true) {

        if (
            xQueueReceive(
                queueSD,
                &amostra,
                pdMS_TO_TICKS(100)
            ) == pdTRUE
        ) {

            if (cartaoOk) {

                if (usado + TAM_AMOSTRA > TAM_BUFFER)
                    descarregarBuffer();

                if (cartaoOk) {
                    serializarAmostra(amostra, buffer + usado);
                    usado += TAM_AMOSTRA;
                }
            }
        }

        TickType_t agora = xTaskGetTickCount();


        // Flush periódico
        if (cartaoOk && agora - ultimoFlush >= pdMS_TO_TICKS(INTERVALO_FLUSH_MS)) {
            if (descarregarBuffer())
                arquivo.flush();
            ultimoFlush = agora;
        }

        // Retry se o cartão estiver indisponível
        if (!cartaoOk && agora - ultimoRetry >= pdMS_TO_TICKS(INTERVALO_RETRY_MS)) {

            ultimoRetry = agora;

            if (montarCartao()) {

                DBG_PRINTF("[SD] cartao recuperado\n");

                ultimoFlush = agora;
            }
        }
    }
}


// ---------------- CRIAÇÃO DA TASK ----------------

bool criarTaskSD() {

    if (queueSD == nullptr)
        return false;

    BaseType_t resultado = xTaskCreate(
        taskSD,
        "Task SD",
        STACK_TASK_SD,
        nullptr,
        PRIORIDADE_TASK_SD,
        nullptr
    );

    return resultado == pdPASS;
}