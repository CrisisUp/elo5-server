/**
 * ============================================================================
 *  Projeto: ELO5 - Servidor Central (IoT Gateway + Backend)
 *  Arquivo: server.js
 * ----------------------------------------------------------------------------
 *  Descrição:
 *    Este servidor atua como o cérebro do sistema ELO5. Ele é responsável por:
 *    1. Conectar-se ao broker MQTT (HiveMQ) e escutar múltiplos tópicos.
 *    2. Processar dados de Sensores Ambientais (Porta, PIR, Distância).
 *    3. Processar dados de Controle de Acesso (Leitor RFID).
 *    4. Salvar os dados históricos em um banco SQLite local.
 *    5. Emitir os eventos em tempo real para o Dashboard via WebSockets.
 *
 *  Tópicos MQTT Escutados:
 *    - elo5-system/sensores/dados -> Dados de telemetria da sala.
 *    - elo5-system/rfid/acesso    -> UID dos cartões passados no leitor.
 * ============================================================================
 */

const express = require("express");
const http = require("http");
const mqtt = require("mqtt");
const { Server } = require("socket.io");
const sqlite3 = require("sqlite3").verbose();
const path = require("path");

// ============================================================================
//  1. CONFIGURAÇÕES GERAIS E CONSTANTES
// ============================================================================
const CONFIG = {
  PORT: 3000,
  MQTT_URL: "mqtt://broker.hivemq.com",
  TOPICOS: {
    SENSORES: "elo5-system/sensores/dados",
    RFID: "elo5-system/rfid/acesso",
  },
  // Lista de cartões RFID que têm permissão para abrir a porta
  CARTOES_AUTORIZADOS: [
    "01:02:03:04", // Blue Card (Wokwi)
    "11:22:33:44", // Green Card (Wokwi)
  ],
};

// ============================================================================
//  2. INICIALIZAÇÃO DO SERVIDOR WEB (Express + Socket.io)
// ============================================================================
const app = express();
const server = http.createServer(app);
const io = new Server(server, {
  cors: { origin: "*" }, // Permite que qualquer origem (ex: localhost) se conecte
});

// Serve os arquivos estáticos do Frontend (HTML/CSS/JS)
app.use(express.static(path.join(__dirname, "public")));

// ============================================================================
//  3. BANCO DE DADOS (SQLite)
// ============================================================================
/**
 * Inicializa o banco de dados e cria as tabelas necessárias.
 */
const db = new sqlite3.Database("./database.db", (err) => {
  if (err) {
    console.error("[DB] Erro crítico ao abrir o banco:", err.message);
  } else {
    console.log("[DB] Conectado ao SQLite com sucesso.");

    // Tabela para os dados dos sensores ambientais
    db.run(`
      CREATE TABLE IF NOT EXISTS sensores (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        porta_aberta BOOLEAN,
        distancia_cm REAL,
        pir_movimento BOOLEAN,
        ts_ms INTEGER,
        data_hora DATETIME DEFAULT CURRENT_TIMESTAMP
      )
    `);

    // Tabela para o histórico de acessos RFID
    db.run(`
      CREATE TABLE IF NOT EXISTS acessos_rfid (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        uid TEXT,
        autorizado BOOLEAN,
        data_hora DATETIME DEFAULT CURRENT_TIMESTAMP
      )
    `);
  }
});

// ============================================================================
//  4. GERENCIADOR DE DISPOSITIVOS MQTT
// ============================================================================
console.log(`[MQTT] Conectando ao broker ${CONFIG.MQTT_URL}...`);
const mqttClient = mqtt.connect(CONFIG.MQTT_URL);

mqttClient.on("connect", () => {
  console.log("[MQTT] Conectado ao Broker!");

  // Inscreve-se nos tópicos definidos na configuração
  mqttClient.subscribe(CONFIG.TOPICOS.SENSORES, { qos: 1 }, (err) => {
    if (!err) console.log(`[MQTT] Inscrito: ${CONFIG.TOPICOS.SENSORES}`);
  });

  mqttClient.subscribe(CONFIG.TOPICOS.RFID, { qos: 1 }, (err) => {
    if (!err) console.log(`[MQTT] Inscrito: ${CONFIG.TOPICOS.RFID}`);
  });
});

/**
 * Roteador de mensagens MQTT.
 * Direciona a mensagem recebida para a função correta com base no tópico.
 */
mqttClient.on("message", (topic, message) => {
  try {
    if (topic === CONFIG.TOPICOS.SENSORES) {
      processarDadosSensores(message.toString());
    } else if (topic === CONFIG.TOPICOS.RFID) {
      processarDadosRfid(message.toString());
    }
  } catch (error) {
    console.error(
      `[ERRO] Falha ao processar mensagem do tópico ${topic}:`,
      error.message,
    );
  }
});

// ============================================================================
//  5. LÓGICA DE NEGÓCIO (Processamento de Dados)
// ============================================================================

/**
 * Processa leituras dos sensores ambientais (JSON).
 * Salva no banco e emite para o frontend.
 * @param {string} payloadString - A mensagem bruta recebida do ESP32.
 */
function processarDadosSensores(payloadString) {
  const dados = JSON.parse(payloadString);

  // 1. Salva no Banco de Dados
  const stmt = db.prepare(`
    INSERT INTO sensores (porta_aberta, distancia_cm, pir_movimento, ts_ms)
    VALUES (?, ?, ?, ?)
  `);

  stmt.run(
    dados.porta_aberta ? 1 : 0,
    dados.distancia_cm !== null ? dados.distancia_cm : -1, // -1 para "fora de alcance"
    dados.pir_movimento ? 1 : 0,
    dados.ts_ms,
  );
  stmt.finalize();

  // 2. Emite em tempo real para o Dashboard
  io.emit("nova_leitura", dados);
}

/**
 * Processa a leitura de um cartão RFID.
 * Verifica se está na lista de autorizados, salva o log e emite o alerta.
 * @param {string} uidRecebido - O UID do cartão (ex: "01:02:03:04").
 */
function processarDadosRfid(uidRecebido) {
  const uid = uidRecebido.trim().toUpperCase();
  const autorizado = CONFIG.CARTOES_AUTORIZADOS.includes(uid);

  console.log(
    `[RFID] Cartão lido: ${uid} | Status: ${autorizado ? "✅ AUTORIZADO" : "❌ NEGADO"}`,
  );

  // 1. Salva no Banco de Dados
  const stmt = db.prepare(`
    INSERT INTO acessos_rfid (uid, autorizado)
    VALUES (?, ?)
  `);
  stmt.run(uid, autorizado ? 1 : 0);
  stmt.finalize();

  // 2. Emite em tempo real para o Dashboard
  io.emit("acesso_rfid", { uid: uid, autorizado: autorizado });
}

// ============================================================================
//  6. API HTTP (Para o Frontend buscar dados históricos)
// ============================================================================

// Retorna as últimas 50 leituras de sensores
app.get("/api/historico/sensores", (req, res) => {
  db.all(
    "SELECT * FROM sensores ORDER BY id DESC LIMIT 50",
    [],
    (err, rows) => {
      if (err) return res.status(500).json({ error: err.message });
      res.json(rows.reverse()); // Retorna em ordem cronológica
    },
  );
});

// Retorna os últimos 50 acessos de RFID
app.get("/api/historico/rfid", (req, res) => {
  db.all(
    "SELECT * FROM acessos_rfid ORDER BY id DESC LIMIT 50",
    [],
    (err, rows) => {
      if (err) return res.status(500).json({ error: err.message });
      res.json(rows.reverse());
    },
  );
});

// ============================================================================
//  7. INICIALIZAÇÃO DO SERVIDOR WEB
// ============================================================================
server.listen(CONFIG.PORT, () => {
  console.log(`\n[SERVER] Servidor rodando com sucesso!`);
  console.log(
    `[SERVER] Acesse o Dashboard em: http://localhost:${CONFIG.PORT}\n`,
  );
});
