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

## ⚙️ Como Executar o Projeto

### Pré-requisitos

- Ter o [Node.js](https://nodejs.org/) instalado (versão 18 ou superior).
- Uma conta no [Wokwi](https://wokwi.com/) para simular os ESP32s.

### Passo 1: Iniciar o Servidor

1. Clone este repositório:

```bash
git clone https://github.com/seu-usuario/elo5-server.git
cd elo5-server
```

2. Instale as dependências:

```bash
npm install
```

3. Inicie o servidor:

```bash
node server.js
```

O terminal deve exibir as conexões com o banco e o broker MQTT.

### Passo 2: Abrir o Dashboard

Com o servidor rodando, abra o navegador e acesse:
👉 **[http://localhost:3000](http://localhost:3000)**

### Passo 3: Ligar os Dispositivos no Wokwi

1. Abra o projeto `esp32-sensores` no Wokwi e inicie a simulação.
2. Abra o projeto `esp32-rfid` em outra aba do Wokwi e inicie a simulação.
3. Os dados começarão a fluir automaticamente no dashboard.

## 📡 Estrutura dos Tópicos MQTT

| Tópico | Payload | Descrição |
| :--- | :--- | :--- |
| `elo5-system/sensores/dados` | JSON | Dados de telemetria (Porta, PIR, Distância). |
| `elo5-system/rfid/acesso` | String | UID do cartão RFID lido (ex: `01:02:03:04`). |

**Exemplo de Payload (Sensores):**

```json
{
  "porta_aberta": false,
  "distancia_cm": 148.2,
  "pir_movimento": false,
  "ts_ms": 128301
}
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

O servidor expõe duas rotas HTTP para consulta do histórico salvo no banco de dados.

### 1. Buscar Histórico de Sensores

- **Rota:** `GET /api/historico/sensores`
- **Retorno:** Array JSON com as últimas 50 leituras dos sensores (ordem cronológica).

**Exemplo de resposta:**

```json
[
  {
    "id": 1,
    "porta_aberta": 0,
    "distancia_cm": 148.2,
    "pir_movimento": 0,
    "ts_ms": 128301,
    "data_hora": "2023-11-06 23:05:21"
  }
]
```

### 2. Buscar Histórico de RFID

- **Rota:** `GET /api/historico/rfid`
- **Retorno:** Array JSON com os últimos 50 acessos de cartões (ordem cronológica).

## 🛠️ Decisões de Engenharia (Hardware)

O código do ESP32 de sensores foi projetado com arquitetura profissional para ambientes de produção:

- **Não-bloqueante:** O `loop()` nunca trava esperando WiFi/MQTT.
- **Backoff Exponencial:** Evita sobrecarregar o broker ao tentar reconectar (2s, 4s, 8s, 16s, 30s).
- **Buffer Circular:** Se a rede cair, até 32 leituras são salvas na RAM e enviadas após a reconexão.
- **Timestamps:** O campo `ts_ms` garante a ordem cronológica dos eventos mesmo após quedas de rede.

## 🪪 Cartões Autorizados (RFID)

Por padrão, o servidor libera o acesso para os cartões simulados do Wokwi:

- 🟦 **Blue Card:** `01:02:03:04`
- 🟩 **Green Card:** `11:22:33:44`

*(Para adicionar novos cartões, edite a constante `CARTOES_AUTORIZADOS` no arquivo `server.js`).*

## 📝 Licença

Este projeto está sob a licença MIT. Sinta-se livre para usar, estudar e modificar.

---

Desenvolvido para o projeto **ELO5**. 🚀

---

### 🚀 O que fazer agora?

1. Salve este arquivo como `README.md` na raiz do seu projeto.
2. Tire as capturas de tela que mencionamos (Dashboard e Wokwi) e coloque na pasta `docs/` e nas pastas de hardware.
3. No terminal, rode:

```bash
git add .
git commit -m "docs: Finaliza README com arquitetura, banco de dados, API e imagens"
git push origin main
```
