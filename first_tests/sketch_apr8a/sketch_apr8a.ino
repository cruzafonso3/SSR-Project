#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESPping.h> // Lembra-te que a tua biblioteca é esta!

// --- COLOCA AQUI OS TEUS DADOS ---
const char* ssid = "afonsocruz";
const char* password = "afonsocruz";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define BTN_NEXT 18
#define BTN_SCAN 4
#define MAX_IPS  25

String  ipsEncontrados[MAX_IPS];
volatile int  totalEncontrados = 0;
volatile int  selecaoAtual     = 0;
volatile bool iniciarScan      = false;
volatile bool scanAdecorrer    = false;
volatile int  ipSendoTestado   = 0;
volatile bool routerOk         = false;

bool estadoUltimoNext = HIGH;
bool estadoUltimoScan = HIGH;

TaskHandle_t TarefaScan;

bool pingRapido(IPAddress ip) {
  if (Ping.ping(ip, 2)) return true;
  vTaskDelay(100 / portTICK_PERIOD_MS);
  if (Ping.ping(ip, 2)) return true;
  return false;
}

void setup() {
  Serial.begin(115200);
  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_SCAN, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Erro OLED");
    for (;;);
  }

  conectarWiFi();

  xTaskCreatePinnedToCore(codigoScanNoCore0, "TarefaScan", 10000, NULL, 1, &TarefaScan, 0);

  iniciarScan = true;
}

// =========================================================================
// CORE 1 — Botões e ecrã
// =========================================================================
void loop() {
  bool botaoNext = digitalRead(BTN_NEXT);
  bool botaoScan = digitalRead(BTN_SCAN);

  if (botaoNext == LOW && estadoUltimoNext == HIGH) {
    delay(50);
    if (digitalRead(BTN_NEXT) == LOW && totalEncontrados > 0) {
      selecaoAtual++;
      if (selecaoAtual >= totalEncontrados) selecaoAtual = 0;
    }
  }
  estadoUltimoNext = botaoNext;

  if (botaoScan == LOW && estadoUltimoScan == HIGH) {
    delay(50);
    if (digitalRead(BTN_SCAN) == LOW && !scanAdecorrer) {
      iniciarScan = true;
    }
  }
  estadoUltimoScan = botaoScan;

  atualizarInterface();
  delay(50);
}

// =========================================================================
// CORE 0 — Scan de rede
// =========================================================================
void codigoScanNoCore0(void* pvParameters) {
  for (;;) {
    if (iniciarScan) {
      scanAdecorrer    = true;
      iniciarScan      = false;
      totalEncontrados = 0;
      selecaoAtual     = 0;
      routerOk         = false;

      vTaskDelay(300 / portTICK_PERIOD_MS);

      IPAddress gateway = WiFi.gatewayIP();
      IPAddress localIP = WiFi.localIP();

      Serial.print("Gateway: "); Serial.println(gateway.toString());
      Serial.print("LocalIP: "); Serial.println(localIP.toString());

      // --- FASE 1: Router ---
      ipSendoTestado = gateway[3];
      bool routerPing = pingRapido(gateway);
      Serial.println(routerPing ? "Router: OK" : "Router: sem resposta ICMP");
      ipsEncontrados[totalEncontrados++] = gateway.toString() +
                                           (routerPing ? " (Router)" : " (Router?)");
      routerOk = true;

      vTaskDelay(400 / portTICK_PERIOD_MS);

      // --- FASE 2: Resto da rede ---
      for (int i = 241; i <= 254 && totalEncontrados < MAX_IPS; i++) {
        IPAddress ip = localIP;
        ip[3] = i;

        if (ip == localIP || ip == gateway) continue;

        ipSendoTestado = i;

        Serial.print("A testar: "); Serial.println(ip.toString());

        if (pingRapido(ip)) {
          ipsEncontrados[totalEncontrados++] = ip.toString();
          Serial.println("  --> ENCONTRADO!");
        }

        vTaskDelay(5 / portTICK_PERIOD_MS);
      }

      Serial.print("Scan concluido. Total: ");
      Serial.println(totalEncontrados);
      scanAdecorrer = false;
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// =========================================================================
// FUNÇÕES GRÁFICAS
// =========================================================================
void conectarWiFi() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println("A ligar ao WiFi...");
  display.display();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
}

void atualizarInterface() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("REDE: "); display.println(ssid);
  display.println("---------------------");

  if (scanAdecorrer) {
    if (!routerOk) {
      display.println("A testar router...");
    } else {
      display.print("Scan: .");
      display.println(ipSendoTestado);
    }
  } else {
    display.println("Scan concluido.");
  }

  if (totalEncontrados == 0) {
    display.setCursor(0, 35);
    if (!scanAdecorrer) display.println("Nenhum encontrado.");
  } else {
    display.setCursor(0, 25);
    display.print("Disp: "); display.print(selecaoAtual + 1);
    display.print("/"); display.println(totalEncontrados);
    display.setCursor(0, 40);
    display.println(ipsEncontrados[selecaoAtual]);
  }

  display.display();
}