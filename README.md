# 🛡️ ELO5 - Sistema de Monitoramento e Controle de Acesso IoT

![Status](https://img.shields.io/badge/Status-Conclu%C3%ADdo-success)
![Node.js](https://img.shields.io/badge/Node.js-v18+-green)
![ESP32](https://img.shields.io/badge/ESP32-Wokwi-blue)
![MQTT](https://img.shields.io/badge/MQTT-HiveMQ-orange)
![License](https://img.shields.io/badge/License-MIT-lightgrey)

Este é um projeto completo de Internet das Coisas (IoT) que integra múltiplos dispositivos ESP32 a um servidor central em tempo real. O sistema monitora sensores ambientais (porta, presença e distância) e valida o acesso via cartões RFID, exibindo tudo em um dashboard web moderno.

---

## 🏗️ Arquitetura do Sistema

O projeto é dividido em três camadas principais que se comunicam via protocolo MQTT:

1. **Hardware (Edge Devices):**
   - **ESP32 Nº 1 (Sensores):** Leitura de Ultrassônico (HC-SR04), Sensor de Movimento (PIR) e Sensor de Porta (Slide Switch).
   - **ESP32 Nº 2 (Acesso):** Leitor de cartões MIFARE (MFRC522).
2. **Gateway de Comunicação (Broker):**
   - Broker público **HiveMQ** (`broker.hivemq.com`) atuando como intermediário das mensagens.
3. **Backend e Frontend (Server):**
   - Servidor **Node.js** (Express + Socket.io + SQLite) para processar, salvar e emitir os dados em tempo real.

---

## 🖼️ Galeria do Projeto

### Imagens a adicionar

![Dashboard](docs/dashboard.png)
*Dashboard rodando em tempo real com alertas de porta e RFID.*

![Circuito de Sensores](hardware/esp32-sensores/esquema.png)
*Circuito simulado no Wokwi (ESP32 + Sensores).*

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
├── docs/                  # Imagens e documentações extras
├── hardware/
│   ├── esp32-sensores/    # Código C++ e diagrama do ESP32 de sensores
│   └── esp32-rfid/        # Código C++ e diagrama do ESP32 de RFID
├── public/
│   └── index.html         # Dashboard Web (Frontend)
├── .gitignore
├── package.json
├── server.js              # Servidor Backend (Node.js)
└── README.md
```
