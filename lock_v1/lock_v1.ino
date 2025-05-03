// Arduino Solenoid Lock Control with RFID and OLED Display

#include <avr/wdt.h> 
#include <avr/sleep.h>
#include <avr/power.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <U8g2lib.h>

// Configuration flags
#define DEBUG false            // Set to false to disable serial command input

// Pin definitions
#define SS_PIN 10
#define RST_PIN 9
#define SOLENOID_PIN 4        // Digital pin connected to MOSFET/transistor gate
#define BYPASS_LOCK_BTN 2   // Button for bypassing lock in case something goes wrong
// #define DRAWER_SENSOR_PIN 7   // Reed switch or hall effect sensor for drawer position

// OLED Display configuration
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// RFID configuration
// Replace with your authorized card UIDs - you can add more as needed
byte authorizedUIDs[][4] = {
  {0xB3, 0x4A, 0x9E, 0x29},  // Example card 1
  {0x12, 0x34, 0x56, 0x78}   // Example card 2
};
#define NUM_AUTHORIZED_CARDS (sizeof(authorizedUIDs) / sizeof(authorizedUIDs[0]))

// Timing configuration
const unsigned long UNLOCK_TIME = 10000;          // Time to keep solenoid energized (ms)
const unsigned long SLEEP_AFTER_TIME = 60000;     // Enter sleep mode after 1 minute of inactivity
const unsigned long DISPLAY_UPDATE_INTERVAL = 1000; // How often to refresh display (ms)
const unsigned long DISPLAY_MESSAGE_TIME = 3000;   // How long to show temporary messages (ms)
const unsigned long DISPLAY_INTRO_INTERVAL = 10000; // Intro screen stays for 10 seconds
unsigned long lastIntroScreenTime = 0;   // Track the intro screen display time
unsigned long lastStatusScreenTime = 0;  // Track the time of status screen display

// Watchdog timer period in milliseconds (8 hours = 28,800,000ms)
// Note: The actual watchdog timer has limited preset values, we'll use the longest available
const unsigned long WATCHDOG_PERIOD = 8000;       // 8 seconds between watchdog resets

// State variables
unsigned long unlockStartTime = 0;     // Timestamp when unlock began
unsigned long lastActivityTime = 0;    // Timestamp of last activity
unsigned long lastDebounceTime = 0;    // For button debouncing
unsigned long lastDisplayUpdate = 0;   // For display refresh timing
unsigned long messageStartTime = 0;    // For temporary message display
const unsigned long TEMP_MESSAGE_DURATION = 3000;  // 3 seconds for temporary message
const unsigned long STATUS_DISPLAY_DURATION = 10000; // 10 seconds for status display
unsigned long tempMessageStartTime = 0;
unsigned long statusScreenStartTime = 0;
unsigned long lastButtonCheck = 0;
unsigned long lastWatchdogReset = 0;   // Track the last time we reset the watchdog

// State flags
bool unlockTimerActive = false;   // Flag to track if unlock timer is running
bool unlockedByAuth = false;      // Flag to track if unlocked by authentication
bool manualUnlocked = false;      // Flag to track if manually unlocked
bool sleepEnabled = true;         // Flag to control sleep mode
bool temporaryMessageActive = false; // Flag for temporary message display
bool statusScreenActive = false;
// bool drawerOpen = false;       // Flag to track if drawer is physically open

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
  delay(100);
  
  // Initialize SPI bus and RFID module
  SPI.begin();
  rfid.PCD_Init();
  
  // Set pin modes
  pinMode(SOLENOID_PIN, OUTPUT);
  // pinMode(DRAWER_SENSOR_PIN, INPUT_PULLUP);  // Pull-up for reed switch

  // Ensure solenoid is locked at startup
  digitalWrite(SOLENOID_PIN, LOW);
  
  // Attach interrupt for waking up from sleep
  // attachInterrupt(digitalPinToInterrupt(MANUAL_UNLOCK_BTN), wakeUp, LOW);
  
  // Initialize activity timer
  lastActivityTime = millis();

  // Initialize OLED display
  u8g2.begin();
  delay(100);

  showIntroScreen();
  
  // Enable the watchdog timer with timeout of approximately 8 seconds
  // For a system used once or twice a day, we'll use the longest available timeout
  wdt_enable(WDTO_8S);  // 8 seconds is the longest standard timeout
  lastWatchdogReset = millis();
  
  // Print startup message
  Serial.println(F("Solenoid Lock Control System with RFID"));
  Serial.println(F("Watchdog timer enabled with 8-second timeout"));
  Serial.println(F("System will auto-reset if it becomes unresponsive"));
  
  if (DEBUG) {
    Serial.println(F("DEBUG mode is ON - Serial commands are enabled"));
    Serial.println(F("Commands: U=Unlock, L=Lock, S=Status, M=Toggle Session Mode, Z=Toggle Sleep Mode"));
  } else {
    Serial.println(F("DEBUG mode is OFF - Serial commands are disabled"));
  }
  Serial.println(F("Place your RFID card near the reader to unlock..."));
  
  // Update display with startup information
  // showStatus();
}

void loop() {
  // Get current time once per loop
  unsigned long currentMillis = millis();
  
  // Reset the watchdog timer only if a certain period has passed
  // This approach ensures we don't reset it on every loop iteration
  if (currentMillis - lastWatchdogReset >= WATCHDOG_PERIOD / 2) {
    wdt_reset();
    lastWatchdogReset = currentMillis;
  }

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
  
  // Handle unlock timer
  handleUnlockTimer();

  // Update display only occasionally (not every loop!)
  static unsigned long lastDisplayUpdate = 0;
  const unsigned long DISPLAY_UPDATE_RATE = 250; // Update display 4 times per second maximum
  
  if (currentMillis - lastDisplayUpdate >= DISPLAY_UPDATE_RATE) {
    lastDisplayUpdate = currentMillis;
    updateDisplay();
  }
}

void updateDisplay() {
  unsigned long currentMillis = millis();
  
  // First priority: Check if temporary message is active
  if (temporaryMessageActive) {
    // Check if temporary message duration has elapsed
    if (currentMillis - tempMessageStartTime >= TEMP_MESSAGE_DURATION) {
      temporaryMessageActive = false;
      
      // After temporary message, show status screen
      showStatus();
      statusScreenActive = true;
      statusScreenStartTime = currentMillis;
    }
    // Else: Keep showing temporary message
  }
  // Second priority: Check if status screen is active
  else if (statusScreenActive) {
    // Check if status screen duration has elapsed
    if (currentMillis - statusScreenStartTime >= STATUS_DISPLAY_DURATION) {
      statusScreenActive = false;
      
      // Return to intro screen after status screen times out
      showIntroScreen();
    }
    // Else: Keep showing status screen
  }
  // Default state: Show intro screen if nothing else is active
  else {
    // We're already showing the intro screen, nothing to do
  }
}

void showTemporaryMessage(const char* line1, const char* line2) {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_7x14B_tf);
    int w1 = u8g2.getStrWidth(line1);
    u8g2.drawStr((128 - w1) / 2, 20, line1);

    u8g2.setFont(u8g2_font_6x10_tf);
    int w2 = u8g2.getStrWidth(line2);
    u8g2.drawStr((128 - w2) / 2, 35, line2);
  } while (u8g2.nextPage());

  messageStartTime = millis();
  temporaryMessageActive = true;
  tempMessageStartTime = millis();
}

void showIntroScreen() {
  bool doorLocked = !isDoorUnlocked();

  u8g2.firstPage();
  do {
    // 1. Title
    u8g2.setFont(u8g2_font_7x14B_tf);  // Bold and readable
    const char* title = "Door Lock System";
    int titleWidth = u8g2.getStrWidth(title);
    int titleX = (128 - titleWidth) / 2;
    u8g2.drawStr(titleX, 20, title);  // Draw at Y = 20 for top section

    // 2. Status
    u8g2.setFont(u8g2_font_6x10_tf);  // Slightly smaller
    const char* status = doorLocked ? "Status: LOCKED" : "Status: UNLOCKED";
    int statusWidth = u8g2.getStrWidth(status);
    int statusX = (128 - statusWidth) / 2;
    u8g2.drawStr(statusX, 38, status);  // Draw below title

    // 3. Instruction (only if locked)
    if (doorLocked) {
      const char* instruction = "Scan RFID to open";
      int instrWidth = u8g2.getStrWidth(instruction);
      int instrX = (128 - instrWidth) / 2;
      u8g2.drawStr(instrX, 56, instruction);  // Near bottom
    }

  } while (u8g2.nextPage());
}

void showStatus() {
  u8g2.firstPage();
  do {
    // 1. Set font (small enough to fit lines, e.g. 6x10)
    u8g2.setFont(u8g2_font_6x10_tf);  // Suitable for 128x64

    // 2. Header
    u8g2.drawStr(0, 10, "Current Status Info");
    u8g2.drawStr(0, 20, "---------------------");

    // 3. Lock status
    u8g2.drawStr(0, 30, "Status:");
    u8g2.drawStr(60, 30, isDoorUnlocked() ? "UNLOCKED" : "LOCKED");

    // 5. Positive message
    u8g2.drawStr(0, 60, "Have a great day!");

  } while (u8g2.nextPage());
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
  if (DEBUG) {
    logRFIDDetails();
  }

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
    Serial.println(F("Authorized card detected"));
    showTemporaryMessage("Access granted!", "Drawer unlocked.");
    unlockDoor();
  } else {
    Serial.println(F("Unauthorized card - access denied"));
    
    // Show card UID on display
    char uidMsg[64];
    sprintf(uidMsg, "Access denied!\nUID:%02X %02X %02X %02X", 
            rfid.uid.uidByte[0], rfid.uid.uidByte[1], 
            rfid.uid.uidByte[2], rfid.uid.uidByte[3]);
    // showTemporaryMessage(uidMsg);
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
    } else if (command == 'z' || command == 'Z') {
      // Toggle sleep mode
      sleepEnabled = !sleepEnabled;
      Serial.println(sleepEnabled ? "Sleep mode enabled" : "Sleep mode disabled");
      // showTemporaryMessage(sleepEnabled ? "Sleep mode ON" : "Sleep mode OFF");
    } else {
      // Indicate unknown command
      Serial.print(F("Unknown command: "));
      Serial.println(command);
      
      char unknownMsg[32];
      sprintf(unknownMsg, "Unknown cmd: %c", command);
      // showTemporaryMessage(unknownMsg);
    }
    
    // Small delay to prevent hogging the CPU if there's a lot of serial data
    delay(10);
  }
}

void handleUnlockTimer() {
  if (unlockTimerActive) {
    if (millis() - unlockStartTime >= UNLOCK_TIME) {
      /* 
      // Uncomment after implementing drawer sensor
      if (!drawerOpen) {
        lockDoor(); // Lock the door after timeout only if drawer is closed
      } else {
        Serial.println(F("Unlock timer expired but drawer is open - remaining unlocked"));
      }
      */
      
      // Until drawer sensor is implemented, always lock after timeout
      lockDoor();
      unlockTimerActive = false; // Deactivate timer
    }
  }
}

void unlockDoor() {
  Serial.println(F("Unlocking door..."));
  digitalWrite(SOLENOID_PIN, HIGH);  // Energize solenoid to unlock
  
  unlockStartTime = millis();       // Store the current time
  unlockTimerActive = true;         // Activate the timer
  lastActivityTime = millis();      // Update activity timestamp
  
  unlockedByAuth = true;
  manualUnlocked = false;
  
  Serial.println(F("Door unlocked - will auto-lock after timeout"));
}

void lockDoor() {
  Serial.println(F("Locking door..."));
  digitalWrite(SOLENOID_PIN, LOW);   // De-energize solenoid to lock
  
  unlockTimerActive = false;        // Deactivate the timer if it was running
  lastActivityTime = millis();      // Update activity timestamp
  
  Serial.println(F("Door locked"));
  showTemporaryMessage("Timed Out", "Drawer Locked");
}


void printStatus() {
  Serial.println(F("----------------------------------------"));
  Serial.println(F("System Status:"));
  Serial.println("Door is: " + String(digitalRead(SOLENOID_PIN) == HIGH ? "UNLOCKED" : "LOCKED"));
  
  Serial.println("Sleep mode: " + String(sleepEnabled ? "ENABLED" : "DISABLED"));
  // Serial.println(F("Drawer is: " + String(drawerOpen ? "OPEN" : "CLOSED")));
  Serial.println(F("----------------------------------------"));
  
  // Update display with current status
  // showStatus();
}

void logRFIDDetails() {
  // Log the UID in both hex and decimal formats if DEBUG is set to True
  Serial.println(F("--------- RFID Card Details ---------"));
  
  // Log in HEX format
  Serial.print(F("Card UID (HEX): "));
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.println();
  
  // Log in decimal format
  Serial.print(F("Card UID (DEC): "));
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(" ");
    Serial.print(rfid.uid.uidByte[i], DEC);
  }
  Serial.println();
  
  // Log the PICC type
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.print(F("PICC Type: "));
  Serial.println(rfid.PICC_GetTypeName(piccType));
  
  Serial.println(F("--------------------------------------"));
  
  // Show card UID on display
  char uidMsg[64];
  sprintf(uidMsg, "Card detected\nUID:%02X %02X %02X %02X", 
          rfid.uid.uidByte[0], rfid.uid.uidByte[1], 
          rfid.uid.uidByte[2], rfid.uid.uidByte[3]);
  delay(1000); // Show UID briefly before checking authorization
}