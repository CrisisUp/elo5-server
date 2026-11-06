/**
 * ============================================================================
 *  Projeto: ELO5 - Módulo de Controle de Acesso (RFID)
 *  Arquivo: main.cpp
 * ----------------------------------------------------------------------------
 *  Descrição:
 *    Este é o segundo dispositivo do sistema ELO5. Ele é responsável por ler
 *    cartões MIFARE (MFRC522) e enviar o UID para o servidor via MQTT.
 *
 *  Destaques da Arquitetura:
 *    - Client ID Único: Usa o MAC address do ESP32 para evitar conflitos de
 *      conexão no broker público (que derrubam a conexão com rc=-2).
 *    - Debounce: Impede que o mesmo cartão seja lido repetidamente enquanto
 *      permanece encostado no leitor.
 *    - Timeouts Ajustados: Configurações otimizadas para a rede do Wokwi.
 *
 *  Payload MQTT (Tópico: elo5-system/rfid/acesso):
 *    <String>  ex: "01:02:03:04"
 * ============================================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>

// ============================================================================
//  CONFIGURAÇÃO DE REDE E MQTT
// ============================================================================
const char *WIFI_SSID = "Wokwi-GUEST";
const char *WIFI_PASS = "";

const char *MQTT_HOST = "broker.hivemq.com";
const uint16_t MQTT_PORT = 1883;
// Tópico diferente do sensor para o servidor saber que é um evento de acesso
const char *MQTT_TOPIC = "elo5-system/rfid/acesso";

// ============================================================================
//  MAPEAMENTO DE PINOS (MFRC522 no barramento VSPI)
// ----------------------------------------------------------------------------
//  GPIO 5  -> SDA (SS)
//  GPIO 18 -> SCK
//  GPIO 23 -> MOSI
//  GPIO 19 -> MISO
//  GPIO 21 -> RST
// ============================================================================
#define SS_PIN 5
#define RST_PIN 21

// ============================================================================
//  OBJETOS GLOBAIS E ESTADO
// ============================================================================
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
MFRC522 rfid(SS_PIN, RST_PIN);

unsigned long ultimoCartaoLidoMs = 0;
String ultimoUidLido = "";
const uint32_t DEBOUNCE_MS = 2000; // Ignora o mesmo cartão por 2 segundos

// ============================================================================
//  GERENCIAMENTO DE CONEXÃO (Não-bloqueante)
// ============================================================================

void conectarWiFi()
{
    if (WiFi.status() == WL_CONNECTED)
        return;

    Serial.print("[WiFi] Conectando");
    WiFi.begin(WIFI_SSID, WIFI_PASS, 6); // Canal 6 (Wokwi)

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(300);
        Serial.print(".");
    }
    Serial.println(" OK");
}

/**
 * Gera um ID único baseado no MAC Address do ESP32.
 * Isso resolve o erro rc=-2 causado por conflito de IDs no broker público.
 */
void conectarMqtt()
{
    while (!mqtt.connected())
    {
        Serial.print("[MQTT] Conectando...");

        // Cria um ID único (ex: "elo5-rfid-A1B2C3D4")
        uint64_t mac = ESP.getEfuseMac();
        String id = "elo5-rfid-" + String((uint32_t)(mac >> 32), HEX) + String((uint32_t)mac, HEX);

        if (mqtt.connect(id.c_str()))
        {
            Serial.println(" OK");
        }
        else
        {
            Serial.printf(" falhou (rc=%d), tentando de novo em 2s\n", mqtt.state());
            delay(2000);
        }
    }
}

// ============================================================================
//  SETUP
// ============================================================================
void setup()
{
    Serial.begin(115200);

    // Inicializa o barramento SPI e o Leitor RFID
    SPI.begin();
    rfid.PCD_Init();

    // Configurações do MQTT (Cruciais para a estabilidade no Wokwi)
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setKeepAlive(30);
    mqtt.setSocketTimeout(2);

    Serial.println("[RFID] Leitor MFRC522 pronto. Aproxime um cartão...");
}

// ============================================================================
//  LOOP PRINCIPAL
// ============================================================================
void loop()
{
    conectarWiFi();
    conectarMqtt();
    mqtt.loop();

    // 1. Verifica se há um cartão novo presente
    if (!rfid.PICC_IsNewCardPresent())
        return;

    // 2. Tenta ler o serial do cartão
    if (!rfid.PICC_ReadCardSerial())
        return;

    // 3. Converte o UID (bytes) para uma String legível (ex: "01:02:03:04")
    String uidAtual = "";
    for (byte i = 0; i < rfid.uid.size; i++)
    {
        if (rfid.uid.uidByte[i] < 0x10)
            uidAtual += "0";
        uidAtual += String(rfid.uid.uidByte[i], HEX);
        if (i < rfid.uid.size - 1)
            uidAtual += ":";
    }
    uidAtual.toUpperCase();

    // 4. Lógica de Debounce (evita ler o mesmo cartão repetidamente)
    bool mesmoCartao = (uidAtual == ultimoUidLido);
    bool tempoDebouncePassou = (millis() - ultimoCartaoLidoMs > DEBOUNCE_MS);

    if (!mesmoCartao || tempoDebouncePassou)
    {
        Serial.print("[RFID] Cartão detectado: " + uidAtual);

        // 5. Publica no broker
        if (mqtt.publish(MQTT_TOPIC, uidAtual.c_str()))
        {
            Serial.println(" | [MQTT] Enviado com sucesso!");
        }
        else
        {
            Serial.println(" | [MQTT] Falha no envio.");
        }

        ultimoUidLido = uidAtual;
        ultimoCartaoLidoMs = millis();
    }

    // 6. Finaliza a comunicação com o cartão atual para liberar o leitor
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();

    delay(100); // Pequena pausa para estabilidade do loop
}