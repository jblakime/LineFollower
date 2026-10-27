// =====================================================================
//  Interrupt proof of concept - DFRduino Nano
//  Wiring (same as the schematic):
//    Button between D3 and VCC, 10k pull-down from D3 to GND (pressed = HIGH)
//    HC-08 TXD -> D12, HC-08 RXD <- D11 (via divider), VCC 3V3, GND
//    LED: built-in LED on D13
//
//  What it shows:
//    - A hardware interrupt (INT1, RISING) toggles the LED immediately,
//      even while loop() is busy with a slow blocking task.
//    - Debounce: a press only counts after the button was stably released,
//      so bounce on press AND release is ignored.
//    - Every accepted press is reported over Bluetooth with a counter.
// =====================================================================
#include <SoftwareSerial.h>

const uint8_t PIN_BUTTON = 3;            // INT1
const uint8_t PIN_LED    = LED_BUILTIN;  // D13
const unsigned long DEBOUNCE_MS = 50;    // min. time between accepted presses
const unsigned long RELEASE_MS  = 50;    // stably LOW this long = released

SoftwareSerial bt(12, 11);               // RX D12, TX D11

volatile bool ledOn = false;
volatile bool armed = true;              // true = next RISING edge may count
volatile bool pressEvent = false;        // set by ISR, handled in loop()
volatile unsigned long lastPressMs = 0;
volatile unsigned int pressCount = 0;

// ---------------- ISR: keep it short ----------------
void onButton() {
  unsigned long now = millis();
  if (armed && now - lastPressMs > DEBOUNCE_MS) {
    ledOn = !ledOn;
    digitalWrite(PIN_LED, ledOn);        // react instantly, inside the ISR
    lastPressMs = now;
    pressCount++;
    armed = false;                       // ignore everything until released
    pressEvent = true;                   // printing is done in loop(), not here
  }
}

void setup() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUTTON, INPUT);            // external pull-down, no INPUT_PULLUP
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), onButton, RISING);

  bt.begin(9600);
  bt.println(F("Interrupt PoC ready. Press the button."));
}

void loop() {
  // 1) Re-arm the button once it has been stably released
  static unsigned long lowSince = 0;
  if (digitalRead(PIN_BUTTON) == HIGH) {
    lowSince = millis();
  } else if (!armed && millis() - lowSince >= RELEASE_MS) {
    armed = true;
  }

  // 2) Report presses (copy volatile data with interrupts off)
  if (pressEvent) {
    noInterrupts();
    pressEvent = false;
    unsigned int count = pressCount;
    bool state = ledOn;
    interrupts();
    bt.print(F("Press #")); bt.print(count);
    bt.print(F("  LED = ")); bt.println(state ? F("ON") : F("OFF"));
  }

  // 3) Deliberately slow "work": without an interrupt you would miss presses
  //    here; with the interrupt the LED still toggles instantly.
  delay(500);
}
