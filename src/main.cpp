#include <Arduino.h>
#include <EMailSender.h>
#include <WebServer.h>
#include <WiFi.h>

#include "secrets.h"

// ======================================================
// WI-FI
// ======================================================

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// ======================================================
// PINOS DO JSN-SR04T
// ======================================================

#define TRIG_PIN 5
#define ECHO_PIN 18

// ======================================================
// CONFIGURAÇÃO DA CAIXA
// ======================================================

// Distância sensor -> água quando cheia
const float DISTANCIA_CHEIA = 15.0;

// Altura útil máxima da água
const float ALTURA_AGUA = 30.0;

// Sensor -> fundo
const float DISTANCIA_VAZIA = DISTANCIA_CHEIA + ALTURA_AGUA;  // 45 cm

// ======================================================
// CONFIGURAÇÃO DE MONITORAMENTO
// ======================================================

// Uma medição a cada 30 segundos
const unsigned long INTERVALO_MEDICAO = 30000;

// Leituras acima disso são consideradas suspeitas
const float DISTANCIA_ESTRANHA = 200.0;

// Número de leituras suspeitas consecutivas necessárias
// para assumir que a água está na zona cega do sensor
const int LIMITE_LEITURAS_ESTRANHAS = 3;

// Alerta de nível baixo
const float NIVEL_ALERTA = 50.0;

// Rearma o alerta somente depois de voltar para 55%
const float NIVEL_REARME_EMAIL = 55.0;

// ======================================================
// SERVIDOR WEB
// ======================================================

WebServer server(80);

// ======================================================
// ESTADOS DO SENSOR
// ======================================================

enum EstadoSensor {
    ESTADO_INICIANDO,
    ESTADO_OK,
    ESTADO_SUSPEITA,
    ESTADO_CHEIO_ESTIMADO,
    ESTADO_ERRO
};

EstadoSensor estadoSensor = ESTADO_INICIANDO;

// ======================================================
// VARIÁVEIS
// ======================================================

float distanciaAtual = 0.0;
float nivelAtual = 0.0;
float alturaAguaAtual = 0.0;

bool dadosDisponiveis = false;

int leiturasEstranhas = 0;

bool emailAlertaEnviado = false;

unsigned long ultimaMedicaoMillis = 0;

// ======================================================
// CONFIGURAÇÃO DO E-MAIL
// ======================================================

EMailSender emailSend(EMAIL_REMETENTE, EMAIL_APP_PASSWORD, EMAIL_REMETENTE, "Water Tank Monitor",
                      "smtp.gmail.com", 587);

// ======================================================
// CONVERTER ESTADO PARA TEXTO
// ======================================================

const char* estadoParaTexto() {
    switch (estadoSensor) {
        case ESTADO_OK:
            return "ok";

        case ESTADO_SUSPEITA:
            return "suspeita";

        case ESTADO_CHEIO_ESTIMADO:
            return "estimado";

        case ESTADO_ERRO:
            return "erro";

        default:
            return "iniciando";
    }
}

// ======================================================
// MEDIR DISTÂNCIA
// ======================================================

float medirDistancia() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);

    digitalWrite(TRIG_PIN, LOW);

    unsigned long duracao = pulseIn(ECHO_PIN, HIGH, 30000);

    // Nenhum eco recebido
    if (duracao == 0) {
        return -1;
    }

    // Velocidade do som ≈ 0.0343 cm/us
    float distancia = duracao * 0.0343 / 2.0;

    return distancia;
}

// ======================================================
// CALCULAR NÍVEL
// ======================================================

float calcularNivel(float distancia) {
    float nivel = ((DISTANCIA_VAZIA - distancia) / ALTURA_AGUA) * 100.0;

    if (nivel > 100.0) {
        nivel = 100.0;
    }

    if (nivel < 0.0) {
        nivel = 0.0;
    }

    return nivel;
}

// ======================================================
// CALCULAR ALTURA DA ÁGUA
// ======================================================

float calcularAlturaAgua(float distancia) {
    float altura = DISTANCIA_VAZIA - distancia;

    if (altura > ALTURA_AGUA) {
        altura = ALTURA_AGUA;
    }

    if (altura < 0.0) {
        altura = 0.0;
    }

    return altura;
}

// ======================================================
// ENVIAR E-MAIL
// ======================================================

bool enviarEmailNivelBaixo() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Nao foi possivel enviar e-mail: Wi-Fi desconectado.");

        return false;
    }

    Serial.println();
    Serial.println("Enviando alerta por e-mail...");

    EMailSender::EMailMessage mensagem;

    mensagem.subject = "Alerta - Nivel baixo da caixa d'agua";

    String corpo = "Water Tank Monitor\n\n";

    corpo += "O nivel da caixa d'agua esta abaixo de 50%.\n\n";

    corpo += "Nivel atual: ";

    corpo += String(nivelAtual, 1);

    corpo += "%\n";

    corpo += "Altura da agua: ";

    corpo += String(alturaAguaAtual, 1);

    corpo += " cm\n";

    corpo += "Distancia medida: ";

    corpo += String(distanciaAtual, 1);

    corpo += " cm\n";

    mensagem.message = corpo;

    EMailSender::Response resposta = emailSend.send(EMAIL_DESTINO, mensagem);

    Serial.print("Codigo SMTP: ");
    Serial.println(resposta.code);

    Serial.print("Resposta: ");
    Serial.println(resposta.desc);

    Serial.print("Status: ");
    Serial.println(resposta.status ? "SUCESSO" : "ERRO");

    return resposta.status;
}

// ======================================================
// VERIFICAR ALERTA DE NÍVEL
// ======================================================

void verificarAlertaEmail() {
    // ==================================================
    // REARME
    // ==================================================

    if (nivelAtual >= NIVEL_REARME_EMAIL && emailAlertaEnviado) {
        emailAlertaEnviado = false;

        Serial.println(
            "Nivel voltou acima de 55%. "
            "Alerta de e-mail rearmado.");
    }

    // ==================================================
    // ALERTA
    // ==================================================

    if (nivelAtual < NIVEL_ALERTA && !emailAlertaEnviado) {
        Serial.println("Nivel abaixo de 50%.");

        bool enviado = enviarEmailNivelBaixo();

        if (enviado) {
            emailAlertaEnviado = true;

            Serial.println("Alerta enviado com sucesso.");

        } else {
            Serial.println("Falha no envio do alerta.");
        }
    }
}

// ======================================================
// EXECUTAR MEDIÇÃO
// ======================================================

void executarMedicao() {
    ultimaMedicaoMillis = millis();

    float distancia = medirDistancia();

    Serial.println();
    Serial.println("============================");
    Serial.println("Nova medicao");

    // ==================================================
    // LEITURA NORMAL
    // ==================================================

    if (distancia > 0 && distancia <= DISTANCIA_ESTRANHA) {
        leiturasEstranhas = 0;

        estadoSensor = ESTADO_OK;

        dadosDisponiveis = true;

        distanciaAtual = distancia;

        nivelAtual = calcularNivel(distancia);

        alturaAguaAtual = calcularAlturaAgua(distancia);

        Serial.print("Distancia: ");
        Serial.print(distanciaAtual, 1);
        Serial.println(" cm");

        Serial.print("Altura agua: ");
        Serial.print(alturaAguaAtual, 1);
        Serial.println(" cm");

        Serial.print("Nivel: ");
        Serial.print(nivelAtual, 1);
        Serial.println(" %");

        verificarAlertaEmail();

        return;
    }

    // ==================================================
    // LEITURA MUITO ALTA
    // ==================================================

    if (distancia > DISTANCIA_ESTRANHA) {
        leiturasEstranhas++;

        estadoSensor = ESTADO_SUSPEITA;

        distanciaAtual = distancia;

        Serial.print("Leitura suspeita: ");

        Serial.print(distancia, 1);

        Serial.println(" cm");

        Serial.print("Contagem: ");

        Serial.print(leiturasEstranhas);

        Serial.print("/");

        Serial.println(LIMITE_LEITURAS_ESTRANHAS);

        // ==============================================
        // TRÊS LEITURAS SUSPEITAS CONSECUTIVAS
        // ==============================================

        if (leiturasEstranhas >= LIMITE_LEITURAS_ESTRANHAS) {
            estadoSensor = ESTADO_CHEIO_ESTIMADO;

            dadosDisponiveis = true;

            nivelAtual = 100.0;

            alturaAguaAtual = ALTURA_AGUA;

            Serial.println("Caixa considerada CHEIA.");

            Serial.println("Nivel = 100% (estimado)");

            // Se anteriormente houve alerta,
            // 100% rearma o sistema
            verificarAlertaEmail();
        }

        return;
    }

    // ==================================================
    // TIMEOUT / SEM ECO
    // ==================================================

    estadoSensor = ESTADO_ERRO;

    leiturasEstranhas = 0;

    Serial.println("ERRO: nenhum eco recebido.");
}

// ======================================================
// PÁGINA HTML
// ======================================================

const char paginaHTML[] PROGMEM = R"rawliteral(

<!DOCTYPE html>
<html lang="pt-BR">

<head>

<meta charset="UTF-8">

<meta
  name="viewport"
  content="width=device-width, initial-scale=1.0"
>

<title>Water Tank Monitor</title>

<style>

* {
  box-sizing: border-box;
}

body {
  margin: 0;
  min-height: 100vh;

  display: flex;
  justify-content: center;
  align-items: center;

  font-family: Arial, Helvetica, sans-serif;

  background:
    linear-gradient(
      135deg,
      #0f172a,
      #1e293b
    );

  color: white;
}

.dashboard {
  width: 92%;
  max-width: 850px;

  padding: 35px;

  background:
    rgba(255,255,255,0.07);

  border:
    1px solid
    rgba(255,255,255,0.12);

  border-radius: 24px;

  box-shadow:
    0 20px 60px
    rgba(0,0,0,0.35);
}

.header {
  display: flex;

  justify-content: space-between;
  align-items: center;

  margin-bottom: 35px;
}

.title h1 {
  margin: 0;
  font-size: 28px;
}

.title p {
  margin-top: 5px;
  color: #94a3b8;
}

.status {
  padding: 9px 15px;

  border-radius: 20px;

  font-size: 14px;

  background: #334155;
}

.status.ok {
  background:
    rgba(34,197,94,0.2);

  color: #4ade80;
}

.status.warning {
  background:
    rgba(245,158,11,0.2);

  color: #fbbf24;
}

.status.error {
  background:
    rgba(239,68,68,0.2);

  color: #f87171;
}

.status.estimated {
  background:
    rgba(56,189,248,0.2);

  color: #38bdf8;
}

.content {
  display: grid;

  grid-template-columns:
    1fr 1.5fr;

  gap: 50px;

  align-items: center;
}

.tank-wrapper {
  display: flex;
  justify-content: center;
  align-items: center;
}

.tank {
  position: relative;

  width: 180px;
  height: 300px;

  border:
    5px solid #cbd5e1;

  border-radius:
    12px 12px 30px 30px;

  overflow: hidden;

  background:
    rgba(255,255,255,0.05);

  box-shadow:
    inset 0 0 20px
    rgba(0,0,0,0.4);
}

.water {
  position: absolute;

  bottom: 0;

  width: 100%;
  height: 0%;

  background:
    linear-gradient(
      to top,
      #0284c7,
      #38bdf8
    );

  transition:
    height 0.8s ease,
    background 0.5s ease;
}

.water::before {
  content: "";

  position: absolute;

  top: 0;
  left: 0;

  width: 100%;
  height: 6px;

  background:
    rgba(255,255,255,0.45);
}

.level-text {
  position: absolute;

  top: 50%;
  left: 50%;

  transform:
    translate(-50%, -50%);

  z-index: 5;

  font-size: 36px;

  font-weight: bold;

  text-shadow:
    0 2px 8px
    rgba(0,0,0,0.8);
}

.data-area h2 {
  margin-top: 0;

  font-size: 22px;

  color: #cbd5e1;
}

.main-level {
  font-size: 70px;

  font-weight: bold;

  margin-bottom: 25px;
}

.cards {
  display: grid;

  grid-template-columns:
    repeat(2, 1fr);

  gap: 15px;
}

.card {
  padding: 18px;

  border-radius: 16px;

  background:
    rgba(255,255,255,0.06);
}

.card-title {
  color: #94a3b8;

  font-size: 13px;

  margin-bottom: 8px;
}

.card-value {
  font-size: 20px;

  font-weight: bold;
}

.restart-button {
  display: block;

  margin: 30px auto 0;

  padding: 12px 22px;

  border: none;

  border-radius: 10px;

  background: #334155;

  color: white;

  cursor: pointer;

  font-size: 14px;

  transition:
    background 0.2s;
}

.restart-button:hover {
  background: #475569;
}

.footer {
  margin-top: 25px;

  text-align: center;

  color: #64748b;

  font-size: 13px;
}

@media(max-width: 650px) {

  .dashboard {
    padding: 25px;
  }

  .header {
    flex-direction: column;

    align-items: flex-start;

    gap: 15px;
  }

  .content {
    grid-template-columns: 1fr;
  }

  .tank {
    width: 150px;
    height: 250px;
  }

  .main-level {
    text-align: center;

    font-size: 55px;
  }

  .cards {
    grid-template-columns: 1fr;
  }
}

</style>

</head>

<body>

<div class="dashboard">

  <div class="header">

    <div class="title">

      <h1>
        Water Tank Monitor
      </h1>

      <p>
        ESP32 IoT Monitoring System
      </p>

    </div>

    <div
      id="status"
      class="status"
    >
      Iniciando...
    </div>

  </div>


  <div class="content">

    <div class="tank-wrapper">

      <div class="tank">

        <div
          id="water"
          class="water"
        >
        </div>

        <div
          id="tankLevel"
          class="level-text"
        >
          --%
        </div>

      </div>

    </div>


    <div class="data-area">

      <h2>
        Nível atual
      </h2>

      <div
        id="mainLevel"
        class="main-level"
      >
        --%
      </div>


      <div class="cards">

        <div class="card">

          <div class="card-title">
            ALTURA DA ÁGUA
          </div>

          <div
            id="waterHeight"
            class="card-value"
          >
            -- cm
          </div>

        </div>


        <div class="card">

          <div class="card-title">
            DISTÂNCIA DO SENSOR
          </div>

          <div
            id="distance"
            class="card-value"
          >
            -- cm
          </div>

        </div>


        <div class="card">

          <div class="card-title">
            ÚLTIMA MEDIÇÃO
          </div>

          <div
            id="lastMeasurement"
            class="card-value"
          >
            --
          </div>

        </div>


        <div class="card">

          <div class="card-title">
            ALERTA DE E-MAIL
          </div>

          <div
            id="emailStatus"
            class="card-value"
          >
            --
          </div>

        </div>

      </div>

    </div>

  </div>


  <button
    class="restart-button"
    onclick="reiniciarESP()"
  >
    Reiniciar ESP32
  </button>


  <div class="footer">
    ESP32 Water Level Monitoring System
  </div>

</div>


<script>

// ======================================================
// COR DO NÍVEL
// ======================================================

function atualizarCor(nivel) {

  const agua =
    document.getElementById("water");

  const nivelPrincipal =
    document.getElementById("mainLevel");

  if (nivel < 20) {

    agua.style.background =
      "linear-gradient(to top, #b91c1c, #ef4444)";

    nivelPrincipal.style.color =
      "#f87171";

  }

  else if (nivel < 50) {

    agua.style.background =
      "linear-gradient(to top, #d97706, #fbbf24)";

    nivelPrincipal.style.color =
      "#fbbf24";

  }

  else {

    agua.style.background =
      "linear-gradient(to top, #0284c7, #38bdf8)";

    nivelPrincipal.style.color =
      "#38bdf8";
  }
}


// ======================================================
// STATUS
// ======================================================

function atualizarStatus(data) {

  const status =
    document.getElementById("status");

  if (data.estado === "ok") {

    status.innerHTML =
      "● Sensor OK";

    status.className =
      "status ok";

  }

  else if (data.estado === "suspeita") {

    status.innerHTML =
      "● Leitura suspeita "
      + data.leiturasEstranhas
      + "/3";

    status.className =
      "status warning";

  }

  else if (data.estado === "estimado") {

    status.innerHTML =
      "● Caixa cheia (estimado)";

    status.className =
      "status estimated";

  }

  else if (data.estado === "erro") {

    status.innerHTML =
      "● Erro no sensor";

    status.className =
      "status error";

  }

  else {

    status.innerHTML =
      "Iniciando...";

    status.className =
      "status";
  }
}


// ======================================================
// BUSCAR DADOS
// ======================================================

function atualizarDados() {

  fetch(
    "/dados",
    {
      cache: "no-store"
    }
  )

  .then(
    response =>
      response.json()
  )

  .then(
    data => {

      atualizarStatus(data);

      // =================================================
      // DADOS DISPONÍVEIS
      // =================================================

      if (data.temDados) {

        const nivel =
          data.nivel;

        document.getElementById(
          "mainLevel"
        ).innerHTML =
          nivel.toFixed(1)
          + "%";

        document.getElementById(
          "tankLevel"
        ).innerHTML =
          nivel.toFixed(0)
          + "%";

        document.getElementById(
          "water"
        ).style.height =
          nivel
          + "%";

        document.getElementById(
          "waterHeight"
        ).innerHTML =
          data.altura.toFixed(1)
          + " cm";

        document.getElementById(
          "distance"
        ).innerHTML =
          data.distancia.toFixed(1)
          + " cm";

        atualizarCor(nivel);
      }


      // =================================================
      // ÚLTIMA MEDIÇÃO
      // =================================================

      document.getElementById(
        "lastMeasurement"
      ).innerHTML =
        data.segundosDesdeMedicao
        + " s atrás";


      // =================================================
      // E-MAIL
      // =================================================

      if (data.emailEnviado) {

        document.getElementById(
          "emailStatus"
        ).innerHTML =
          "Alerta enviado";

      } else {

        document.getElementById(
          "emailStatus"
        ).innerHTML =
          "Monitorando";
      }

    }
  )

  .catch(
    error => {

      const status =
        document.getElementById(
          "status"
        );

      status.innerHTML =
        "● ESP32 desconectado";

      status.className =
        "status error";

    }
  );
}


// ======================================================
// REINICIAR ESP32
// ======================================================

function reiniciarESP() {

  const confirmar =
    confirm(
      "Deseja realmente reiniciar o ESP32?"
    );

  if (!confirmar) {
    return;
  }

  const status =
    document.getElementById(
      "status"
    );

  status.innerHTML =
    "Reiniciando...";

  status.className =
    "status warning";


  fetch(
    "/restart",
    {
      method: "POST"
    }
  )

  .then(() => {

    setTimeout(
      () => {
        location.reload();
      },
      5000
    );

  });

}


// Busca imediatamente
atualizarDados();

// Página consulta o ESP32 a cada 2 segundos.
// Isso NÃO significa que o sensor mede a cada 2 segundos.
setInterval(
  atualizarDados,
  2000
);

</script>

</body>

</html>

)rawliteral";

// ======================================================
// CONFIGURAR ROTAS DO SERVIDOR
// ======================================================

void configurarServidor() {
    // ==================================================
    // PÁGINA PRINCIPAL
    // ==================================================

    server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", paginaHTML); });

    // ==================================================
    // DADOS JSON
    // ==================================================

    server.on("/dados", HTTP_GET, []() {
        unsigned long segundosDesdeMedicao = (millis() - ultimaMedicaoMillis) / 1000;

        String json;

        json.reserve(300);

        json += "{";

        json += "\"distancia\":";

        json += String(distanciaAtual, 1);

        json += ",";

        json += "\"nivel\":";

        json += String(nivelAtual, 1);

        json += ",";

        json += "\"altura\":";

        json += String(alturaAguaAtual, 1);

        json += ",";

        json += "\"estado\":\"";

        json += estadoParaTexto();

        json += "\",";

        json += "\"temDados\":";

        json += (dadosDisponiveis ? "true" : "false");

        json += ",";

        json += "\"leiturasEstranhas\":";

        json += String(leiturasEstranhas);

        json += ",";

        json += "\"segundosDesdeMedicao\":";

        json += String(segundosDesdeMedicao);

        json += ",";

        json += "\"emailEnviado\":";

        json += (emailAlertaEnviado ? "true" : "false");

        json += "}";

        server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");

        server.send(200, "application/json", json);
    });

    // ==================================================
    // REINICIAR ESP32
    // ==================================================

    server.on("/restart", HTTP_POST, []() {
        Serial.println();
        Serial.println("Reinicio solicitado pela pagina WEB.");

        server.send(200, "text/plain", "Reiniciando ESP32...");

        delay(500);

        ESP.restart();
    });
}

// ======================================================
// SETUP
// ======================================================

void setup() {
    Serial.begin(115200);

    pinMode(TRIG_PIN, OUTPUT);

    pinMode(ECHO_PIN, INPUT);

    digitalWrite(TRIG_PIN, LOW);

    // ==================================================
    // WI-FI
    // ==================================================

    WiFi.mode(WIFI_STA);

    // Não grava constantemente configuração na flash
    WiFi.persistent(false);

    // Tenta reconectar automaticamente
    WiFi.setAutoReconnect(true);

    Serial.println();
    Serial.print("Conectando ao Wi-Fi");

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);

        Serial.print(".");
    }

    Serial.println();
    Serial.println("Wi-Fi conectado!");

    Serial.print("Endereco IP: ");

    Serial.println(WiFi.localIP());

    // ==================================================
    // SERVIDOR
    // ==================================================

    configurarServidor();

    server.begin();

    Serial.println("Servidor WEB iniciado!");

    // ==================================================
    // PRIMEIRA MEDIÇÃO
    // ==================================================

    // Não espera 30 segundos após ligar.
    executarMedicao();
}

// ======================================================
// LOOP
// ======================================================

void loop() {
    // Mantém o servidor WEB respondendo continuamente
    server.handleClient();

    // ==================================================
    // MEDIÇÃO A CADA 30 SEGUNDOS
    // ==================================================

    if (millis() - ultimaMedicaoMillis >= INTERVALO_MEDICAO) {
        executarMedicao();
    }
}