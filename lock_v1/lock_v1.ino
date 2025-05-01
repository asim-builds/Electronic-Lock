// Arduino Solenoid Lock Control with RFID - Improved Version

#include <avr/wdt.h> 
#include <avr/sleep.h>
#include <avr/power.h>
#include <SPI.h>
#include <MFRC522.h>

// Configuration flags
#define DEBUG true            // Set to false to disable serial command input

// Pin definitions
#define SS_PIN 10
#define RST_PIN 9
#define SOLENOID_PIN 2        // Digital pin connected to MOSFET/transistor gate
#define MANUAL_UNLOCK_BTN 4   // Button for manually unlocking when authenticated
#define LED_GREEN_PIN 5       // Green LED for unlocked status
#define LED_RED_PIN 6         // Red LED for locked status
// #define DRAWER_SENSOR_PIN 7   // Reed switch or hall effect sensor for drawer position

// RFID configuration
// Replace with your authorized card UIDs - you can add more as needed
byte authorizedUIDs[][4] = {
  {0xB3, 0x4A, 0x9E, 0x29},  // Example card 1
  {0x12, 0x34, 0x56, 0x78}   // Example card 2
};
#define NUM_AUTHORIZED_CARDS (sizeof(authorizedUIDs) / sizeof(authorizedUIDs[0]))

// Timing configuration
const unsigned long UNLOCK_TIME = 10000;          // Time to keep solenoid energized (ms)
const unsigned long SESSION_DURATION = 7200000;  // Session mode duration (2 hours in ms)
const unsigned long SLEEP_AFTER_TIME = 60000;    // Enter sleep mode after 1 minute of inactivity
const unsigned long DEBOUNCE_DELAY = 50;         // Button debounce time in milliseconds

// State variables
unsigned long unlockStartTime = 0;     // Timestamp when unlock began
unsigned long sessionStartTime = 0;    // Timestamp when session mode began
unsigned long lastActivityTime = 0;    // Timestamp of last activity
unsigned long lastDebounceTime = 0;    // For button debouncing

// State flags
bool unlockTimerActive = false;   // Flag to track if unlock timer is running
bool sessionModeActive = false;   // Flag to track if session mode is active
bool unlockedByAuth = false;      // Flag to track if unlocked by authentication
bool manualUnlocked = false;      // Flag to track if manually unlocked
bool sleepEnabled = true;         // Flag to control sleep mode
// bool drawerOpen = false;       // Flag to track if drawer is physically open

// Button state tracking
int lastButtonState = HIGH;       // Last stable state of the button
int buttonState = HIGH;           // Current reading from the button

// Create MFRC522 instance
MFRC522 rfid(SS_PIN, RST_PIN);

// ISR for waking up from sleep mode
void wakeUp() {
  // This function will be called when interrupt is triggered
  // No need to do anything here, just waking up
}

// Helper function to check if the door is already unlocked
bool isDoorUnlocked() {
  return digitalRead(SOLENOID_PIN) == HIGH;
}

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(9600);
  
  // Initialize SPI bus and RFID module
  SPI.begin();
  rfid.PCD_Init();
  
  // Set pin modes
  pinMode(SOLENOID_PIN, OUTPUT);
  pinMode(MANUAL_UNLOCK_BTN, INPUT_PULLUP);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  // pinMode(DRAWER_SENSOR_PIN, INPUT_PULLUP);  // Pull-up for reed switch

  // Ensure solenoid is locked at startup
  digitalWrite(SOLENOID_PIN, LOW);
  
  // Set initial LED state (locked - red on)
  updateLEDs(false);
  
  // Attach interrupt for waking up from sleep
  attachInterrupt(digitalPinToInterrupt(MANUAL_UNLOCK_BTN), wakeUp, LOW);
  
  // Initialize activity timer
  lastActivityTime = millis();

  // Enable the watchdog timer with a timeout of 8 seconds
  // wdt_enable(WDTO_8S);
  
  // Flash LEDs to indicate successful startup
  for (int i = 0; i < 2; i++) {
    digitalWrite(LED_GREEN_PIN, HIGH);
    digitalWrite(LED_RED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_GREEN_PIN, LOW);
    digitalWrite(LED_RED_PIN, LOW);
    delay(200);
  }
  updateLEDs(false);
  
  // Print startup message
  Serial.println("Solenoid Lock Control System with RFID");
  if (DEBUG) {
    Serial.println("DEBUG mode is ON - Serial commands are enabled");
    Serial.println("Commands: U=Unlock, L=Lock, S=Status, M=Toggle Session Mode, Z=Toggle Sleep Mode");
  } else {
    Serial.println("DEBUG mode is OFF - Serial commands are disabled");
  }
  Serial.println("Place your RFID card near the reader to unlock...");
}

void loop() {
  // Reset the watchdog timer at the beginning of each loop
  wdt_reset();

  // Check for RFID card
  checkRFID();
  
  // Check for serial commands (only if DEBUG is enabled)
  if (DEBUG) {
    checkSerial();
  } else {
    // Clear serial buffer even when not processing commands
    while (Serial.available()) {
      Serial.read();
    }
  }
  
  // Check manual unlock button
  checkManualUnlock();
  
  // Check drawer sensor (currently disabled)
  // checkDrawerSensor();
  
  // Handle unlock timer
  handleUnlockTimer();
  
  // Handle session mode timer
  handleSessionTimer();
  
  // Check if we should enter sleep mode
  checkSleep();
}

void checkRFID() {
  // Skips RFID checks if already unlocked.
  if (isDoorUnlocked()) {
    return;
  }

  // Check if there's a new card present
  if (!rfid.PICC_IsNewCardPresent()) return;
  
  // Try to read the card
  if (!rfid.PICC_ReadCardSerial()) return;

  // Log detailed card information
  logRFIDDetails();

  // Check if this is an authorized card
  boolean authorized = false;
  
  // Compare against all authorized UIDs
  for (byte cardIdx = 0; cardIdx < NUM_AUTHORIZED_CARDS; cardIdx++) {
    bool match = true;
    
    // Compare each byte of the UID
    for (byte i = 0; i < 4; i++) {
      if (rfid.uid.uidByte[i] != authorizedUIDs[cardIdx][i]) {
        match = false;
        break;
      }
    }
    
    if (match) {
      authorized = true;
      break;
    }
  }

  if (authorized) {
    Serial.println("Authorized card detected");
    unlockDoor();
  } else {
    Serial.println("Unauthorized card - access denied");
    // Flash red LED to indicate unauthorized card
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_RED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_RED_PIN, LOW);
      delay(100);
    }
    updateLEDs(false);
  }

  // Halt PICC and stop encryption
  rfid.PICC_HaltA(); 
  rfid.PCD_StopCrypto1();
  
  // Update activity timestamp
  lastActivityTime = millis();
}

void checkSerial() {
  // Process all available serial data to prevent buffer overflow
  while (Serial.available() > 0) {
    char command = Serial.read();
    lastActivityTime = millis(); // Update activity timestamp
    
    // Process only valid commands and ignore line endings/whitespace
    if (command == '\n' || command == '\r' || command == ' ' || command == '\t') {
      continue; // Skip whitespace and line endings
    }

    // If door is unlocked, only accept lock commands or status reqeusts.
    if (isDoorUnlocked()) {
      if (command == 'l' || command == 'L') {
        lockDoor();
      } else if (command == 's' || command == 'S') {
        printStatus();
      }
      continue; // Skip other commands while door is unlocked
    }
    
    // Process commands normally when door is locked
    if (command == 'u' || command == 'U') {
      unlockDoor();
    } else if (command == 'l' || command == 'L') {
      lockDoor();
    } else if (command == 's' || command == 'S') {
      // Status request
      printStatus();
    } else if (command == 'm' || command == 'M') {
      // Toggle session mode
      toggleSessionMode();
    } else if (command == 'z' || command == 'Z') {
      // Toggle sleep mode
      sleepEnabled = !sleepEnabled;
      Serial.println(sleepEnabled ? "Sleep mode enabled" : "Sleep mode disabled");
    } else {
      // Indicate unknown command
      Serial.print("Unknown command: ");
      Serial.println(command);
    }
    
    // Small delay to prevent hogging the CPU if there's a lot of serial data
    delay(10);
  }
}

void checkManualUnlock() {
  // If door is already unlocked, ignore button presses
  if (isDoorUnlocked()) {
    return;
  }

  // Read the current state of the button
  int reading = digitalRead(MANUAL_UNLOCK_BTN);
  
  // Check if the button state has changed
  if (reading != lastButtonState) {
    // Reset the debounce timer
    lastDebounceTime = millis();
  }
  
  // Check if enough time has passed since the last state change
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    // If the button state has changed
    if (reading != buttonState) {
      buttonState = reading;
      
      // Only process when the button is pressed (LOW)
      if (buttonState == LOW) {
        Serial.println("Button pressed");
        lastActivityTime = millis(); // Update activity timestamp
        
        if ((unlockedByAuth && !manualUnlocked) || sessionModeActive) {
          Serial.println("Manual unlock triggered");
          
          /* 
          // Uncomment after implementing drawer sensor
          if (drawerOpen) {
            Serial.println("Drawer already open - ignoring manual unlock");
            return;
          }
          */
          
          digitalWrite(SOLENOID_PIN, HIGH);   // Unlock
          updateLEDs(true);
          delay(UNLOCK_TIME);                 // Wait
          
          /* 
          // Uncomment after implementing drawer sensor
          if (!drawerOpen) {
            digitalWrite(SOLENOID_PIN, LOW);    // Lock again only if drawer is closed
            updateLEDs(false);
            Serial.println("Door re-locked after manual unlock");
          } else {
            Serial.println("Drawer is open - remaining unlocked");
          }
          */
          
          // Until drawer sensor is implemented, always lock after timeout
          digitalWrite(SOLENOID_PIN, LOW);
          updateLEDs(false);
          Serial.println("Door re-locked after manual unlock");
          
          manualUnlocked = true;  // Mark as manually unlocked once
        } else {
          Serial.println("Button press ignored - authentication required");
          // Flash red LED to indicate invalid button press
          for (int i = 0; i < 3; i++) {
            digitalWrite(LED_RED_PIN, HIGH);
            delay(100);
            digitalWrite(LED_RED_PIN, LOW);
            delay(100);
          }
          updateLEDs(false); // Reset LEDs to locked state
        }
      }
    }
  }
  
  // Save the current reading for next loop
  lastButtonState = reading;
}

/*
void checkDrawerSensor() {
  // Read the state of the drawer sensor
  bool currentDrawerState = (digitalRead(DRAWER_SENSOR_PIN) == LOW); // LOW means drawer is open (sensor triggered)
  
  // If drawer state has changed
  if (currentDrawerState != drawerOpen) {
    drawerOpen = currentDrawerState;
    lastActivityTime = millis(); // Update activity timestamp
    
    if (drawerOpen) {
      Serial.println("Drawer opened");
    } else {
      Serial.println("Drawer closed");
      // If drawer was just closed and solenoid is unlocked, lock it
      if (digitalRead(SOLENOID_PIN) == HIGH) {
        Serial.println("Drawer closed - locking solenoid");
        lockDoor();
      }
    }
  }
}
*/

void handleUnlockTimer() {
  if (unlockTimerActive) {
    if (millis() - unlockStartTime >= UNLOCK_TIME) {
      /* 
      // Uncomment after implementing drawer sensor
      if (!drawerOpen) {
        lockDoor(); // Lock the door after timeout only if drawer is closed
      } else {
        Serial.println("Unlock timer expired but drawer is open - remaining unlocked");
      }
      */
      
      // Until drawer sensor is implemented, always lock after timeout
      lockDoor();
      unlockTimerActive = false; // Deactivate timer
    }
  }
}

void handleSessionTimer() {
  if (sessionModeActive) {
    if (millis() - sessionStartTime >= SESSION_DURATION) {
      sessionModeActive = false;
      Serial.println("Session mode expired");
    }
  }
}

void unlockDoor() {
  Serial.println("Unlocking door...");
  digitalWrite(SOLENOID_PIN, HIGH);  // Energize solenoid to unlock
  updateLEDs(true);
  
  unlockStartTime = millis();       // Store the current time
  unlockTimerActive = true;         // Activate the timer
  lastActivityTime = millis();      // Update activity timestamp
  
  unlockedByAuth = true;
  manualUnlocked = false;
  
  Serial.println("Door unlocked - will auto-lock after timeout");
  
  // Automatically enter session mode when unlocked via authentication
  if (!sessionModeActive) {
    toggleSessionMode();
  }
}

void lockDoor() {
  Serial.println("Locking door...");
  digitalWrite(SOLENOID_PIN, LOW);   // De-energize solenoid to lock
  updateLEDs(false);
  
  unlockTimerActive = false;        // Deactivate the timer if it was running
  lastActivityTime = millis();      // Update activity timestamp
  
  Serial.println("Door locked");
}

void updateLEDs(bool unlocked) {
  if (unlocked) {
    digitalWrite(LED_GREEN_PIN, HIGH);
    digitalWrite(LED_RED_PIN, LOW);
  } else {
    digitalWrite(LED_GREEN_PIN, LOW);
    digitalWrite(LED_RED_PIN, HIGH);
  }
}

void toggleSessionMode() {
  sessionModeActive = !sessionModeActive;
  
  if (sessionModeActive) {
    sessionStartTime = millis();
    Serial.println("Session mode activated for 2 hours");
  } else {
    Serial.println("Session mode deactivated");
  }
}

void printStatus() {
  Serial.println("----------------------------------------");
  Serial.println("System Status:");
  Serial.println("Door is: " + String(digitalRead(SOLENOID_PIN) == HIGH ? "UNLOCKED" : "LOCKED"));
  Serial.println("Session mode: " + String(sessionModeActive ? "ACTIVE" : "INACTIVE"));
  
  if (sessionModeActive) {
    unsigned long sessionTimeLeft = SESSION_DURATION - (millis() - sessionStartTime);
    Serial.print("Session time remaining: ");
    Serial.print(sessionTimeLeft / 60000);
    Serial.println(" minutes");
  }
  
  Serial.println("Sleep mode: " + String(sleepEnabled ? "ENABLED" : "DISABLED"));
  // Serial.println("Drawer is: " + String(drawerOpen ? "OPEN" : "CLOSED"));
  Serial.println("----------------------------------------");
}

void checkSleep() {
  // Only consider sleep if enabled and enough time has passed without activity
  if (sleepEnabled && (millis() - lastActivityTime > SLEEP_AFTER_TIME)) {
    // Don't sleep if door is unlocked or drawer is open
    if (digitalRead(SOLENOID_PIN) == HIGH /* || drawerOpen */) {
      return;
    }
    
    enterSleepMode();
  }
}

void enterSleepMode() {
  Serial.println("Entering sleep mode to save power...");
  delay(100); // Allow serial to finish transmitting
  
  // Turn off LEDs to save power
  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_RED_PIN, LOW);
  
  // Configure sleep mode
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Deepest sleep mode
  sleep_enable();
  
  // Disable ADC to save power
  ADCSRA &= ~(1 << ADEN);
  
  // Enter sleep mode
  sleep_mode();
  
  // Code will resume here after waking up
  sleep_disable();
  
  // Re-enable ADC
  ADCSRA |= (1 << ADEN);
  
  // Reset activity timer
  lastActivityTime = millis();
  
  // Restore LED status
  updateLEDs(digitalRead(SOLENOID_PIN) == HIGH);
  
  Serial.println("Waking up from sleep mode");
}



void logRFIDDetails() {
  // Log the UID in both hex and decimal formats
  Serial.println("--------- RFID Card Details ---------");
  
  // Log in HEX format
  Serial.print("Card UID (HEX): ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.println();
  
  // Log in decimal format
  Serial.print("Card UID (DEC): ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(" ");
    Serial.print(rfid.uid.uidByte[i], DEC);
  }
  Serial.println();
  
  // Log the PICC type
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.print("PICC Type: ");
  Serial.println(rfid.PICC_GetTypeName(piccType));
  
  Serial.println("--------------------------------------");
}