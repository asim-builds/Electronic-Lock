# 🔒 RFID-Based Smart Lock System using Arduino

A smart drawer locking system that uses RFID authentication and manual controls, displayed on an OLED screen, to manage access. This project was built on an **Arduino UNO** with a focus on modular design and real-world usability. It also explores limitations of embedded systems like memory constraints, debounce issues, and display update latency.

---

## 📌 Overview

This project aims to:

- Authenticate users using RFID tags.
- Control a solenoid lock based on authentication.
- Provide visual feedback via an OLED screen.
- Integrate session modes and manual button override (with limitations).
- Explore expandability with keypad input and battery backup.

---

## ⚙️ Variables Breakdown

### Authentication & Session Management

| Variable              | Type  | Description |
|-----------------------|-------|-------------|
| `unauthorized`        | bool  | Indicates if the scanned UID is unauthorized. Used to break out of the loop early. |
| `match`               | bool  | Temporary flag used during UID comparison loop. |
| `unlockedByAuth`      | bool  | Tracks if door was unlocked via RFID or keypad. |
| `manualUnlocked`      | bool  | Becomes true when the manual unlock button is used post-authentication. |
| `unlockTimerActive`   | bool  | True when the unlock timer is currently running. |
| `sessionModeActive`   | bool  | True if session mode is active, which affects behavior of manual button unlock. |

### Button Debouncing

| Variable              | Type  | Description |
|-----------------------|-------|-------------|
| `reading`             | int   | Current state of the manual unlock button from digitalRead(). |
| `lastButtonState`     | int   | Last known stable state before bounce filtering. |
| `buttonState`         | int   | Final debounced state of the button used in logic decisions. |

### OLED Display Management

| Variable                  | Type  | Description |
|---------------------------|-------|-------------|
| `DISPLAY_UPDATE_RATE`     | int   | How often the OLED is refreshed (in ms). Reduces flicker. |
| `temporaryMessageActive`  | bool  | True when a temporary message is currently showing. |
| `statusScreenActive`      | bool  | True when a status message is being shown. |
| `TEMP_MESSAGE_DURATION`   | long  | Duration for temporary messages to stay on screen (in ms). |
| `STATUS_DISPLAY_DURATION` | long  | Duration for status messages to stay on screen (in ms). |

---

## 📇 How RFID Cards Are Authenticated

- `authorizedUIDs[][4]`: A 2D array containing allowed RFID UIDs (each 4 bytes long).
- `NUM_AUTHORIZED_CARDS`: Calculated automatically based on array length.

### Flow:
1. User scans RFID card.
2. `checkRFID()` is called.
3. If door is **already unlocked**, the scan is ignored.
4. If not, the card is read using `rfid.PICC_ReadCardSerial()`.
5. If UID matches any in `authorizedUIDs`, `unlockDoor()` is called.

---

## 🔓 How `unlockDoor()` Works

1. Activates the solenoid (HIGH signal).
2. Turns on the Green LED.
3. Sets `unlockedByAuth = true` and `manualUnlocked = false`.
4. Activates **session mode**, which allows special behaviors (explained below).

---

## 🔒 How `lockDoor()` Works

1. Deactivates the solenoid (LOW signal).
2. Turns on the Red LED.
3. Sets `unlockTimerActive = false`.

---

## 🖥 OLED Display Workflow

Managed via the `updateDisplay()` function, which runs every **250 ms** using the `DISPLAY_UPDATE_RATE` to reduce flicker and lag.

### Display Modes:
- **Intro Screen**: Default idle screen using `showIntroScreen()`.
- **Temporary Message**: Shown using `showTemporaryMessage()` for a short time.
- **Status Screen**: Uses `showStatus()` to show current lock status or other data.

Flags like `temporaryMessageActive` and `statusScreenActive` control which screen is shown. Once their durations elapse, the display returns to the intro screen.

---


## Cancelled Phases
## 🚪 Phase 2: Manual Button Integration

- Implemented a **Session Mode**: a temporary state activated **after RFID/Numpad unlock**. This allows manual button usage without re-authentication.
- Intended to improve usability during active sessions (e.g., keep drawer open for repeated access).
  
**Issue Encountered**:
- OLED updates caused **latency** due to low RAM on the Arduino UNO.
- This interfered with the **debounce logic** of the manual button.
- Result: Button presses were **inconsistently registered**.

💡 **Final Decision**: Instead of compromising system stability by removing debounce, the manual button feature was disabled. However, the code logic for future upgrades remains intact.

---

## 🔢 Phase 3: Keypad Integration

Originally planned to integrate a **4x4 matrix keypad**.

### Problem:
- Arduino UNO/Nano has only **13 digital pins**.
- The keypad alone requires **8 pins**.
  
### Solution Options:
- Use an **Arduino Mega**.
- Add **I2C GPIO expanders**.

💡 **Final Decision**: Feature put on hold to avoid additional cost.

---

## 🔋 Phase 5: Battery Backup (Abandoned)

Initially planned to implement **battery backup** for Arduino.

### Realization:
- Backup needs to support the **12V solenoid** as well.
- A full **UPS system** for such a small project is **not cost-effective**.

💡 **Final Decision**: Feature abandoned to keep the project practical and affordable.

---

## 🛠 Migration to Perfboard (Cancelled)

After building and testing the project on a **breadboard**, the goal was to make it permanent by migrating to a **perfboard**.

### Barrier:
- Required purchasing a **UPS** to power Arduino + Solenoid.
- This was not a smart choice for a small-scale drawer-lock system.

💡 **Final Decision**: Project remains a **functional prototype** on the breadboard.

---

## 📈 Lessons Learned

- Embedded systems are constrained by **RAM**, **I/O pins**, and **power**.
- Good planning upfront helps prevent **feature creep**.
- Debounce logic and timing is critical for reliable hardware interactions.
- OLED screens and real-time feedback can affect **critical performance** on limited microcontrollers.

---

## ✅ Current Features

- [x] RFID-based unlocking
- [x] Solenoid control (lock/unlock)
- [x] OLED display for feedback
- [x] Modular and upgradeable code

---

## 🚧 Planned/Future Features

- [ ] Keypad authentication
- [ ] Battery backup
- [ ] Manual button integration with better hardware

---

## 📷 Demo (Optional)

*Include photos, wiring diagrams, or demo video here if available.*

---

## 🧠 Final Thoughts

This project started as a basic drawer lock, and evolved into a deep exploration of embedded system design, from handling I/O constraints to UI and debouncing challenges. Even though all features weren't implemented, the structure leaves plenty of room for upgrades.

