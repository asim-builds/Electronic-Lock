#define MANUAL_UNLOCK_BTN 4   // Button for manually unlocking when authenticated
#define LED_GREEN_PIN 5       // Green LED for unlocked status
#define LED_RED_PIN 6         // Red LED for locked status

// Timing configuration
const unsigned long SESSION_DURATION = 7200000;  // Session mode duration (2 hours in ms)
const unsigned long SLEEP_AFTER_TIME = 60000;    // Enter sleep mode after 1 minute of inactivity
const unsigned long DEBOUNCE_DELAY = 50;         // Button debounce time in milliseconds

// State variables
unsigned long sessionStartTime = 0;    // Timestamp when session mode began
unsigned long lastActivityTime = 0;    // Timestamp of last activity
unsigned long lastDebounceTime = 0;    // For button debouncing

// State flags
bool sessionModeActive = false;   // Flag to track if session mode is active
bool unlockedByAuth = false;      // Flag to track if unlocked by authentication
bool manualUnlocked = false;      // Flag to track if manually unlocked
bool sleepEnabled = true;         // Flag to control sleep mode

// Button state tracking
int lastButtonState = HIGH;       // Last stable state of the button
int buttonState = HIGH;           // Current reading from the button

// ISR for waking up from sleep mode
void wakeUp() {
  // This function will be called when interrupt is triggered
  // No need to do anything here, just waking up
}

void setup() {
  Serial.begin(9600);

  pinMode(MANUAL_UNLOCK_BTN, INPUT_PULLUP);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);

  // Set initial LED state (locked - red on)
  updateLEDs(false);

  // Attach interrupt for waking up from sleep
  attachInterrupt(digitalPinToInterrupt(MANUAL_UNLOCK_BTN), wakeUp, LOW);
  
  // Initialize activity timer
  lastActivityTime = millis();

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
}

void loop() {
  // Check manual unlock button
  checkManualUnlock();
  
  // Handle unlock timer
  handleUnlockTimer();
  
  // Handle session mode timer
  handleSessionTimer();
  
  // Check if we should enter sleep mode
  checkSleep();
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

void handleSessionTimer() {
  if (sessionModeActive) {
    if (millis() - sessionStartTime >= SESSION_DURATION) {
      sessionModeActive = false;
      Serial.println("Session mode expired");
    }
  }
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

/* I was thinking of implementing this mode when the Arduino runs on battery backup, ensuring that it draws
minimal power by only turing on the components that are absolutely needed. */

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