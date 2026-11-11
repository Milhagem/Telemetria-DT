#include "SD.hpp"

#include <SD.h>   // biblioteca SD do Arduino 
#include <SPI.h> // estabelece comunicação entre o esp e o sd
#include <string.h>

#include "dados.hpp"     // struct Amostra
#include "queues.hpp"    // extern QueueHandle_t queueSD

// ---------------- debug ----------------
// 1 = imprime mensagens no Serial (testes). 0 = sem mensagens (versão final).
#define SD_DEBUG 1

#if SD_DEBUG
    #define DBG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
    #define DBG_PRINTF(...) do {} while (0)
#endif

// ---------------- configuração ----------------

// pinos do SD no ESP32-S3 (SPI2 / FSPI, pinos padrão)
static constexpr uint8_t PIN_SD_CS   = 10;
static constexpr uint8_t PIN_SD_SCK  = 12;
static constexpr uint8_t PIN_SD_MISO = 13;
static constexpr uint8_t PIN_SD_MOSI = 11;

static constexpr uint32_t SD_FREQ_HZ         = 20000000;  // frequencia de comunicação 20 MHz;baixar para 10 MHz se der erro
static constexpr uint32_t INTERVALO_FLUSH_MS = 1000;      // de quanto em quanto tempo força gravar
static constexpr uint32_t INTERVALO_RETRY_MS = 2000;      // tentativa de remontar o cartão após erro

static constexpr UBaseType_t TAM_QUEUE_SD = 1024;         // quantidade max de amostras na fila (1024 * 24 B = 24 KB)

static constexpr uint8_t  PRIORIDADE_TASK_SD = 2;        
static constexpr uint32_t STACK_TASK_SD      = 6144;   // tamanho da fila 

// formato binário de cada amostra no arquivo: 24 bytes 
//   bytes  0..7  : uint64_t tempo_us
//   bytes  8..11 : float corrente_A
//   bytes 12..15 : float tensao_V
//   bytes 16..19 : float potencia_W
//   bytes 20..23 : float energia_J

//confere se o tamanho da amostra esta certa
static constexpr size_t TAM_AMOSTRA = 24;
static_assert(sizeof(uint64_t) + 4 * sizeof(float) == TAM_AMOSTRA, "formato da amostra deve ter 24 bytes");

// quantidade de amostras e tamanho do buffer
static constexpr size_t AMOSTRAS_POR_BLOCO = 256;
static constexpr size_t TAM_BUFFER = TAM_AMOSTRA * AMOSTRAS_POR_BLOCO;   // 6144 bytes

// ---------------- estado interno ----------------
static SPIClass spiSD(FSPI);
static bool     spiIniciado = false;
static File     arquivo;
static bool     cartaoOk = false;

static uint8_t buffer[TAM_BUFFER];
static size_t  usado = 0;   

// copia uma amostra para o buffer no formato fixo de 24 bytes
// destina os dados serialmente 
static void serializarAmostra(const Amostra& a, uint8_t* destino) {
    memcpy(destino,      &a.tempo_us,   8);
    memcpy(destino + 8,  &a.corrente_A, 4);
    memcpy(destino + 12, &a.tensao_V,   4);
    memcpy(destino + 16, &a.potencia_W, 4);
    memcpy(destino + 20, &a.energia_J,  4);
}

// cria  arquivo  ex:log_000.bin...
static bool abrirNovoArquivo() {
    char nome[24];
    for (uint16_t i = 0; i < 1000; i++) {
        snprintf(nome, sizeof(nome), "/log_%03u.bin", i);
        if (!SD.exists(nome)) {
            arquivo = SD.open(nome, FILE_WRITE);
            if (!arquivo) return false;
            DBG_PRINTF("[SD] gravando em %s\n", nome);
            return true;
        }
    }
    return false;  // 1000 arquivos já existem
}

// monta o cartão e abre um arquivo novo. não mexe na queueSD.
// é usada na inicialização e nas tentativas de reconexão.
static bool montarCartao() {
    if (!spiIniciado) {
        spiSD.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
        spiIniciado = true;
    }

    if (!SD.begin(PIN_SD_CS, spiSD, SD_FREQ_HZ)) {
        DBG_PRINTF("[SD] falha ao montar o cartao\n");
        cartaoOk = false;
        return false;
    }

    if (!abrirNovoArquivo()) {
        DBG_PRINTF("[SD] falha ao criar arquivo de log\n");
        SD.end();
        cartaoOk = false;
        return false;
    }

    usado = 0;
    cartaoOk = true;
    return true;
}

//inicia o sd
bool iniciarSD() {
    // a queue só é criada uma vez, nunca recriada em reconexões
    if (queueSD == nullptr) {
        queueSD = xQueueCreate(TAM_QUEUE_SD, sizeof(Amostra));
        if (queueSD == nullptr) {
            DBG_PRINTF("[SD] falha ao criar a queueSD\n");
            return false;
        }
    }
    return montarCartao();
}

// cartão indisponível: fecha o arquivo, desmonta e descarta o que estava no buffer
static void marcarFalhaCartao() {
    cartaoOk = false;
    arquivo.close();
    SD.end();
    usado = 0;
}

// escreve o buffer em RAM no cartão (só amostras inteiras); retorna false se falhar
static bool descarregarBuffer() {
    if (usado == 0) return true;
    if (!cartaoOk) return false;

    size_t escrito = arquivo.write(buffer, usado);
    if (escrito != usado) {
        DBG_PRINTF("[SD] erro de escrita (%u de %u bytes)\n", (unsigned)escrito, (unsigned)usado);
        marcarFalhaCartao();
        return false;
    }
    usado = 0;
    return true;
}

static void taskSD(void* parametro) {
    Amostra a;
    TickType_t ultimoFlush = xTaskGetTickCount();//marca o ultimo sincronizado
    TickType_t ultimoRetry = 0; //marca a ultima tenativa de recuperação

    for (;;) {
        // tenta retirar uma amostra da fila e espera até 100 ms 
        if (xQueueReceive(queueSD, &a, pdMS_TO_TICKS(100)) == pdTRUE) {

            // com o cartão fora, a amostra é descartada 
            if (cartaoOk) {
                // sem espaço para mais uma amostra inteira: grava o bloco antes e descarta as outras amostras
                if (usado + TAM_AMOSTRA > TAM_BUFFER) {
                    descarregarBuffer();
                }
                if (cartaoOk) {   // verifica o cartao apos a gravação, descarregarBuffer() pode ter derrubado o cartão
                    serializarAmostra(a, buffer + usado);
                    usado += TAM_AMOSTRA;
                }
            }
        }

        //controle de tempo 
        TickType_t agora = xTaskGetTickCount();

        // flush periódico: no máximo ~1 s de dados se perde se faltar energia
        if (cartaoOk && (agora - ultimoFlush) >= pdMS_TO_TICKS(INTERVALO_FLUSH_MS)) {
            if (descarregarBuffer()) {
                arquivo.flush();
            }
            ultimoFlush = agora;
        }

        // cartão indisponível: tenta remontar de tempos em tempos, sempre em arquivo novo
        if (!cartaoOk && (agora - ultimoRetry) >= pdMS_TO_TICKS(INTERVALO_RETRY_MS)) {
            ultimoRetry = agora;
            if (montarCartao()) {
                DBG_PRINTF("[SD] cartao recuperado\n");
                ultimoFlush = agora;
            }
        }
    }
}

// cria task sd 

bool criarTaskSD() {
    if (queueSD == nullptr) {
        DBG_PRINTF("[SD] queueSD inexistente, taskSD nao criada\n");
        return false;
    }

    BaseType_t resultado = xTaskCreate(
        taskSD,
        "taskSD",
        STACK_TASK_SD,
        nullptr,
        PRIORIDADE_TASK_SD,
        nullptr
    );

    if (resultado != pdPASS) {
        DBG_PRINTF("[SD] falha ao criar a taskSD\n");
        return false;
    }
    return true;
}