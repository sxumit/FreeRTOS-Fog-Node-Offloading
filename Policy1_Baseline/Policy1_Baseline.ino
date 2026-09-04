 
// POLICY 1 BASELINE EXPERIMENT - NO OFFLOADING

#include <math.h>
#include "esp_timer.h"

#define PERIOD_MS 100
#define DEADLINE_MS 50
#define NUM_SAMPLES 1000
#define NORMAL_ITERATIONS 3500

// CONTENTION PARAMETERS
#define CONTENTION_START 500 // 501st sample (0-indexed)
#define CONTENTION_END 699   // 700th sample (0-indexed)
#define CONTENTION_ITERATIONS 25000 // Heavy dummy load to steal CPU

volatile float calculationResult = 0.0;

unsigned long executionTimes[NUM_SAMPLES];

unsigned long long totalExecutionTime = 0;
unsigned long long totalBusyTime = 0;

unsigned long minExecutionTime = ULONG_MAX;
unsigned long maxExecutionTime = 0;

unsigned long deadlineMisses = 0;

double sumExecutionTime = 0;
double sumExecutionTimeSquared = 0;

// Handle for the new contention task
TaskHandle_t ContentionTaskHandle = NULL;

// HIGH PRIORITY CPU HOG
void contentionTask(void *parameter) {
  while (true) {
    // Sleep peacefully until main task sends a signal
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // WAKE UP AND STEAL CORE 0
    volatile float dummy = 0.5;
    for (int i = 0; i < CONTENTION_ITERATIONS; i++) {
      dummy = sin(dummy) * cos(dummy) + 1.5;
    }
  }
}

void baselineTask(void *parameter) {

  TickType_t lastWakeTime = xTaskGetTickCount();

  // Warm-up
  vTaskDelay(pdMS_TO_TICKS(1000));

  for (int sample = 0; sample < NUM_SAMPLES; sample++) {


    // START TIME
    unsigned long long startTime = esp_timer_get_time();

    // TRIGGER THE CPU SPIKE
    if (sample >= CONTENTION_START && sample <= CONTENTION_END) {
      xTaskNotifyGive(ContentionTaskHandle); 
    }

    // NORMAL CPU WORKLOAD
    float x = 0.5;

    for (int i = 0; i < NORMAL_ITERATIONS; i++) {
      x = sin(x) * cos(x) + sqrt(x + 1.0);
    }

    calculationResult = x;

    // FINISH TIME
    unsigned long long finishTime = esp_timer_get_time();

    // Execution time in microseconds
    unsigned long executionTime = finishTime - startTime;

    // STORE RAW MEASUREMENT
    executionTimes[sample] = executionTime;

    // STATISTICS
    totalExecutionTime += executionTime;
    totalBusyTime += executionTime;

    if (executionTime < minExecutionTime) {
      minExecutionTime = executionTime;
    }

    if (executionTime > maxExecutionTime) {
      maxExecutionTime = executionTime;
    }

    sumExecutionTime += executionTime;

    sumExecutionTimeSquared +=
      (double)executionTime * executionTime;

    // DEADLINE CHECK
    if (executionTime > (DEADLINE_MS * 1000ULL)) {
      deadlineMisses++;
    }

    // MAINTAIN PERIOD
    vTaskDelayUntil(
      &lastWakeTime,
      pdMS_TO_TICKS(PERIOD_MS)
    );
  }

  // FINAL STATISTICS
  double averageExecutionTime =
    sumExecutionTime / NUM_SAMPLES;

  double variance =
    (sumExecutionTimeSquared / NUM_SAMPLES) -
    (averageExecutionTime * averageExecutionTime);

  double standardDeviation = sqrt(variance);

  double experimentTime =
    NUM_SAMPLES * PERIOD_MS * 1000.0;

  double cpuUtilization =
    (totalBusyTime / experimentTime) * 100.0;

  // MEMORY
  size_t freeHeap = ESP.getFreeHeap();
  size_t minimumFreeHeap = ESP.getMinFreeHeap();
  UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);

  // RAW DATA (Updated to include Mode)
  Serial.println();
  Serial.println("========================================");
  Serial.println("RAW EXECUTION-TIME DATA");

  Serial.println("Run,Mode,Execution_Time_us");

  for (int i = 0; i < NUM_SAMPLES; i++) {
    Serial.print(i + 1);
    Serial.print(",");
    
    // Determine the mode based on the sample index
    if (i >= CONTENTION_START && i <= CONTENTION_END) {
        Serial.print("CONTENTION,");
    } else {
        Serial.print("NORMAL,");
    }
    
    Serial.println(executionTimes[i]);
  }

  // FINAL SUMMARY
  Serial.println();
  Serial.println("========================================");
  Serial.println("PROJECT 2 - POLICY 1 (STATIC LOCAL)");
  Serial.print("Samples: ");
  Serial.println(NUM_SAMPLES);
  Serial.print("Period: ");
  Serial.print(PERIOD_MS);
  Serial.println(" ms");
  Serial.print("Deadline: ");
  Serial.print(DEADLINE_MS);
  Serial.println(" ms");
  Serial.println();

  Serial.print("Minimum execution time: ");
  Serial.print(minExecutionTime / 1000.0, 3);
  Serial.println(" ms");
  Serial.print("Maximum execution time: ");
  Serial.print(maxExecutionTime / 1000.0, 3);
  Serial.println(" ms");
  Serial.print("Average execution time: ");
  Serial.print(averageExecutionTime / 1000.0, 3);
  Serial.println(" ms");
  Serial.print("Timing standard deviation: ");
  Serial.print(standardDeviation / 1000.0, 3);
  Serial.println(" ms");
  Serial.print("Deadline misses: ");
  Serial.println(deadlineMisses);
  Serial.print("Deadline miss rate: ");
  Serial.print((deadlineMisses * 100.0) / NUM_SAMPLES, 2);
  Serial.println(" %");
  Serial.println();
  
  Serial.println("Baseline Experiment 1 complete.");
  // Stop task
  vTaskDelete(NULL);
}

void setup() {
  Serial.begin(115200);

  // Create the Contention Task (Priority 2 - Higher)
  xTaskCreatePinnedToCore(
    contentionTask,
    "ContentionTask",
    4096,
    NULL,
    2,
    &ContentionTaskHandle,
    0
  );

  // Create the Main Task (Priority 1 - Lower)
  xTaskCreatePinnedToCore(
    baselineTask,
    "BaselineTask",
    4096,
    NULL,
    1,
    NULL,
    0
  );
}

void loop() {
}
