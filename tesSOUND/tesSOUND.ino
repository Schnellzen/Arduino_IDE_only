#define TRIG_PIN 15
#define ECHO_PIN 2


float previousDistance = 0;
bool movementDetected = false;

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  Serial.println("Motion Detector Started");
  
  // Take initial reading
  previousDistance = readDistance();
  delay(100); // Small delay for stabilization
}

float readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.034 / 2;  // Using 340 m/s for air
  return distance;
}

void loop() {
  float currentDistance = readDistance();
  float distanceChange = abs(currentDistance - previousDistance);
  
  if (distanceChange > 1.0) {
    movementDetected = true;
    Serial.print("Movement detected! ");
    Serial.print("Change: ");
    Serial.print(distanceChange, 1);
    Serial.print("cm (");
    Serial.print(previousDistance, 1);
    Serial.print("cm -> ");
    Serial.print(currentDistance, 1);
    Serial.println("cm)");
  } else {
    if (movementDetected) {
      Serial.println("No movement - system calm");
      movementDetected = false;
    }
  }
  
  previousDistance = currentDistance;
  delay(200);  // 200ms delay between readings
}