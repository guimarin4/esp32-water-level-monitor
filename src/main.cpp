#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

// ======================================================
// CONFIGURAÇÃO DO WI-FI
// ======================================================

const char* ssid = "LIGGA-MARIN-2.4G";
const char* password = "@Bodasdeprata25";

// ======================================================
// PINOS DO JSN-SR04T
// ======================================================

#define TRIG_PIN 5
#define ECHO_PIN 18

// ======================================================
// CONFIGURAÇÃO DA CAIXA
// ======================================================

// Distância entre o sensor e a água quando a caixa está cheia
const float DISTANCIA_CHEIA = 15.0;

// Altura máxima da coluna de água
const float ALTURA_AGUA = 30.0;

// Distância sensor -> fundo da caixa
const float DISTANCIA_VAZIA = DISTANCIA_CHEIA + ALTURA_AGUA;  // 45 cm

// ======================================================
// SERVIDOR WEB
// ======================================================

WebServer server(80);

// ======================================================
// VARIÁVEIS
// ======================================================

float distanciaAtual = 0.0;
float nivelAtual = 0.0;
float alturaAguaAtual = 0.0;

bool sensorOK = false;

// ======================================================
// MEDIR DISTÂNCIA
// ======================================================

float medirDistancia() {
    // Garante TRIG em LOW
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    // Pulso de 10 us
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // Mede duração do pulso ECHO
    unsigned long duracao = pulseIn(ECHO_PIN, HIGH, 30000);

    // Timeout
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

    // Limita entre 0 e 100%
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
// PÁGINA WEB
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
  align-items: center;
  justify-content: center;

  font-family:
    Arial,
    Helvetica,
    sans-serif;

  background:
    linear-gradient(
      135deg,
      #0f172a,
      #1e293b
    );

  color: white;
}

/* ============================== */
/* CONTAINER PRINCIPAL            */
/* ============================== */

.dashboard {

  width: 92%;
  max-width: 850px;

  background:
    rgba(
      255,
      255,
      255,
      0.07
    );

  border:
    1px solid
    rgba(
      255,
      255,
      255,
      0.12
    );

  border-radius: 24px;

  padding: 35px;

  box-shadow:
    0 20px 60px
    rgba(0, 0, 0, 0.35);

}

/* ============================== */
/* CABEÇALHO                      */
/* ============================== */

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

/* ============================== */
/* STATUS DO SENSOR               */
/* ============================== */

.status {

  padding:
    8px 14px;

  border-radius:
    20px;

  font-size:
    14px;

  background:
    #334155;

}

.status.ok {

  background:
    rgba(
      34,
      197,
      94,
      0.2
    );

  color:
    #4ade80;

}

.status.error {

  background:
    rgba(
      239,
      68,
      68,
      0.2
    );

  color:
    #f87171;

}

/* ============================== */
/* ÁREA PRINCIPAL                 */
/* ============================== */

.content {

  display: grid;

  grid-template-columns:
    1fr 1.5fr;

  gap: 50px;

  align-items: center;

}

/* ============================== */
/* TANQUE                         */
/* ============================== */

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
    5px solid
    #cbd5e1;

  border-radius:
    12px 12px
    30px 30px;

  overflow: hidden;

  background:
    rgba(
      255,
      255,
      255,
      0.05
    );

  box-shadow:
    inset 0 0 20px
    rgba(
      0,
      0,
      0,
      0.4
    );

}

/* ============================== */
/* ÁGUA                           */
/* ============================== */

.water {

  position:
    absolute;

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

/* efeito de brilho */

.water::before {

  content: "";

  position:
    absolute;

  top: 0;
  left: 0;

  width: 100%;
  height: 6px;

  background:
    rgba(
      255,
      255,
      255,
      0.45
    );

}

/* ============================== */
/* PORCENTAGEM NO TANQUE          */
/* ============================== */

.level-text {

  position:
    absolute;

  top: 50%;
  left: 50%;

  transform:
    translate(
      -50%,
      -50%
    );

  z-index: 5;

  font-size:
    36px;

  font-weight:
    bold;

  text-shadow:
    0 2px 8px
    rgba(
      0,
      0,
      0,
      0.8
    );

}

/* ============================== */
/* DADOS                          */
/* ============================== */

.data-area h2 {

  margin-top: 0;

  font-size:
    22px;

  color:
    #cbd5e1;

}

.main-level {

  font-size:
    70px;

  font-weight:
    bold;

  margin-bottom:
    25px;

}

.cards {

  display:
    grid;

  grid-template-columns:
    repeat(
      2,
      1fr
    );

  gap:
    15px;

}

.card {

  padding:
    18px;

  border-radius:
    16px;

  background:
    rgba(
      255,
      255,
      255,
      0.06
    );

}

.card-title {

  color:
    #94a3b8;

  font-size:
    13px;

  margin-bottom:
    8px;

}

.card-value {

  font-size:
    22px;

  font-weight:
    bold;

}

/* ============================== */
/* RODAPÉ                         */
/* ============================== */

.footer {

  margin-top:
    30px;

  text-align:
    center;

  color:
    #64748b;

  font-size:
    13px;

}

/* ============================== */
/* RESPONSIVO                     */
/* ============================== */

@media
(max-width: 650px) {

  .dashboard {

    padding:
      25px;

  }

  .header {

    flex-direction:
      column;

    align-items:
      flex-start;

    gap:
      15px;

  }

  .content {

    grid-template-columns:
      1fr;

  }

  .main-level {

    text-align:
      center;

    font-size:
      55px;

  }

  .cards {

    grid-template-columns:
      1fr;

  }

  .tank {

    width:
      150px;

    height:
      250px;

  }

}

</style>

</head>

<body>

<div class="dashboard">

  <!-- HEADER -->

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

      Conectando...

    </div>

  </div>


  <!-- CONTEÚDO -->

  <div class="content">

    <!-- TANQUE -->

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


    <!-- DADOS -->

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
            CAPACIDADE
          </div>

          <div
            id="capacity"
            class="card-value"
          >

            -- %

          </div>

        </div>


        <div class="card">

          <div class="card-title">
            ÚLTIMA ATUALIZAÇÃO
          </div>

          <div
            id="update"
            class="card-value"
          >

            --

          </div>

        </div>

      </div>

    </div>

  </div>


  <div class="footer">

    ESP32 Water Level Monitoring System

  </div>

</div>


<script>

// ======================================================
// DEFINIR COR COM BASE NO NÍVEL
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
// BUSCAR DADOS DO ESP32
// ======================================================

function atualizarDados() {

  fetch("/dados")

  .then(
    response =>
      response.json()
  )

  .then(
    data => {

      const status =
        document.getElementById(
          "status"
        );

      // ==========================
      // SENSOR OK
      // ==========================

      if (data.sensor) {

        status.innerHTML =
          "● Sensor OK";

        status.className =
          "status ok";

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
          "distance"
        ).innerHTML =
          data.distancia.toFixed(1)
          + " cm";

        document.getElementById(
          "waterHeight"
        ).innerHTML =
          data.altura.toFixed(1)
          + " cm";

        document.getElementById(
          "capacity"
        ).innerHTML =
          nivel.toFixed(1)
          + " %";

        document.getElementById(
          "update"
        ).innerHTML =
          new Date()
          .toLocaleTimeString();

        atualizarCor(nivel);

      }

      // ==========================
      // ERRO DO SENSOR
      // ==========================

      else {

        status.innerHTML =
          "● Erro no sensor";

        status.className =
          "status error";

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


// Executa imediatamente
atualizarDados();

// Atualiza a cada 1 segundo
setInterval(
  atualizarDados,
  1000
);

</script>

</body>

</html>

)rawliteral";

// ======================================================
// SETUP
// ======================================================

void setup() {
    Serial.begin(115200);

    pinMode(TRIG_PIN, OUTPUT);

    pinMode(ECHO_PIN, INPUT);

    digitalWrite(TRIG_PIN, LOW);

    // ====================================================
    // WI-FI
    // ====================================================

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

    // ====================================================
    // ROTA PRINCIPAL
    // ====================================================

    server.on("/", []() {
        server.send_P(200, "text/html", paginaHTML);
    });

    // ====================================================
    // API /dados
    // ====================================================

    server.on("/dados", []() {
        String json = "{";

        json += "\"distancia\":";

        json += String(distanciaAtual, 1);

        json += ",";

        json += "\"nivel\":";

        json += String(nivelAtual, 1);

        json += ",";

        json += "\"altura\":";

        json += String(alturaAguaAtual, 1);

        json += ",";

        json += "\"sensor\":";

        if (sensorOK) {
            json += "true";

        } else {
            json += "false";
        }

        json += "}";

        server.sendHeader("Cache-Control", "no-cache");

        server.send(200, "application/json", json);
    });

    // ====================================================
    // INICIA SERVIDOR
    // ====================================================

    server.begin();

    Serial.println("Servidor WEB iniciado!");
}

// ======================================================
// LOOP
// ======================================================

void loop() {
    // Atende requisições HTTP
    server.handleClient();

    // Controle não bloqueante
    static unsigned long ultimaMedicao = 0;

    // Nova medição a cada 1 segundo
    if (millis() - ultimaMedicao >= 1000) {
        ultimaMedicao = millis();

        float distancia = medirDistancia();

        // ==================================================
        // MEDIÇÃO VÁLIDA
        // ==================================================

        if (distancia > 0 && distancia < 500) {
            sensorOK = true;

            distanciaAtual = distancia;

            nivelAtual = calcularNivel(distancia);

            alturaAguaAtual = calcularAlturaAgua(distancia);

            // Serial Monitor

            Serial.print("Distancia: ");

            Serial.print(distanciaAtual, 1);

            Serial.print(" cm | Altura: ");

            Serial.print(alturaAguaAtual, 1);

            Serial.print(" cm | Nivel: ");

            Serial.print(nivelAtual, 1);

            Serial.println(" %");

        }

        // ==================================================
        // ERRO
        // ==================================================

        else {
            sensorOK = false;

            Serial.println("Erro na leitura do sensor");
        }
    }
}