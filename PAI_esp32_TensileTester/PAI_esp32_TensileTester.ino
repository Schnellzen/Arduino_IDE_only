#include <HX711.h>

// Stepper Pins
#define stepPinLeft     13
#define dirPinLeft      12
#define enablePinLeft   14
#define stepPinRight    27
#define dirPinRight     26
#define enablePinRight  25

// HX711 Pins
#define HX711_DT 4
#define HX711_SCK 2

// Common settings
const int microstep = 32;
const int stepsPerRev = 200 * microstep;
const float mmPerRev = 8.0;
float defaultSpeed = 0.1;
const int WEIGHT_SAMPLE_INTERVAL = 16;

HX711 scale;
unsigned long lastWeightPrint = 0;
unsigned long lastStepTime = 0;
bool isMoving = false;
bool weightSamplingEnabled = true;

// Movement tracking variables
int totalStepsTaken = 0;
float totalDistanceMoved = 0.0;
float currentMoveDistance = 0.0;
float currentMoveSpeed = 0.0;
String currentMoveType = "";

// Output mode variables
bool checkMode = false;  // false = data mode (default), true = check mode

void setup() {
  // Stepper Setup
  pinMode(stepPinLeft, OUTPUT);
  pinMode(dirPinLeft, OUTPUT);
  pinMode(enablePinLeft, OUTPUT);
  pinMode(stepPinRight, OUTPUT);
  pinMode(dirPinRight, OUTPUT);
  pinMode(enablePinRight, OUTPUT);
  digitalWrite(enablePinLeft, LOW);
  digitalWrite(enablePinRight, LOW);
  delay(10);

  // Load Cell Setup
  scale.begin(HX711_DT, HX711_SCK);
  scale.set_scale(2280.f);
  scale.tare();

  Serial.begin(115200);
  Serial.println("System Ready");
  Serial.println("Output Mode: DATA (step<TAB>distance<TAB>weight)");
  Serial.println("Commands:");
  Serial.println("D0 Z<distance> F<speed> - Move both steppers");
  Serial.println("D1/D2 Z<distance> F<speed> - Move left/right stepper");
  Serial.println("D3 - Reset movement counter");
  Serial.println("D4 Z<max_distance> F<speed> W<target_weight> - Move until weight or distance reached");
  Serial.println("D10 - Tare scale");
  Serial.println("D11 - Toggle weight sampling during movement");
  Serial.println("D12 - Print movement statistics");
  Serial.println("D100 - Toggle output mode (DATA/CHECK)");
}

void resetMovementCount() {
  totalStepsTaken = 0;
  totalDistanceMoved = 0.0;
  Serial.println("Movement counter reset to zero");
}

void toggleOutputMode() {
  checkMode = !checkMode;
  Serial.print("Output mode changed to: ");
  Serial.println(checkMode ? "CHECK" : "DATA");
}

void printWeightDataMode(int currentStep, int totalSteps) {
  float weight = scale.get_units(1);
  float currentDistance = (currentStep * currentMoveDistance) / totalSteps;
  
  // DATA MODE: <step><TAB><distance><TAB><weight>
  Serial.print(totalStepsTaken);
  Serial.print("\t");
  Serial.print(totalDistanceMoved + currentDistance, 6);
  Serial.print("\t");
  Serial.println(weight, 6);
}

void printWeightCheckMode(int currentStep, int totalSteps, String motor) {
  float weight = scale.get_units(1);
  
  // CHECK MODE: Detailed human-readable format
  Serial.print("MOVEMENT: ");
  Serial.print(motor);
  Serial.print(" | STEP: ");
  Serial.print(currentStep);
  Serial.print("/");
  Serial.print(totalSteps);
  Serial.print(" | DIST: ");
  Serial.print((currentStep * currentMoveDistance) / totalSteps, 2);
  Serial.print("/");
  Serial.print(currentMoveDistance, 2);
  Serial.print("mm | SPEED: ");
  Serial.print(currentMoveSpeed, 3);
  Serial.print("mm/s | WEIGHT: ");
  Serial.print(weight, 2);
  Serial.println(" units");
}

void printIdleWeight() {
  float weight = scale.get_units(1);
  
  if (checkMode) {
    // CHECK MODE: Human-readable
    Serial.print("IDLE | TOTAL_STEPS: ");
    Serial.print(totalStepsTaken);
    Serial.print(" | TOTAL_DIST: ");
    Serial.print(totalDistanceMoved, 2);
    Serial.print("mm | WEIGHT: ");
    Serial.print(weight, 2);
    Serial.println(" units");
  } else {
    // DATA MODE: Tab-separated
    Serial.print(totalStepsTaken);
    Serial.print("\t");
    Serial.print(totalDistanceMoved, 6);
    Serial.print("\t");
    Serial.println(weight, 6);
  }
}

void printMovementStats() {
  Serial.println("=== MOVEMENT STATISTICS ===");
  Serial.print("Total steps taken: ");
  Serial.println(totalStepsTaken);
  Serial.print("Total distance moved: ");
  Serial.print(totalDistanceMoved, 2);
  Serial.println("mm");
  Serial.print("Weight sampling during movement: ");
  Serial.println(weightSamplingEnabled ? "ON" : "OFF");
  Serial.print("Output mode: ");
  Serial.println(checkMode ? "CHECK" : "DATA");
  Serial.println("===========================");
}

void moveUntilWeight(float maxDistance, float speed, float targetWeight) {
  isMoving = true;
  digitalWrite(dirPinLeft, maxDistance > 0 ? HIGH : LOW);
  digitalWrite(dirPinRight, maxDistance > 0 ? HIGH : LOW);
  delay(1);
  
  float stepsPerMm = stepsPerRev / mmPerRev;
  float stepsPerSecond = speed * stepsPerMm;
  
  int stepInterval = (stepsPerSecond > 0) ? (1000.0 / stepsPerSecond) : 1000;
  
  int maxSteps = abs(maxDistance) * stepsPerMm;
  int weightSampleCounter = 0;
  unsigned long stepStartTime = millis();
  unsigned long lastWeightCheckTime = 0;
  bool targetReached = false;

  // Store movement info
  currentMoveDistance = maxDistance;
  currentMoveSpeed = speed;
  currentMoveType = "WEIGHT_CONTROL";

  if (checkMode) {
    Serial.print("Weight-controlled movement started | ");
    Serial.print("Max distance: ");
    Serial.print(maxDistance, 2);
    Serial.print("mm | Speed: ");
    Serial.print(speed, 3);
    Serial.print("mm/s | Target weight: ");
    Serial.print(targetWeight, 3);
    Serial.print(" | Max steps: ");
    Serial.print(maxSteps);
    Serial.print(" | Step interval: ");
    Serial.print(stepInterval);
    Serial.println("ms");
  }

  for (int i = 0; i < maxSteps; i++) {
    unsigned long currentMillis = millis();
    
    // NON-BLOCKING weight check (only check every 100ms)
    if (currentMillis - lastWeightCheckTime >= 100) {
      lastWeightCheckTime = currentMillis;
      
      // Quick weight check (non-blocking)
      if (scale.is_ready()) {
        float currentWeight = scale.get_units(1);
        
        if (abs(currentWeight) >= abs(targetWeight)) {
          targetReached = true;
          if (checkMode) {
            Serial.print("Target weight reached: ");
            Serial.print(currentWeight, 3);
            Serial.print(" (target: ");
            Serial.print(targetWeight, 3);
            Serial.println(")");
          }
          break;
        }
        
        // Print progress (less frequently)
        weightSampleCounter++;
        if (weightSampleCounter >= 5) { // Print every ~500ms
          weightSampleCounter = 0;
          if (checkMode) {
            Serial.print("Progress: ");
            Serial.print(i + 1);
            Serial.print("/");
            Serial.print(maxSteps);
            Serial.print(" steps | Weight: ");
            Serial.print(currentWeight, 3);
            Serial.print(" | Target: ");
            Serial.println(targetWeight, 3);
          } else {
            float currentDistance = ((i + 1) * abs(maxDistance)) / maxSteps;
            Serial.print(totalStepsTaken);
            Serial.print("\t");
            Serial.print(totalDistanceMoved + currentDistance, 6);
            Serial.print("\t");
            Serial.println(currentWeight, 6);
          }
        }
      }
    }
    
    // Wait for the appropriate time between steps (NON-BLOCKING)
    if (currentMillis - stepStartTime >= stepInterval) {
      // Time to step!
      digitalWrite(stepPinLeft, HIGH);
      digitalWrite(stepPinRight, HIGH);
      delayMicroseconds(500); // Very short pulse (0.5ms)
      digitalWrite(stepPinLeft, LOW);
      digitalWrite(stepPinRight, LOW);
      stepStartTime = currentMillis;
      
      // Update tracking
      totalStepsTaken += 2; // Both motors stepped
      
      // Move to next step
      continue;
    }
    
    // Check for abort commands (non-blocking)
    if (Serial.available()) {
      String command = Serial.readString();
      processCommand(command);
      if (command.length() > 0) {
        isMoving = false;
        if (checkMode) Serial.println("Movement interrupted by command");
        return;
      }
    }
    
    // Small delay to prevent CPU hogging
    delay(1);
  }
  
  // Update total distance
  float stepsTaken = targetReached ? (totalStepsTaken - (totalStepsTaken % 2)) : (maxSteps * 2);
  float actualDistance = stepsTaken / stepsPerMm / 2; // Divide by 2 since both motors step
  totalDistanceMoved += actualDistance;
  
  if (checkMode) {
    if (targetReached) {
      Serial.print("Weight target reached | Actual distance: ");
    } else {
      Serial.print("Max distance reached | Actual distance: ");
    }
    Serial.print(actualDistance, 2);
    Serial.print("mm | Total distance: ");
    Serial.print(totalDistanceMoved, 2);
    Serial.println("mm");
  }
  
  isMoving = false;
}

void stepMotor(int stepPin, int dirPin, float distance, float speed, String motorName) {
  isMoving = true;
  digitalWrite(dirPin, distance > 0 ? HIGH : LOW);
  delay(1);
  
  float stepsPerMm = stepsPerRev / mmPerRev;
  float stepsPerSecond = speed * stepsPerMm;
  
  int stepInterval;
  if (stepsPerSecond < 1.0) {
    stepInterval = 1000;
  } else {
    stepInterval = 1000 / stepsPerSecond;
  }
  
  int steps = abs(distance) * stepsPerMm;
  int weightSampleCounter = 0;
  unsigned long stepStartTime = millis();

  // Store movement info for tracking
  currentMoveDistance = distance;
  currentMoveSpeed = speed;
  currentMoveType = motorName;

  if (checkMode) {
    Serial.print("Starting movement: ");
    Serial.print(motorName);
    Serial.print(" | Distance: ");
    Serial.print(distance, 2);
    Serial.print("mm | Speed: ");
    Serial.print(speed, 3);
    Serial.print("mm/s | Steps: ");
    Serial.println(steps);
  }

  for (int i = 0; i < steps; i++) {
    unsigned long currentMillis = millis();
    
    // Wait for the appropriate time between steps
    while (currentMillis - stepStartTime < stepInterval) {
      currentMillis = millis();
      
      if (Serial.available()) {
        String command = Serial.readString();
        processCommand(command);
        if (command.length() > 0) {
          isMoving = false;
          if (checkMode) Serial.println("Movement interrupted by command");
          return;
        }
      }
      
      delay(10);
    }
    
    // Time to step!
    digitalWrite(stepPin, HIGH);
    delay(1);
    digitalWrite(stepPin, LOW);
    stepStartTime = millis();
    
    // Update tracking
    totalStepsTaken++;
    
    // Weight sampling with movement data
    weightSampleCounter++;
    if (weightSamplingEnabled && weightSampleCounter >= WEIGHT_SAMPLE_INTERVAL) {
      weightSampleCounter = 0;
      if (scale.is_ready()) {
        if (checkMode) {
          printWeightCheckMode(i + 1, steps, motorName);
        } else {
          printWeightDataMode(i + 1, steps);
        }
      }
    }
    
    // Check for abort commands
    if (Serial.available()) {
      String command = Serial.readString();
      processCommand(command);
      if (command.length() > 0) {
        isMoving = false;
        if (checkMode) Serial.println("Movement interrupted by command");
        return;
      }
    }
  }
  
  // Update total distance
  totalDistanceMoved += abs(distance);
  
  if (checkMode) {
    Serial.print("Movement completed: ");
    Serial.print(motorName);
    Serial.print(" | Steps: ");
    Serial.print(steps);
    Serial.print(" | Total distance: ");
    Serial.print(totalDistanceMoved, 2);
    Serial.println("mm");
  }
  
  isMoving = false;
}

void stepMotorsSimultaneously(float distanceLeft, float distanceRight, float speed) {
  isMoving = true;
  digitalWrite(dirPinLeft, distanceLeft > 0 ? HIGH : LOW);
  digitalWrite(dirPinRight, distanceRight > 0 ? HIGH : LOW);
  delay(1);
  
  float stepsPerMm = stepsPerRev / mmPerRev;
  float stepsPerSecond = speed * stepsPerMm;
  
  int stepInterval;
  if (stepsPerSecond < 1.0) {
    stepInterval = 1000;
  } else {
    stepInterval = 1000 / stepsPerSecond;
  }
  
  int stepsLeft = abs(distanceLeft) * stepsPerMm;
  int stepsRight = abs(distanceRight) * stepsPerMm;
  int maxSteps = max(stepsLeft, stepsRight);
  int weightSampleCounter = 0;
  unsigned long stepStartTime = millis();

  // Store movement info
  currentMoveDistance = distanceLeft; // Using left distance for tracking
  currentMoveSpeed = speed;
  currentMoveType = "BOTH";

  if (checkMode) {
    Serial.print("Starting simultaneous movement | Distance: ");
    Serial.print(distanceLeft, 2);
    Serial.print("mm | Speed: ");
    Serial.print(speed, 3);
    Serial.print("mm/s | Steps (L/R): ");
    Serial.print(stepsLeft);
    Serial.print("/");
    Serial.println(stepsRight);
  }

  for (int i = 0; i < maxSteps; i++) {
    unsigned long currentMillis = millis();
    
    while (currentMillis - stepStartTime < stepInterval) {
      currentMillis = millis();
      if (Serial.available()) {
        String command = Serial.readString();
        processCommand(command);
        if (command.length() > 0) {
          isMoving = false;
          if (checkMode) Serial.println("Movement interrupted by command");
          return;
        }
      }
      delay(10);
    }
    
    // Step both motors
    if (i < stepsLeft) {
      digitalWrite(stepPinLeft, HIGH);
      delay(1);
      digitalWrite(stepPinLeft, LOW);
      totalStepsTaken++;
    }
    if (i < stepsRight) {
      digitalWrite(stepPinRight, HIGH);
      delay(1);
      digitalWrite(stepPinRight, LOW);
      totalStepsTaken++;
    }
    stepStartTime = millis();
    
    // Weight sampling with movement data
    weightSampleCounter++;
    if (weightSamplingEnabled && weightSampleCounter >= WEIGHT_SAMPLE_INTERVAL) {
      weightSampleCounter = 0;
      if (scale.is_ready()) {
        if (checkMode) {
          printWeightCheckMode(i + 1, maxSteps, "BOTH");
        } else {
          printWeightDataMode(i + 1, maxSteps);
        }
      }
    }
    
    if (Serial.available()) {
      String command = Serial.readString();
      processCommand(command);
      if (command.length() > 0) {
        isMoving = false;
        if (checkMode) Serial.println("Movement interrupted by command");
        return;
      }
    }
  }
  
  totalDistanceMoved += abs(distanceLeft);
  
  if (checkMode) {
    Serial.print("Simultaneous movement completed | Total distance: ");
    Serial.print(totalDistanceMoved, 2);
    Serial.println("mm");
  }
  
  isMoving = false;
}

void processCommand(String cmd) {
  cmd.trim();
  
  if (cmd == "D3") {
    resetMovementCount();
    return;
  }
  
  if (cmd == "D10") {
    scale.tare();
    Serial.println("Scale tared");
    return;
  }
  
  if (cmd == "D11") {
    weightSamplingEnabled = !weightSamplingEnabled;
    Serial.print("Weight sampling during movement: ");
    Serial.println(weightSamplingEnabled ? "ON" : "OFF");
    return;
  }

  if (cmd == "D12") {
    printMovementStats();
    return;
  }

  if (cmd == "D100") {
    toggleOutputMode();
    return;
  }

  // D4 command - Weight-controlled movement
  if (cmd.startsWith("D4")) {
    float maxDistance = 5.0;    // Default
    float speed = 1.0;          // Default
    float targetWeight = 0.05;  // Default
    
    // Parse parameters
    int zIndex = cmd.indexOf('Z');
    int fIndex = cmd.indexOf('F');
    int wIndex = cmd.indexOf('W');
    
    if (zIndex != -1) {
      int endIndex = cmd.length();
      if (fIndex != -1) endIndex = fIndex;
      else if (wIndex != -1) endIndex = wIndex;
      maxDistance = cmd.substring(zIndex + 1, endIndex).toFloat();
    }
    
    if (fIndex != -1) {
      int endIndex = cmd.length();
      if (wIndex != -1) endIndex = wIndex;
      speed = cmd.substring(fIndex + 1, endIndex).toFloat();
    }
    
    if (wIndex != -1) {
      targetWeight = cmd.substring(wIndex + 1).toFloat();
    }
    
    speed = constrain(speed, 0.01, 100.0);
    
    if (checkMode) {
      Serial.print("D4 command: Z");
      Serial.print(maxDistance, 2);
      Serial.print(" F");
      Serial.print(speed, 3);
      Serial.print(" W");
      Serial.println(targetWeight, 3);
    }
    
    moveUntilWeight(maxDistance, speed, targetWeight);
    return;
  }

  // Regular movement commands
  float distance = 0.0, speed = defaultSpeed;
  int zIndex = cmd.indexOf('Z');
  int fIndex = cmd.indexOf('F');
  
  if (zIndex != -1) distance = cmd.substring(zIndex + 1, fIndex != -1 ? fIndex : cmd.length()).toFloat();
  if (fIndex != -1) speed = cmd.substring(fIndex + 1).toFloat();

  speed = constrain(speed, 0.01, 100.0);

  if (cmd.startsWith("D0")) {
    stepMotorsSimultaneously(distance, distance, speed);
    if (checkMode) Serial.print("Both moved ");
  }
  else if (cmd.startsWith("D1")) {
    stepMotor(stepPinLeft, dirPinLeft, distance, speed, "LEFT");
    if (checkMode) Serial.print("Left moved ");
  }
  else if (cmd.startsWith("D2")) {
    stepMotor(stepPinRight, dirPinRight, distance, speed, "RIGHT");
    if (checkMode) Serial.print("Right moved ");
  }
  else {
    Serial.println("Error: Invalid command");
    return;
  }
  
  if (checkMode) {
    Serial.print(distance, 2);
    Serial.print("mm @ ");
    Serial.print(speed, 3);
    Serial.println("mm/s");
  }
}

void loop() {
  if (Serial.available()) {
    processCommand(Serial.readStringUntil('\n'));
  }

  if (!isMoving && millis() - lastWeightPrint >= 1000) {
    printIdleWeight();
    lastWeightPrint = millis();
  }
}