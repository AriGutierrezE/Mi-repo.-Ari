#include <Arduino.h>
extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/queue.h"
  #include "freertos/semphr.h"
}

// ------------------------ A4: Ultrasónico ------------------------
#define TRIG_PIN 5
#define ECHO_PIN 18

QueueHandle_t qDist = nullptr;

// ------------------------ A5: Motor L293D (M1) -------------------
#define L293_ENA 23   // PWM velocidad (conectar a D5 del shield)
#define L293_IN1 19   // Dirección (conectar a D4 del shield)
#define L293_IN2 21   // Dirección (conectar a D7 del shield)

// Estado compartido del motor
SemaphoreHandle_t mtxMotor;
volatile int  gSpeedPct = 0;   // 0..100 %
volatile char gDir      = 'F'; // 'F' o 'R'

// ---------- helpers PWM usando analogWrite ----------
static inline uint8_t pctToDuty(int pct) {
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return (uint8_t) map(pct, 0, 100, 0, 255);
}

// ------------------------ A4: medición ---------------------------
float measureDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long dur = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (dur == 0) return NAN;
  return (float)dur / 58.0f;
}

void T_Producer(void *pv) {
  if (!qDist) vTaskDelete(NULL);
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    float d = measureDistanceCm();
    xQueueSend(qDist, &d, 0);
    vTaskDelayUntil(&last, pdMS_TO_TICKS(100)); // 100 ms SIN drift
  }
}

void T_Consumer(void *pv) {
  if (!qDist) vTaskDelete(NULL);
  float d;
  for (;;) {
    if (xQueueReceive(qDist, &d, portMAX_DELAY) == pdTRUE) {
      if (!isnan(d)) Serial.printf("{\"distance_cm\":%.2f}\n", d);
      else           Serial.println("{\"distance_cm\":null}");
    }
  }
}

// ------------------------ A5: motor ------------------------------
void motorApplyFromState() {
  int sp; char dir;
  xSemaphoreTake(mtxMotor, portMAX_DELAY);
  sp  = gSpeedPct;
  dir = gDir;
  xSemaphoreGive(mtxMotor);

  // Dirección
  if (dir == 'R') { digitalWrite(L293_IN1, LOW);  digitalWrite(L293_IN2, HIGH); }
  else            { digitalWrite(L293_IN1, HIGH); digitalWrite(L293_IN2, LOW);  }

  // Velocidad (PWM a 20 kHz, 8 bits)
  analogWrite(L293_ENA, pctToDuty(sp)); // 0..255
}

// Parser muy simple de {"speed":75,"dir":"R"}
bool parseCommand(const String& line, int &outSpeed, char &outDir) {
  outSpeed = -1; outDir = 0;

  int iS = line.indexOf("\"speed\"");
  if (iS >= 0) {
    int colon = line.indexOf(':', iS);
    if (colon > 0) {
      int j = colon + 1; while (j < (int)line.length() && isspace((unsigned char)line[j])) j++;
      int k = j; while (k < (int)line.length() && isdigit((unsigned char)line[k])) k++;
      if (k > j) outSpeed = line.substring(j, k).toInt();
    }
  }

  int iD = line.indexOf("\"dir\"");
  if (iD >= 0) {
    int colon = line.indexOf(':', iD);
    int q1 = line.indexOf('"', colon+1);
    int q2 = (q1 >= 0) ? line.indexOf('"', q1+1) : -1;
    if (q1 >= 0 && q2 > q1) {
      char c = toupper((unsigned char)line.substring(q1+1, q2)[0]);
      if (c == 'F' || c == 'R' || c == 'L') outDir = (c=='L') ? 'R' : c;
    }
  }

  return (outSpeed >= 0 || outDir != 0);
}

void T_SerialMotor(void *pv) {
  Serial.setTimeout(50);
  static char ack[64];
  for (;;) {
    if (Serial.available()) {
      String line = Serial.readStringUntil('\n');
      line.trim();
      if (!line.length()) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }

      int sp; char dir;
      if (parseCommand(line, sp, dir)) {
        bool changed = false;
        xSemaphoreTake(mtxMotor, portMAX_DELAY);
        if (sp >= 0 && sp <= 100 && sp != gSpeedPct) { gSpeedPct = sp; changed = true; }
        if (dir == 'F' || dir == 'R') { if (dir != gDir) { gDir = dir; changed = true; } }
        int curSp = gSpeedPct; char curDir = gDir;
        xSemaphoreGive(mtxMotor);

        if (changed) motorApplyFromState();
        snprintf(ack, sizeof(ack), "{\"ack\":true,\"speed\":%d,\"dir\":\"%c\"}\n", curSp, curDir);
        Serial.print(ack);
      } else {
        Serial.println("{\"ack\":false,\"error\":\"bad_format\"}");
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

// ------------------------ setup / loop ---------------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  // A4
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT); // ECHO via divisor 5V->3.3V

  // A5
  pinMode(L293_IN1, OUTPUT);
  pinMode(L293_IN2, OUTPUT);

  // Configura PWM genérico
  analogWriteResolution(L293_ENA, 8);        // resolución 8 bits
  analogWriteFrequency(L293_ENA, 20000);     // 20 kHz en ENA

  mtxMotor = xSemaphoreCreateMutex();
  if (!mtxMotor) { Serial.println("[setup] ERROR mutex"); for(;;) delay(1000); }

  qDist = xQueueCreate(10, sizeof(float));
  if (!qDist) { Serial.println("[setup] ERROR cola"); for(;;) delay(1000); }

  xSemaphoreTake(mtxMotor, portMAX_DELAY);
  gSpeedPct = 0; gDir = 'F';
  xSemaphoreGive(mtxMotor);
  motorApplyFromState();

  const uint32_t STACK = 4096;
  xTaskCreatePinnedToCore(T_Producer,    "producer", STACK, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(T_Consumer,    "consumer", STACK, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(T_SerialMotor, "serialM",  STACK, nullptr, 2, nullptr, 1);

  Serial.println("[setup] OK: A4+A5 activos");
}

void loop() {}
