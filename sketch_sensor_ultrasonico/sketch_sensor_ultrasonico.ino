#include <Arduino.h>
extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/queue.h"
}

#define TRIG_PIN 5
#define ECHO_PIN 18

// ---- RTOS objects ----
QueueHandle_t qDist = nullptr;

// ---- Medición ultrasónico ----
float measureDistanceCm() {
  // Pulso de disparo
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Lee ancho del pulso ECHO (timeout 30 ms ≈ 5 m)
  unsigned long dur = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (dur == 0) {
    // diagnóstico: no llegó pulso
    return NAN;
  }
  // cm ≈ dur(us) / 58
  return (float)dur / 58.0f;
}

// ---- Tarea productora: 100 ms SIN drift ----
void T_Producer(void *pv) {
  // Seguridad: no continuar si la cola no existe
  if (qDist == NULL) {
    Serial.println("[Producer] ERROR: qDist == NULL");
    vTaskDelete(NULL);
  }

  TickType_t last = xTaskGetTickCount();
  for (;;) {
    float d = measureDistanceCm();

    // Intento no bloqueante de enviar (0 ticks). Si la cola está llena, avisamos.
    BaseType_t ok = xQueueSend(qDist, &d, 0);
    if (ok != pdTRUE) {
      Serial.println("[Producer] WARN: xQueueSend falló (cola llena o invalida)");
    }

    // (opcional) log muy ocasional para ver headroom de stack
    static uint32_t cnt = 0;
    if ((++cnt % 50) == 0) { // cada 5 s aprox (50*100ms)
      UBaseType_t hw = uxTaskGetStackHighWaterMark(NULL);
      Serial.printf("[Producer] Stack HW mark: %u bytes\n", (unsigned)hw);
    }

    vTaskDelayUntil(&last, pdMS_TO_TICKS(100));
  }
}

// ---- Tarea consumidora: imprime al instante ----
void T_Consumer(void *pv) {
  if (qDist == NULL) {
    Serial.println("[Consumer] ERROR: qDist == NULL");
    vTaskDelete(NULL);
  }

  float d;
  for (;;) {
    if (xQueueReceive(qDist, &d, portMAX_DELAY) == pdTRUE) {
      if (!isnan(d)) {
        Serial.printf("{\"distance_cm\":%.2f}\n", d);
      }
      // (opcional) monitoreo de stack
      static uint32_t cnt = 0;
      if ((++cnt % 100) == 0) { // ~cada 10 s
        UBaseType_t hw = uxTaskGetStackHighWaterMark(NULL);
        Serial.printf("[Consumer] Stack HW mark: %u bytes\n", (unsigned)hw);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT); // OJO: ECHO debe llegar via divisor 5V->3.3V

  // Crea la cola y valida
  qDist = xQueueCreate(10, sizeof(float));
  if (qDist == NULL) {
    Serial.println("[setup] ERROR: no se pudo crear la cola qDist");
    // No seguimos: evitar llamadas sobre puntero nulo
    for(;;) { delay(1000); }
  }

  // Crea tareas con stack amplio (Serial.printf consume stack)
  const uint32_t STACK = 4096;
  xTaskCreatePinnedToCore(T_Producer, "producer", STACK, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(T_Consumer, "consumer", STACK, nullptr, 2, nullptr, 1);

  Serial.println("[setup] OK: tareas y cola creadas");
}

void loop() {
  // vacío: todo corre en FreeRTOS
}
