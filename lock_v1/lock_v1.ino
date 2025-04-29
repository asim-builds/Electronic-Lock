// Arduino Solenoid Lock Control
// Pin definitions
const int solenoidPin = 2;  // Digital pin connected to MOSFET/transistor gate
const int unlockTime = 3000;  // Time to keep solenoid energized (in milliseconds)

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(9600);
  Serial.println("Solenoid Lock Control System");

  // Set solenoid pin as output
  pinMode(solenoidPin, OUTPUT);

  // Ensure solenoid is locked at startup
  digitalWrite(solenoidPin, LOW);
}

void loop() {
  // Check if command received through serial
  if (Serial.available() > 0) {
    char command = Serial.read();

    if (command == 'u' || command == 'U') {
      unlockDoor();
    } else if (command == 'l' || command == 'L') {
      lockDoor();
    } else if (command == 's' || command == 'S') {
      // Status request
      Serial.println("Door is currently: " + String(digitalRead(solenoidPin) == HIGH ? "UNLOCKED" : "LOCKED"));
    }
  }

  // Add any additional loop logic here
  // For example, you might want to check RFID or other sensors
}

void unlockDoor() {
  Serial.println("Unlocking door...");
  digitalWrite(solenoidPin, HIGH);  // Energize solenoid to unlock
  delay(unlockTime);                // Keep unlocked for the specified time
  digitalWrite(solenoidPin, LOW);   // De-energize solenoid to lock again
  Serial.println("Door locked again after timeout");
}

void lockDoor() {
  Serial.println("Locking door...");
  digitalWrite(solenoidPin, LOW);   // De-energize solenoid to lock
  Serial.println("Door locked");
}