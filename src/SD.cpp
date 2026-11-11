#include "SD.hpp"

#include <SD.h>   // biblioteca SD do Arduino (resolve para SD.h, não para este SD.hpp)
#include <SPI.h>

#include "dados.hpp"     // struct Amostra
#include "queues.hpp"    // extern QueueHandle_t queueSD

// ---------------- configuração ----------------

// pinos do SD no ESP32-S3 (SPI2 / FSPI, pinos padrão)
static constexpr uint8_t PIN_SD_CS   = 10;
static constexpr uint8_t PIN_SD_SCK  = 12;
static constexpr uint8_t PIN_SD_MISO = 13;
static constexpr uint8_t PIN_SD_MOSI = 11;

static constexpr uint32_t SD_FREQ_HZ         = 20000000;  // 20 MHz; baixar para 10 MHz se der erro
static constexpr size_t   TAM_BUFFER         = 4096;      // bytes juntados antes de escrever
static constexpr uint32_t INTERVALO_FLUSH_MS = 1000;      // de quanto em quanto tempo força gravar
static constexpr uint32_t INTERVALO_RETRY_MS = 2000;      // tentativa de remontar o cartão após erro
static constexpr uint8_t  PRIORIDADE_TASK_SD = 1;
static constexpr uint32_t STACK_TASK_SD      = 4096;

// ---------------- estado interno ----------------
static SPIClass spiSD(FSPI);
static File     arquivo;
static bool     cartaoOk = false;

static char   buffer[TAM_BUFFER];
static size_t usado = 0;

// cria log_000.csv, log_001.csv... usando o primeiro nome livre
static bool abrirNovoArquivo() {
    char nome[24];
    for (uint16_t i = 0; i < 1000; i++) {
        snprintf(nome, sizeof(nome), "/log_%03u.csv", i);
        if (!SD.exists(nome)) {
            arquivo = SD.open(nome, FILE_WRITE);
            if (!arquivo) return false;
            arquivo.println("tempo_us,corrente_A,tensao_V,potencia_W,energia_J");
            arquivo.flush();
            Serial.printf("[SD] gravando em %s\n", nome);
            return true;
        }
    }
    return false;  // 1000 arquivos já existem
}

bool iniciarSD() {
    spiSD.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

    if (!SD.begin(PIN_SD_CS, spiSD, SD_FREQ_HZ)) {
        Serial.println("[SD] falha ao montar o cartao");
        cartaoOk = false;
        return false;
    }
    if (!abrirNovoArquivo()) {
        Serial.println("[SD] falha ao criar arquivo de log");
        cartaoOk = false;
        return false;
    }
    cartaoOk = true;
    usado = 0;
    return true;
}

// escreve o buffer em RAM no cartão; retorna false se falhar
static bool descarregarBuffer() {
    if (usado == 0) return true;
    if (!cartaoOk) return false;

    size_t escrito = arquivo.write(reinterpret_cast<const uint8_t*>(buffer), usado);
    if (escrito != usado) {
        Serial.println("[SD] erro de escrita");
        cartaoOk = false;
        arquivo.close();
        SD.end();
        return false;
    }
    usado = 0;
    return true;
}

static void taskSD(void* parametro) {
    Amostra a;
    char linha[128];
    TickType_t ultimoFlush = xTaskGetTickCount();
    TickType_t ultimoRetry = 0;

    for (;;) {
        // espera até 100 ms por uma amostra (timeout deixa o flush por tempo funcionar)
        if (xQueueReceive(queueSD, &a, pdMS_TO_TICKS(100)) == pdTRUE) {
            int n = snprintf(linha, sizeof(linha), "%llu,%.4f,%.4f,%.4f,%.4f\n",
                             (unsigned long long)a.tempo_us,
                             a.corrente_A, a.tensao_V, a.potencia_W, a.energia_J);

            if (n > 0 && cartaoOk) {
                if (usado + n > TAM_BUFFER) descarregarBuffer();
                if (usado + n <= TAM_BUFFER) {   // se o cartão caiu, a amostra é descartada
                    memcpy(buffer + usado, linha, n);
                    usado += n;
                }
            }
        }

        TickType_t agora = xTaskGetTickCount();

        // flush periódico: garante que no máximo ~1 s de dados se perde se faltar energia
        if (cartaoOk && (agora - ultimoFlush) >= pdMS_TO_TICKS(INTERVALO_FLUSH_MS)) {
            if (descarregarBuffer()) arquivo.flush();
            ultimoFlush = agora;
        }

        // cartão com problema: tenta remontar de tempos em tempos (abre um arquivo novo)
        if (!cartaoOk && (agora - ultimoRetry) >= pdMS_TO_TICKS(INTERVALO_RETRY_MS)) {
            ultimoRetry = agora;
            iniciarSD();
        }
    }
}

void criarTaskSD() {
    xTaskCreate(taskSD, "taskSD", STACK_TASK_SD, nullptr, PRIORIDADE_TASK_SD, nullptr);
}