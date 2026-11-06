/**
 * ============================================================================
 *  Projeto: ELO5 - Monitoramento de Sala (ESP32)
 *  Arquivo: main.cpp
 * ----------------------------------------------------------------------------
 *  Descrição:
 *    Lê três sensores (Ultrassônico, Sensor de Porta e PIR) e publica os
 *    dados no broker MQTT. O sistema foi projetado para ser resiliente:
 *    se a rede cair, os dados são guardados em um buffer circular em RAM
 *    e enviados assim que a conexão for restabelecida.
 *
 *  Destaques da Arquitetura:
 *    - Rede Não-Bloqueante: O loop() nunca trava esperando WiFi ou MQTT.
 *    - Backoff Exponencial: Evita sobrecarregar o broker ao reconectar.
 *    - Buffer Circular: Armazena até 32 leituras offline sem estourar a RAM.
 *    - Timestamps (ts_ms): Garante que o servidor saiba a hora exata da
 *      leitura, mesmo que ela chegue atrasada após uma queda de rede.
 *
 *  Payload MQTT (Tópico: elo5-system/sensores/dados):
 *    {
 *      "porta_aberta":  <bool>,
 *      "distancia_cm":  <float|null>,  // null se fora de alcance
 *      "pir_movimento": <bool>,
 *      "ts_ms":         <uint32>       // millis() local no momento da leitura
 *    }
 * ============================================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>

// ============================================================================
//  CONFIGURAÇÃO DE REDE E MQTT
// ============================================================================
const char *WIFI_SSID = "Wokwi-GUEST";
const char *WIFI_PASS = "";

const char *MQTT_HOST = "broker.hivemq.com";           // Broker público
const uint16_t MQTT_PORT = 1883;                       // Porta TCP padrão
const char *MQTT_TOPIC = "elo5-system/sensores/dados"; // Tópico de publicação

// ============================================================================
//  MAPEAMENTO DE PINOS
// ----------------------------------------------------------------------------
//  GPIO 5  -> TRIG  do HC-SR04
//  GPIO 18 -> ECHO  do HC-SR04
//  GPIO 4  -> Slide Switch (simula sensor MC-38) - INPUT_PULLUP
//  GPIO 19 -> OUT do sensor PIR
// ============================================================================
const int PIN_TRIG = 5;
const int PIN_ECHO = 18;
const int PIN_SENSOR_PORTA = 4;
const int PIN_PIR = 19;

// ============================================================================
//  CONSTANTES DE COMPORTAMENTO
// ============================================================================
const uint32_t INTERVALO_ENVIO_MS = 1000; // Heartbeat de 1 segundo
const uint32_t TIMEOUT_ECHO_US = 30000;   // ~5m de alcance máximo no ultrassônico
const float VEL_SOM_CM_US = 0.0343f;      // Velocidade do som (cm/µs)
const float DIST_MAX_CM = 400.0f;         // Limite para considerar "fora de alcance"
const size_t PAYLOAD_MAX = 160;           // Tamanho máximo do buffer do JSON

// Backoff Exponencial (2s -> 4s -> 8s -> 16s -> 30s)
const uint32_t BACKOFF_MIN_MS = 2000;
const uint32_t BACKOFF_MAX_MS = 30000;
const uint8_t BACKOFF_MAX_TENT = 5;

// ============================================================================
//  BUFFER CIRCULAR (Para quedas de rede)
// ============================================================================
const size_t BUFFER_TAM = 32; // ~512 bytes de RAM

struct Leitura
{
    uint32_t timestamp;
    float distancia;
    bool porta;
    bool pir;
};

Leitura bufferLeituras[BUFFER_TAM];
size_t bufHead = 0;       // Índice do item mais antigo
size_t bufCount = 0;      // Quantidade de itens no buffer
uint32_t bufOverflow = 0; // Contador de dados perdidos se o buffer encher

// ============================================================================
//  OBJETOS GLOBAIS E ESTADO
// ============================================================================
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

unsigned long ultimoEnvio = 0;
bool ultimoPorta = false;
bool ultimoPir = false;

// Controle de Backoff MQTT
unsigned long proximaTentativaMqtt = 0;
uint8_t tentativasMqtt = 0;

// ============================================================================
//  FUNÇÕES DO BUFFER CIRCULAR (O(1) para push e pop)
// ============================================================================
bool bufferCheio() { return bufCount == BUFFER_TAM; }
bool bufferVazio() { return bufCount == 0; }

void bufferPush(const Leitura &l)
{
    if (bufferCheio())
    {
        // Se cheio, avança o head para sobrescrever o mais antigo
        bufHead = (bufHead + 1) % BUFFER_TAM;
        bufOverflow++;
    }
    else
    {
        bufCount++;
    }
    // Calcula a posição do novo item e salva
    size_t idx = (bufHead + bufCount - 1) % BUFFER_TAM;
    bufferLeituras[idx] = l;
}

bool bufferPop(Leitura &out)
{
    if (bufferVazio())
        return false;
    out = bufferLeituras[bufHead];
    bufHead = (bufHead + 1) % BUFFER_TAM;
    bufCount--;
    return true;
}

// ============================================================================
//  GERENCIAMENTO DE CONEXÃO (Não-bloqueante)
// ============================================================================

bool wifiConectado() { return WiFi.status() == WL_CONNECTED; }

/**
 * Inicia a conexão WiFi sem travar o loop.
 */
void iniciarWiFiSePreciso()
{
    if (wifiConectado())
        return;

    static unsigned long proximaTentativa = 0;
    if (millis() < proximaTentativa)
        return;

    Serial.println("[WiFi] Iniciando conexão...");
    // O canal 6 é específico do Wokwi. Em hardware real, use 0 (auto).
    WiFi.begin(WIFI_SSID, WIFI_PASS, 6);
    proximaTentativa = millis() + 5000;
}

/**
 * Calcula o tempo de espera baseado no número de falhas consecutivas.
 */
uint32_t calcularBackoff(uint8_t tentativas)
{
    if (tentativas >= BACKOFF_MAX_TENT)
        return BACKOFF_MAX_MS;
    uint32_t ms = BACKOFF_MIN_MS << tentativas; // 2s * 2^n
    return (ms > BACKOFF_MAX_MS) ? BACKOFF_MAX_MS : ms;
}

/**
 * Tenta conectar ao MQTT respeitando o backoff. NUNCA bloqueia.
 */
void manterMqtt()
{
    if (mqtt.connected())
    {
        tentativasMqtt = 0; // Reseta backoff no sucesso
        return;
    }
    if (!wifiConectado())
        return;
    if (millis() < proximaTentativaMqtt)
        return;

    String id = "elo5-esp32-" + String((uint32_t)esp_random(), HEX);
    Serial.printf("[MQTT] Tentando conectar (tentativa %u)... ", tentativasMqtt + 1);

    if (mqtt.connect(id.c_str()))
    {
        Serial.println("OK");
        tentativasMqtt = 0;
    }
    else
    {
        tentativasMqtt++;
        uint32_t espera = calcularBackoff(tentativasMqtt);
        proximaTentativaMqtt = millis() + espera;
        Serial.printf("falhou (rc=%d). Próxima em %lu ms\n", mqtt.state(), espera);
    }
}

// ============================================================================
//  SENSOR E PUBLICAÇÃO
// ============================================================================

/**
 * Lê a distância do HC-SR04 em centímetros.
 * @return float Distância em cm, ou -1.0 se não houver eco/fora de alcance.
 */
float lerDistanciaCm()
{
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);

    uint32_t duracao = pulseIn(PIN_ECHO, HIGH, TIMEOUT_ECHO_US);
    if (duracao == 0)
        return -1.0f;

    float cm = (duracao * VEL_SOM_CM_US) / 2.0f;
    return (cm > DIST_MAX_CM) ? -1.0f : cm;
}

/**
 * Serializa e publica uma leitura individual.
 * @return true se o publish foi aceito (QoS 0), false se offline/falhou.
 */
bool publicarUma(const Leitura &l)
{
    if (!mqtt.connected())
        return false;

    char payload[PAYLOAD_MAX];
    if (l.distancia < 0)
    {
        snprintf(payload, sizeof(payload),
                 "{\"porta_aberta\":%s,\"distancia_cm\":null,\"pir_movimento\":%s,\"ts_ms\":%lu}",
                 l.porta ? "true" : "false", l.pir ? "true" : "false", (unsigned long)l.timestamp);
    }
    else
    {
        snprintf(payload, sizeof(payload),
                 "{\"porta_aberta\":%s,\"distancia_cm\":%.1f,\"pir_movimento\":%s,\"ts_ms\":%lu}",
                 l.porta ? "true" : "false", l.distancia, l.pir ? "true" : "false", (unsigned long)l.timestamp);
    }

    bool ok = mqtt.publish(MQTT_TOPIC, payload);
    Serial.printf("[MQTT] %s -> %s\n", ok ? "enviado" : "FALHOU", payload);
    return ok;
}

// Variável para garantir que uma mensagem não se perca se o broker cair no meio do dreno
const size_t MAX_POR_CICLO = 8;
Leitura pendente;
bool temPendente = false;

/**
 * Drena o buffer circular em lotes pequenos para não travar o mqtt.loop().
 */
void drenarBuffer()
{
    if (!mqtt.connected())
        return;

    size_t enviados = 0;
    while (enviados < MAX_POR_CICLO && mqtt.connected())
    {
        if (temPendente)
        {
            if (!publicarUma(pendente))
                return; // Falhou, tenta de novo no próximo ciclo
            temPendente = false;
            enviados++;
            continue;
        }
        if (!bufferPop(pendente))
            break; // Buffer vazio
        temPendente = true;
    }

    if (enviados > 0)
    {
        Serial.printf("[MQTT] Drenadas %u leituras (restam %u)\n", (unsigned)enviados, (unsigned)bufCount);
    }
    if (bufOverflow > 0)
    {
        Serial.printf("[BUFFER] Overflow: %lu leituras perdidas\n", (unsigned long)bufOverflow);
        bufOverflow = 0;
    }
}

// ============================================================================
//  SETUP
// ============================================================================
void setup()
{
    Serial.begin(115200);

    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    pinMode(PIN_SENSOR_PORTA, INPUT_PULLUP);
    pinMode(PIN_PIR, INPUT);

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);

    iniciarWiFiSePreciso();

    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setKeepAlive(30);
    mqtt.setSocketTimeout(2);
    mqtt.setBufferSize(512);

    Serial.printf("[BUFFER] Capacidade: %u leituras (~%u bytes)\n",
                  (unsigned)BUFFER_TAM, (unsigned)(BUFFER_TAM * sizeof(Leitura)));
}

// ============================================================================
//  LOOP PRINCIPAL
// ============================================================================
void loop()
{
    // 1. Manutenção de Conexão (nunca bloqueia)
    iniciarWiFiSePreciso();
    manterMqtt();
    mqtt.loop();

    // 2. Leitura dos Sensores
    Leitura atual;
    atual.timestamp = millis();
    atual.porta = (digitalRead(PIN_SENSOR_PORTA) == HIGH);
    atual.pir = (digitalRead(PIN_PIR) == HIGH);
    atual.distancia = lerDistanciaCm();

    // 3. Log Serial
    Serial.print("[PORTA] ");
    Serial.print(atual.porta ? "ABERTA | " : "Fechada | ");
    Serial.print("[PIR] ");
    Serial.print(atual.pir ? "Movimento | " : "Sem mov. | ");
    Serial.print("[DIST] ");
    if (atual.distancia < 0)
        Serial.println("Fora de alcance");
    else
        Serial.printf("%.1f cm\n", atual.distancia);

    // 4. Lógica de Decisão (Enviar ou Enfileirar)
    bool mudou = (atual.porta != ultimoPorta) || (atual.pir != ultimoPir);
    bool passouHeartbeat = (millis() - ultimoEnvio >= INTERVALO_ENVIO_MS);

    if (mudou || passouHeartbeat)
    {
        if (mqtt.connected())
        {
            publicarUma(atual); // Caminho rápido: online
        }
        else
        {
            bufferPush(atual); // Caminho lento: guarda na memória
        }
        ultimoEnvio = millis();
        ultimoPorta = atual.porta;
        ultimoPir = atual.pir;
    }

    // 5. Se houver dados represados, tenta enviá-los
    drenarBuffer();

    delay(100); // Estabilidade para o sensor ultrassônico
}