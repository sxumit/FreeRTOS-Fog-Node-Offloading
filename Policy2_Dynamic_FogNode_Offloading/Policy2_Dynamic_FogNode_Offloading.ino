
// POLICY 2 DYNAMIC FOG OFFLOADING VIA MQTT BROKER 
// OFFLOADED DATA PROCESSED IN THE PYTHON SCRIPT AND SENT OVER THROUGH WIFI

#include <math.h>
#include "esp_timer.h"
#include <WiFi.h>
#include <PubSubClient.h>

// WIFI & MQTT SETTINGS
const char* ssid = "your wifi name ";
const char* password = "wifi passwords ";
const char* mqtt_server = " PC's IP v4 address"; // ipconfig command

WiFiClient espClient;
PubSubClient client(espClient);

// POLICY 2 PARAMETERS
#define PERIOD_MS 100
#define DEADLINE_MS 50
#define NUM_SAMPLES 1000

#define NORMAL_ITERATIONS 3500
#define CONTENTION_ITERATIONS 25000

#define CONTENTION_START 500  // 501st sample (0-indexed)
#define CONTENTION_END 699    // 700th sample (0-indexed)

#define OFFLOAD_THRESHOLD 10000

// DATA LOGGING
struct ResultRecord {
  unsigned long mainTask_us;
  unsigned long contentionRTT_us;
  bool offloaded;
};

ResultRecord experimentData[NUM_SAMPLES];

// FreeRTOS Handles
TaskHandle_t ContentionTaskHandle = NULL;
TaskHandle_t NetworkTaskHandle = NULL;
QueueHandle_t offloadQueue;
QueueHandle_t sampleQueue;  //Precision queue to prevent skipped samples
SemaphoreHandle_t mqttSemaphore;

// MQTT CALLBACK
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  xSemaphoreGive(mqttSemaphore);
}

// CORE 0: NETWORK & MQTT MANAGER
void networkTask(void* parameter) {
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCallback);
  int offloadRequestIters;

  while (true) {
    if (!client.connected() && WiFi.status() == WL_CONNECTED) {
      if (client.connect("ESP32_Edge_Node")) {
        client.subscribe("cps/compute/result");
        Serial.println("[+] MQTT Broker Connected. Ready for experiment.");
      } else {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
      }
    }

    client.loop();

    if (xQueueReceive(offloadQueue, &offloadRequestIters, 0) == pdTRUE) {
      String payload = "{\"value\": " + String(offloadRequestIters) + "}";
      client.publish("cps/compute/request", payload.c_str());
    }

    vTaskDelay(2 / portTICK_PERIOD_MS);
  }
}

// CORE 1 HIGH PRIORITY CPU HOG
void contentionTask(void* parameter) {
  int sampleContext;  // Local variable to hold the exact sample ID

  while (true) {
    // Wait for the exact sample ID from the Queue
    xQueueReceive(sampleQueue, &sampleContext, portMAX_DELAY);

    unsigned long long startRTT = esp_timer_get_time();

    if (CONTENTION_ITERATIONS > OFFLOAD_THRESHOLD) {
      experimentData[sampleContext].offloaded = true;

      int iters = CONTENTION_ITERATIONS;
      xQueueSend(offloadQueue, &iters, portMAX_DELAY);

      xSemaphoreTake(mqttSemaphore, portMAX_DELAY);

      unsigned long long finishRTT = esp_timer_get_time();
      experimentData[sampleContext].contentionRTT_us = (finishRTT - startRTT);
    } else {
      experimentData[sampleContext].offloaded = false;
      volatile float dummy = 0.5;
      for (int i = 0; i < CONTENTION_ITERATIONS; i++) {
        dummy = sin(dummy) * cos(dummy) + 1.5;
      }
      unsigned long long finishRTT = esp_timer_get_time();
      experimentData[sampleContext].contentionRTT_us = (finishRTT - startRTT);
    }
  }
}

// CORE 1 THE MAIN TASK
void baselineTask(void* parameter) {
  TickType_t lastWakeTime = xTaskGetTickCount();
  unsigned long deadlineMisses = 0;

  while (!client.connected()) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  Serial.println("\n========================================");
  Serial.println("STARTING POLICY 2 (DYNAMIC OFFLOADING)");
  Serial.println("Threshold: 10000 | Main: 3500 | Contention: 25000");
  Serial.println("Running silently...\n");

  vTaskDelay(pdMS_TO_TICKS(1000));

  for (int sample = 0; sample < NUM_SAMPLES; sample++) {
    unsigned long long startTime = esp_timer_get_time();

    if (sample >= CONTENTION_START && sample <= CONTENTION_END) {

      // Send the exact sample ID into the queue
      xQueueSend(sampleQueue, &sample, 0);
    } else {
      experimentData[sample].contentionRTT_us = 0;
      experimentData[sample].offloaded = false;
    }

    volatile float x = 0.5;
    if (NORMAL_ITERATIONS <= OFFLOAD_THRESHOLD) {
      for (int i = 0; i < NORMAL_ITERATIONS; i++) {
        x = sin(x) * cos(x) + sqrt(x + 1.0);
      }
    }

    unsigned long long finishTime = esp_timer_get_time();
    unsigned long executionTime = finishTime - startTime;

    experimentData[sample].mainTask_us = executionTime;

    if (executionTime > (DEADLINE_MS * 1000ULL)) {
      deadlineMisses++;
    }

    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(PERIOD_MS));
  }

  // Give the Contention Task a moment to process any backlogged network messages before dumping
  vTaskDelay(pdMS_TO_TICKS(2000));

  Serial.println("========================================");
  Serial.println("RAW POLICY 2 DATA (CSV FORMAT)");
  Serial.println("Sample,Mode,Main_Time_us,Contention_RTT_us,Offloaded");

  for (int i = 0; i < NUM_SAMPLES; i++) {
    Serial.print(i + 1);
    Serial.print(",");

    if (i >= CONTENTION_START && i <= CONTENTION_END) {
      Serial.print("CONTENTION,");
    } else {
      Serial.print("NORMAL,");
    }

    Serial.print(experimentData[i].mainTask_us);
    Serial.print(",");
    Serial.print(experimentData[i].contentionRTT_us);
    Serial.print(",");
    Serial.println(experimentData[i].offloaded ? "YES" : "NO");
  }

  Serial.println("\n========================================");
  Serial.println("POLICY 2 SUMMARY");
  Serial.print("Total Deadline Misses: ");
  Serial.println(deadlineMisses);
  Serial.println("Experiment complete. Copy CSV data to Excel.");

  vTaskDelete(NULL);
}

void setup() {
  Serial.begin(115200);

  Serial.println("\n[!] Wiping old WiFi memory...");
  WiFi.disconnect(true, true);
  delay(500);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[+] WiFi Connected Successfully!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[!] FATAL: WiFi failed to connect. Restart board.");
    while (1)
      ;
  }

  // INITIALIZE SYNCHRONIZATION TOOLS
  offloadQueue = xQueueCreate(10, sizeof(int));
  // Create a queue big enough to hold 250 sample IDs if the network lags
  sampleQueue = xQueueCreate(250, sizeof(int));
  mqttSemaphore = xSemaphoreCreateBinary();

  // TASKS
  xTaskCreatePinnedToCore(networkTask, "NetworkTask", 8192, NULL, 1, &NetworkTaskHandle, 0);
  xTaskCreatePinnedToCore(contentionTask, "ContentionTask", 4096, NULL, 2, &ContentionTaskHandle, 1);
  xTaskCreatePinnedToCore(baselineTask, "BaselineTask", 4096, NULL, 1, NULL, 1);
}

void loop() {
  vTaskDelete(NULL);
}
