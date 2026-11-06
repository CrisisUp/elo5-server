# 🛡️ ELO5 - Sistema de Monitoramento e Controle de Acesso IoT

![Status](https://img.shields.io/badge/Status-Conclu%C3%ADdo-success)
![Node.js](https://img.shields.io/badge/Node.js-v18+-green)
![ESP32](https://img.shields.io/badge/ESP32-Wokwi-blue)
![MQTT](https://img.shields.io/badge/MQTT-HiveMQ-orange)

Este é um projeto completo de Internet das Coisas (IoT) que integra múltiplos dispositivos ESP32 a um servidor central em tempo real. O sistema monitora sensores ambientais e valida o acesso via RFID, exibindo tudo em um dashboard moderno.

## 🏗️ Arquitetura do Sistema

O projeto é dividido em três camadas principais que se comunicam via protocolo MQTT:

1. **Hardware (Sensores e Atuadores):**
   - **ESP32 Nº 1:** Leitor de sensores (Ultrassônico HC-SR04, PIR e Sensor de Porta MC-38).
   - **ESP32 Nº 2:** Controle de acesso (Leitor RFID MFRC522).
2. **Gateway de Comunicação (Broker):**
   - Broker público **HiveMQ** (`broker.hivemq.com`).
3. **Backend e Frontend:**
   - Servidor **Node.js** (Express + Socket.io + SQLite).
   - Dashboard Web em tempo real.

---

## 🚀 Tecnologias Utilizadas

### Backend e Frontend

- **Node.js** e **Express** (Servidor Web)
- **MQTT.js** (Comunicação com o Broker)
- **Socket.io** (WebSockets para atualização do Dashboard em tempo real)
- **SQLite3** (Banco de dados local para persistência do histórico)
- **HTML5 / CSS3 / JavaScript Vanilla** (Interface visual)

### Hardware (Simulado no Wokwi)

- **ESP32 DevKit C v4** (2 unidades)
- **HC-SR04** (Sensor de Distância)
- **PIR** (Sensor de Movimento)
- **Slide Switch** (Simula sensor magnético de porta)
- **MFRC522** (Leitor RFID)
- Bibliotecas C++: `PubSubClient`, `MFRC522`, `SPI`, `WiFi`

---

## 📂 Estrutura de Pastas

```text
elo5-server/
├── hardware/
│   ├── esp32-sensores/    # Código C++ e diagrama do ESP32 de sensores
│   └── esp32-rfid/        # Código C++ e diagrama do ESP32 de RFID
├── public/
│   └── index.html         # Dashboard Web
├── .gitignore
├── package.json
├── server.js              # Servidor Backend
└── README.md
```

## 🗄️ Modelagem do Banco de Dados

O servidor utiliza SQLite3. Ao rodar o `server.js` pela primeira vez, o arquivo `database.db` é criado com as seguintes tabelas:

### Tabela: `sensores`

| Coluna | Tipo | Descrição |
| :--- | :--- | :--- |
| `id` | INTEGER | Chave primária (Autoincremento) |
| `porta_aberta` | BOOLEAN | 1 se aberta, 0 se fechada |
| `distancia_cm` | REAL | Distância lida pelo ultrassônico (-1 se fora de alcance) |
| `pir_movimento` | BOOLEAN | 1 se detectou movimento |
| `ts_ms` | INTEGER | Timestamp (millis) enviado pelo ESP32 |
| `data_hora` | DATETIME | Data/Hora em que o servidor salvou o dado |

### Tabela: `acessos_rfid`

| Coluna | Tipo | Descrição |
| :--- | :--- | :--- |
| `id` | INTEGER | Chave primária (Autoincremento) |
| `uid` | TEXT | UID do cartão lido (ex: "01:02:03:04") |
| `autorizado` | BOOLEAN | 1 se o cartão está na lista permitida |
| `data_hora` | DATETIME | Data/Hora exata do acesso |

## 🌐 API HTTP (Rotas disponíveis)

O servidor expõe duas rotas para consulta de histórico:

### 1. Buscar Histórico de Sensores

- **Rota:** `GET /api/historico/sensores`
- **Retorno:** Array JSON com as últimas 50 leituras dos sensores.

### 2. Buscar Histórico de RFID

- **Rota:** `GET /api/historico/rfid`
- **Retorno:** Array JSON com os últimos 50 acessos de cartões.