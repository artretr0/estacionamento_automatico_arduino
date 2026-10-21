
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// =====================================
// LCD I2C - ENDERECO 0x27
// =====================================

LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================================
// CONFIGURACOES DO ESTACIONAMENTO
// =====================================

const byte TOTAL_VAGAS = 2;

// PINOS DOS LEDS
const byte pinosLED[2] = {3, 4};

// PINOS DOS BOTOES
const byte pinosBotaoVaga[2] = {10, 11};
const byte pinoBotaoCancela = 12;
const byte pinoBotaoModo = 13;

// PINOS DOS SENSORES E SERVO
const byte pinoLDR = A0;
const byte pinoServo = 9;

// ANGULOS DA CANCELA
const byte anguloAberto = 90;
const byte anguloFechado = 0;

// CONFIGURACAO DO LDR
const int limiteLuz = 175;
const int margemLDR = 15;

// TEMPOS
const unsigned long tempoDebounce = 50;
const unsigned long tempoMensagem = 2000;
const unsigned long intervaloSerial = 1000;

// =====================================
// VARIAVEIS
// =====================================

Servo cancela;

// false = vaga livre
// true = vaga ocupada
bool vagasOcupadas[2] = {false, false};

byte vagasLivres = TOTAL_VAGAS;

// false = manual
// true = automatico
bool modoAutomatico = false;

// ESTADO DA CANCELA
bool cancelaAberta = false;

// SENSOR LDR
int valorLDR = 0;
bool carroDetectado = false;

// BOTOES
const byte TOTAL_BOTOES = 4;

const byte botoes[4] = {
  10, 11, 12, 13
};

bool ultimaLeitura[4];
bool estadoEstavel[4];

unsigned long ultimoDebounce[4] = {
  0, 0, 0, 0
};

// LCD
bool atualizarTela = true;
bool mensagemTemporaria = false;
bool vagaFoiLiberada = false;

byte vagaAlterada = 0;

unsigned long inicioMensagem = 0;
unsigned long ultimoSerial = 0;

// Mensagem inicial sem bloquear o programa
const unsigned long tempoInicializacao = 1500;
unsigned long inicioInicializacao = 0;
bool inicializando = true;

// =====================================
// ESCREVER NO LCD
// =====================================

void escreverLinha(byte linha, const char *texto) {

  lcd.setCursor(0, linha);

  // Limpa a linha
  lcd.print("                ");

  lcd.setCursor(0, linha);
  lcd.print(texto);
}

// =====================================
// CONTROLAR A CANCELA
// =====================================

void definirCancela(bool abrir) {

  // Nao abre se estiver lotado
  if (abrir && vagasLivres == 0) {
    abrir = false;
  }

  // Evita comandos repetidos
  if (abrir == cancelaAberta) {
    return;
  }

  cancelaAberta = abrir;

  if (cancelaAberta) {

    cancela.write(anguloAberto);
    Serial.println("CANCELA ABERTA - 90 GRAUS");

  } else {

    cancela.write(anguloFechado);
    Serial.println("CANCELA FECHADA - 0 GRAUS");
  }

  atualizarTela = true;
}

// =====================================
// ATUALIZAR QUANTIDADE DE VAGAS
// =====================================

void atualizarVagas() {

  vagasLivres = 0;

  for (byte i = 0; i < TOTAL_VAGAS; i++) {

    if (!vagasOcupadas[i]) {
      vagasLivres++;
    }
  }

  // Se lotar, fecha a cancela
  if (vagasLivres == 0) {
    definirCancela(false);
  }

  atualizarTela = true;
}

// =====================================
// ALTERNAR ESTADO DA VAGA
// =====================================

void alternarVaga(byte i) {

  // Primeiro clique: ocupa
  // Segundo clique: libera
  vagasOcupadas[i] = !vagasOcupadas[i];

  // LED aceso = livre
  // LED apagado = ocupada
  digitalWrite(
    pinosLED[i],
    vagasOcupadas[i] ? LOW : HIGH
  );

  atualizarVagas();

  vagaAlterada = i + 1;
  vagaFoiLiberada = !vagasOcupadas[i];

  mensagemTemporaria = true;
  inicioMensagem = millis();

  atualizarTela = true;

  Serial.print("VAGA ");
  Serial.print(i + 1);

  if (vagasOcupadas[i]) {
    Serial.println(" OCUPADA");
  } else {
    Serial.println(" LIBERADA");
  }
}

// =====================================
// ALTERNAR MODO DE OPERACAO
// =====================================

void alternarModo() {

  modoAutomatico = !modoAutomatico;

  // Fecha a cancela ao trocar de modo
  definirCancela(false);

  carroDetectado = false;

  mensagemTemporaria = false;
  atualizarTela = true;

  if (modoAutomatico) {

    Serial.println("MODO AUTOMATICO ATIVADO");

  } else {

    Serial.println("MODO MANUAL ATIVADO");
  }
}

// =====================================
// EXECUTAR ACAO DOS BOTOES
// =====================================

void executarBotao(byte indice) {

  // BOTAO D10 - VAGA 1
  if (indice == 0) {

    alternarVaga(0);
  }

  // BOTAO D11 - VAGA 2
  else if (indice == 1) {

    alternarVaga(1);
  }

  // BOTAO D12 - CANCELA MANUAL
  else if (indice == 2) {

    if (!modoAutomatico) {

      if (!cancelaAberta && vagasLivres == 0) {

        Serial.println("ENTRADA BLOQUEADA!");
        Serial.println("ESTACIONAMENTO LOTADO!");

      } else {

        definirCancela(!cancelaAberta);
      }
    }
  }

  // BOTAO D13 - ALTERAR MODO
  else if (indice == 3) {

    alternarModo();
  }
}

// =====================================
// VERIFICAR BOTOES COM DEBOUNCE
// =====================================

void verificarBotoes() {

  for (byte i = 0; i < TOTAL_BOTOES; i++) {

    bool leitura = digitalRead(botoes[i]);

    if (leitura != ultimaLeitura[i]) {
      ultimoDebounce[i] = millis();
    }

    if (millis() - ultimoDebounce[i]
        >= tempoDebounce) {

      if (leitura != estadoEstavel[i]) {

        estadoEstavel[i] = leitura;

        // Executa apenas ao pressionar
        if (estadoEstavel[i] == LOW) {
          executarBotao(i);
        }
      }
    }

    ultimaLeitura[i] = leitura;
  }
}

// =====================================
// CONTROLE AUTOMATICO PELO LDR
// =====================================

void controlarAutomatico() {

  if (!modoAutomatico) {
    return;
  }

  valorLDR = analogRead(pinoLDR);

  // Detecta carro com pouca luz
  if (valorLDR < limiteLuz - margemLDR) {

    carroDetectado = true;
  }

  // Detecta ausencia de carro
  else if (valorLDR > limiteLuz + margemLDR) {

    carroDetectado = false;
  }

  // Entre os limites, mantem estado

  bool deveAbrir =
    carroDetectado && vagasLivres > 0;

  definirCancela(deveAbrir);
}

// =====================================
// ATUALIZAR MENSAGENS DO LCD
// =====================================

void atualizarLCD() {

  // Mantem a mensagem inicial por 1,5 s usando millis().
  // Enquanto isso, os botoes e demais rotinas seguem ativos.
  if (inicializando) {
    if (millis() - inicioInicializacao < tempoInicializacao) {
      return;
    }
    inicializando = false;
    atualizarTela = true;
  }

  // Encerra mensagem temporaria
  if (mensagemTemporaria &&
      millis() - inicioMensagem >= tempoMensagem) {

    mensagemTemporaria = false;
    atualizarTela = true;
  }

  if (!atualizarTela) {
    return;
  }

  atualizarTela = false;

  char linha1[17];
  char linha2[17];

  // ESTACIONAMENTO LOTADO
  if (vagasLivres == 0) {

    escreverLinha(0, "ESTACIONAMENTO");
    escreverLinha(1, "LOTADO");

    return;
  }

  // CANCELA ABERTA
  if (cancelaAberta) {

    escreverLinha(0, "ENTRADA LIBERADA");

    snprintf(
      linha2,
      sizeof(linha2),
      "VAGAS LIVRES: %u",
      vagasLivres
    );

    escreverLinha(1, linha2);

    return;
  }

  // VAGA OCUPADA OU LIBERADA
  if (mensagemTemporaria) {

    snprintf(
      linha1,
      sizeof(linha1),
      "VAGA %u %s",
      vagaAlterada,
      vagaFoiLiberada ? "LIBERADA" : "OCUPADA"
    );

    snprintf(
      linha2,
      sizeof(linha2),
      "VAGAS LIVRES: %u",
      vagasLivres
    );

    escreverLinha(0, linha1);
    escreverLinha(1, linha2);

    return;
  }

  // SITUACAO NORMAL
  snprintf(
    linha1,
    sizeof(linha1),
    "VAGAS LIVRES: %u",
    vagasLivres
  );

  escreverLinha(0, linha1);

  if (modoAutomatico) {

    escreverLinha(1, "MODO AUTOMATICO");

  } else {

    escreverLinha(1, "MODO MANUAL");
  }
}

// =====================================
// MONITOR SERIAL
// =====================================

void mostrarStatus() {

  Serial.println();
  Serial.println("==========================");
  Serial.println("STATUS DO ESTACIONAMENTO");
  Serial.println("==========================");

  Serial.print("Modo: ");

  if (modoAutomatico) {
    Serial.println("AUTOMATICO");
  } else {
    Serial.println("MANUAL");
  }

  Serial.print("Vagas livres: ");
  Serial.print(vagasLivres);
  Serial.print("/");
  Serial.println(TOTAL_VAGAS);

  for (byte i = 0; i < TOTAL_VAGAS; i++) {

    Serial.print("Vaga ");
    Serial.print(i + 1);
    Serial.print(": ");

    if (vagasOcupadas[i]) {
      Serial.println("OCUPADA");
    } else {
      Serial.println("LIVRE");
    }
  }

  Serial.print("Cancela: ");

  if (cancelaAberta) {
    Serial.println("ABERTA");
  } else {
    Serial.println("FECHADA");
  }

  if (modoAutomatico) {

    Serial.print("Leitura LDR: ");
    Serial.println(valorLDR);

    Serial.print("Carro detectado: ");
    Serial.println(carroDetectado ? "SIM" : "NAO");
  }

  if (vagasLivres == 0) {
    Serial.println("ESTACIONAMENTO LOTADO!");
  }

  Serial.println("==========================");
}

// =====================================
// INICIALIZACAO DO ARDUINO
// =====================================

void setup() {

  Serial.begin(9600);

  // CONFIGURA OS LEDS
  for (byte i = 0; i < TOTAL_VAGAS; i++) {

    pinMode(pinosLED[i], OUTPUT);

    // Todas as vagas iniciam livres
    digitalWrite(pinosLED[i], HIGH);
  }

  // CONFIGURA OS BOTOES
  for (byte i = 0; i < TOTAL_BOTOES; i++) {

    pinMode(botoes[i], INPUT_PULLUP);

    ultimaLeitura[i] = digitalRead(botoes[i]);
    estadoEstavel[i] = ultimaLeitura[i];

    ultimoDebounce[i] = millis();
  }

  // INICIALIZA SERVOMOTOR
  cancela.attach(pinoServo);
  cancela.write(anguloFechado);

  // INICIALIZA LCD I2C
  Wire.begin();

  lcd.init();
  lcd.backlight();
  lcd.clear();

  escreverLinha(0, "ESTACIONAMENTO");
  escreverLinha(1, "INICIANDO...");

  Serial.println("SISTEMA INICIADO");
  Serial.println("CAPACIDADE: 2 VAGAS");
  Serial.println("MODO INICIAL: MANUAL");

  inicioInicializacao = millis();

  atualizarTela = true;
}

// =====================================
// LOOP PRINCIPAL
// =====================================

void loop() {

  verificarBotoes();

  controlarAutomatico();

  atualizarLCD();

  if (millis() - ultimoSerial >= intervaloSerial) {

    ultimoSerial = millis();

    mostrarStatus();
  }
}
