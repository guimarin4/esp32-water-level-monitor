
// ======================================================
// TESTE SIMPLES - ESP32 + JSN-SR04T
// TRIG -> GPIO 5
// ECHO -> GPIO 18 (via divisor de tensão)
// ======================================================

#define TRIG_PIN 5
#define ECHO_PIN 18

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  digitalWrite(TRIG_PIN, LOW);

  Serial.println("Teste JSN-SR04T iniciado");
}

void loop() {
  // Garante que o TRIG começa em LOW
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // Envia pulso de 10 us
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Mede o tempo que o ECHO fica em HIGH
  // Timeout de 30 ms para evitar travamento
  unsigned long duracao = pulseIn(ECHO_PIN, HIGH, 30000);

  // Se não recebeu eco
  if (duracao == 0) {
    Serial.println("ERRO: nenhum eco recebido");
  } else {
    // Velocidade do som ≈ 0,0343 cm/us
    // Divide por 2 porque o som vai e volta
    float distancia = duracao * 0.0343 / 2.0;

    Serial.print("Distancia: ");
    Serial.print(distancia, 1);
    Serial.println(" cm");
  }

  delay(500);
}
