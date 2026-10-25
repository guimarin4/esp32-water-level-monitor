const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

// Run the actual embedded dashboard script with a minimal DOM and fake requests.
const source = fs.readFileSync(path.join(__dirname, "..", "src", "main.cpp"), "utf8");
const script = source.match(/<script>([\s\S]*?)<\/script>/)[1];
const elements = new Map();
const document = {
  getElementById(id) {
    if (!elements.has(id)) {
      let text = "";
      elements.set(id, {
        style: {}, hidden: true, className: "",
        get innerHTML() { return text; },
        set innerHTML(value) { text = String(value); },
        get textContent() { return text; },
        set textContent(value) { text = String(value); }
      });
    }
    return elements.get(id);
  }
};

let data = {
  estado: "ok", temDados: true, leituraValida: true,
  distancia: 30, altura: 15, nivel: 50,
  amostrasValidas: 5, totalAmostras: 5, segundosDesdeMedicao: 0,
  alertaCalibracao: true, emailHabilitado: false, emailEnviado: false
};
let offline = false;
const context = vm.createContext({
  document,
  fetch: async () => {
    if (offline) throw new Error("Offline");
    return { json: async () => data };
  },
  setInterval: () => {}, setTimeout: () => {}, confirm: () => false
});

async function refresh() {
  context.atualizarDados();
  await new Promise(resolve => setImmediate(resolve));
}

(async () => {
  vm.runInContext(script, context);
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(document.getElementById("mainLevel").textContent, "50.0%");
  assert.match(document.getElementById("status").textContent, /Sensor OK/);
  assert.equal(document.getElementById("calibrationWarning").hidden, false);
  assert.equal(document.getElementById("emailStatus").textContent, "Desativado para testes");

  data = { ...data, estado: "suspeita", leituraValida: false,
    amostrasValidas: 2, segundosDesdeMedicao: 12 };
  await refresh();
  assert.equal(document.getElementById("mainLevel").textContent, "50.0%");
  assert.equal(document.getElementById("distance").textContent, "30.0 cm");
  assert.match(document.getElementById("status").textContent, /Sem leitura confiável.*2\/5/);
  assert.equal(document.getElementById("lastMeasurement").textContent, "12 s atrás (desatualizada)");
  assert.equal(document.getElementById("mainLevel").style.opacity, "0.5");

  data = { ...data, estado: "erro", temDados: false, amostrasValidas: 0,
    nivel: null, distancia: null, altura: null, segundosDesdeMedicao: null };
  await refresh();
  assert.equal(document.getElementById("mainLevel").textContent, "--%");
  assert.equal(document.getElementById("distance").textContent, "-- cm");
  assert.equal(document.getElementById("water").style.height, "0%");
  assert.equal(document.getElementById("lastMeasurement").textContent, "Nenhuma leitura válida");
  assert.match(document.getElementById("status").textContent, /Sem eco/);

  data = { ...data, estado: "ok", temDados: true, leituraValida: true,
    nivel: 40, distancia: 33, altura: 12, segundosDesdeMedicao: 0,
    alertaCalibracao: false, amostrasValidas: 5 };
  await refresh();
  assert.equal(document.getElementById("mainLevel").textContent, "40.0%");
  assert.equal(document.getElementById("mainLevel").style.opacity, "1");
  assert.equal(document.getElementById("calibrationWarning").hidden, true);

  data = { ...data, leituraValida: false, segundosDesdeMedicao: 8 };
  await refresh();
  assert.match(document.getElementById("status").textContent, /Leitura desatualizada/);

  offline = true;
  await refresh();
  assert.match(document.getElementById("status").textContent, /ESP32 desconectado/);
  assert.equal(document.getElementById("mainLevel").textContent, "40.0%");
  assert.equal(document.getElementById("lastMeasurement").textContent, "Sem conexão para atualizar");
  console.log("Dashboard: leitura válida, falha, início sem dados, recuperação e desconexão passaram.");
})().catch(error => { console.error(error); process.exitCode = 1; });
