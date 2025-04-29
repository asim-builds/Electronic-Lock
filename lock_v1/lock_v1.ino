// Arduino Solenoid Lock Control

#include <avr/wdt.h> 

// Pin definitions
const int solenoidPin = 2;  // Digital pin connected to MOSFET/transistor gate
const int unlockTime = 3000;  // Time to keep solenoid energized (in milliseconds)
unsigned long unlockStartTime = 0; // Timestamp when unlock began
bool unlockTimerActive = false; // Flag to track if unlock timer is running
const int manualUnlockButton = 4; // Manual Lock Button

bool unlockedByAuth = false;
bool manualUnlocked = false;

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(9600);
  Serial.println("Solenoid Lock Control System with Watchdog");

  // Set solenoid pin as output
  pinMode(solenoidPin, OUTPUT);

  // Ensure solenoid is locked at startup
  digitalWrite(solenoidPin, LOW);

  pinMode(manualUnlockButton, INPUT_PULLUP);

  // Enable the watchdog timer with a timeout of 8 seconds
  // wdt_enable(WDTO_8S);
}

void loop() {

  // Reset the watchdog timer at the beginning of each loop
  // wdt_reset();
  // Serial.print("Unlocked by Authentication: ");
  // Serial.println(unlockedByAuth);
  // Serial.print("Manual Unlocked: ");
  // Serial.println(manualUnlocked);

  if (digitalRead(manualUnlockButton) == LOW) {
    Serial.println("Button Pressed");
    delay(500);  // debounce
  }

  // Check if unlock timer is active
  if (unlockTimerActive){
    if (millis() - unlockStartTime >= unlockTime) {
      lockDoor(); // Lock the door after timeout
      unlockTimerActive = false; // Deactivate timer
    }
  }


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

  if (digitalRead(manualUnlockButton) == LOW) {
    if (unlockedByAuth && !manualUnlocked) {
      Serial.println("Manual re-unlock triggered.");
      digitalWrite(solenoidPin, HIGH);   // Re-unlock
      delay(unlockTime);                 // Wait
      digitalWrite(solenoidPin, LOW);    // Lock again
      Serial.println("Door re-locked after manual unlock.");

      manualUnlocked = true;  // Disable button until RFID used again
      delay(300);               // Debounce
    }
  }

  // Add any additional loop logic here
  // For example, you might want to check RFID or other sensors
}

void unlockDoor() {
  Serial.println("Unlocking door...");
  digitalWrite(solenoidPin, HIGH);  // Energize solenoid to unlock
  unlockStartTime = millis();       // Store the current time
  unlockTimerActive = true;         // Activate the timer
  Serial.println("Door unlocked - will auto-lock after timeout");

  unlockedByAuth = true;
  manualUnlocked = false;
}

void lockDoor() {
  Serial.println("Locking door...");
  digitalWrite(solenoidPin, LOW);   // De-energize solenoid to lock
  unlockTimerActive = false;        // Deactivate the timer if it was running
  Serial.println("Door locked");
}