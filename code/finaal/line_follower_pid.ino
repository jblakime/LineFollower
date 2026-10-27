// =====================================================================
//  PID line follower - DFRduino Nano + DRV8833 + HY-S301 + HC-08
//  - Hardware interrupt on D3 stops the robot instantly
//  - Crossovers (most sensors black) are driven straight through
//  - Read sensors, tune Kp/Ki/Kd/base speed and start/stop over Bluetooth
// =====================================================================
#include <SoftwareSerial.h>

// ---------------- Pins ----------------
const int SENSOR_PINS[8] = {A0, A1, A2, A3, A4, A5, A6, A7}; // A0 = leftmost
const int PIN_IR     = 2;    // HY-S301 IR: HIGH = IR emitters on
const int PIN_BUTTON = 3;    // button to VCC, external 10k pull-down, INT1 (pressed = HIGH)
const int PIN_AIN1 = 5, PIN_AIN2 = 6;    // left motor
const int PIN_SLEEP = 8;                 // DRV8833 nSLEEP
const int PIN_BIN1 = 9, PIN_BIN2 = 10;   // right motor
const int PIN_LED  = 13;                 // built-in LED = status

SoftwareSerial bt(12, 11);   // RX D12 <- HC-08 TXD, TX D11 -> HC-08 RXD (via divider)

// ---------------- Settings ----------------
const bool BLACK_IS_HIGH         = true;   // false if black gives LOW raw values
const bool SENSOR_ORDER_REVERSED = false;  // true if A0 is the RIGHTmost sensor
const bool LEFT_MOTOR_REVERSED   = false;
const bool RIGHT_MOTOR_REVERSED  = false;

const int   MAX_PWM       = 190;   // 190/255 * 8.4 V = ~6.3 V -> protects 6 V motors
const int   LOOP_MS       = 5;     // control loop period (200 Hz)
const float DT            = LOOP_MS / 1000.0;
const int   BLACK_LEVEL   = 500;   // normalized 0..1000, above = "sees black"
const int   CROSS_COUNT   = 6;     // this many sensors black = crossover
const int   CROSS_HOLD_MS = 60;    // keep driving straight this long after crossover
const float I_MAX         = 60;    // anti-windup: max PWM the I-term may add

int   baseSpeed = 110;
float Kp = 0.03;    // start value: full error (3500) -> ~105 PWM correction
float Ki = 0.0;     // add after Kp is tuned (only if there is a constant offset)
float Kd = 0.0;     // damping: start around 0.1-0.5, uses change in error per loop

// ---------------- State ----------------
volatile bool running = false;
volatile bool stopArmed = false;   // true once the button is stably released while running
volatile unsigned long stopTime = 0;

int  minV[8], maxV[8];     // calibration (defaults to full range until 'c' is sent)
int  blackCount = 0;
long lastPosition = 3500;
float iTerm = 0;
long  lastError = 0;
bool  resetD = true;       // true = skip D once (after start or crossover, avoids a kick)
unsigned long lastCrossSeen = 0;
bool wasRunning = false;

// ---------------- Motors ----------------
void setMotor(int in1, int in2, int speed, bool reversed) {
  speed = constrain(speed, -MAX_PWM, MAX_PWM);
  if (reversed) speed = -speed;
  if (speed >= 0) { analogWrite(in1, speed); digitalWrite(in2, LOW); }
  else            { digitalWrite(in1, LOW);  analogWrite(in2, -speed); }
}

void setMotors(int left, int right) {
  setMotor(PIN_AIN1, PIN_AIN2, left,  LEFT_MOTOR_REVERSED);
  setMotor(PIN_BIN1, PIN_BIN2, right, RIGHT_MOTOR_REVERSED);
}

void stopMotors() {
  digitalWrite(PIN_AIN1, LOW); digitalWrite(PIN_AIN2, LOW);
  digitalWrite(PIN_BIN1, LOW); digitalWrite(PIN_BIN2, LOW);
}

// ---------------- Stop interrupt ----------------
// Only STOPS (a bounce or glitch can never start the robot).
// Starting is done in loop() with a debounced press, or with 's'.
// stopArmed blocks the bounce edges when releasing the button that just
// started the robot (they are RISING edges too and would stop it again).
void onButtonISR() {
  if (running && stopArmed) {
    running = false;
    stopArmed = false;
    stopMotors();
    stopTime = millis();
  }
}

bool buttonPressedDebounced() {
  static bool lastRaw = false, stable = false;
  static unsigned long changedAt = 0;
  bool raw = (digitalRead(PIN_BUTTON) == HIGH);   // pull-down: pressed = HIGH
  if (raw != lastRaw) { lastRaw = raw; changedAt = millis(); }
  if (millis() - changedAt > 30) stable = raw;
  return stable;
}

// ---------------- Sensors ----------------
// Reads all 8 sensors. Result: high = BLACK (0..1023).
void readSensors(int out[8]) {
  for (int i = 0; i < 8; i++) {
    int raw = analogRead(SENSOR_PINS[i]);
    out[i] = BLACK_IS_HIGH ? raw : 1023 - raw;
  }
}

void resetCalibration() {
  for (int i = 0; i < 8; i++) { minV[i] = 0; maxV[i] = 1023; }
}

void calibrate() {
  for (int i = 0; i < 8; i++) { minV[i] = 1023; maxV[i] = 0; }
  bt.println(F("Calibrating 5 s: slide the sensor over line and background"));
  unsigned long start = millis();
  int s[8];
  while (millis() - start < 5000) {
    readSensors(s);
    for (int i = 0; i < 8; i++) {
      if (s[i] < minV[i]) minV[i] = s[i];
      if (s[i] > maxV[i]) maxV[i] = s[i];
    }
    digitalWrite(PIN_LED, (millis() / 100) % 2);   // fast blink
  }
  digitalWrite(PIN_LED, LOW);
  bt.print(F("Calibration done. min/max per sensor: "));
  for (int i = 0; i < 8; i++) {
    bt.print(minV[i]); bt.print('/'); bt.print(maxV[i]); bt.print(' ');
  }
  bt.println();
}

// Returns 0 (line under leftmost sensor) .. 7000 (rightmost), 3500 = centre
long readPosition() {
  long weighted = 0, sum = 0;
  blackCount = 0;
  int s[8];
  readSensors(s);

  for (int i = 0; i < 8; i++) {
    int idx = SENSOR_ORDER_REVERSED ? 7 - i : i;
    int range = maxV[i] - minV[i];
    if (range < 50) range = 50;                       // avoid divide-by-tiny
    long v = (long)(s[i] - minV[i]) * 1000L / range;
    v = constrain(v, 0, 1000);

    if (v > BLACK_LEVEL) blackCount++;
    if (v > 100) {                                    // ignore background noise
      weighted += v * (idx * 1000L);
      sum += v;
    }
  }

  if (blackCount == 0) {
    // Line lost: steer hard toward the side we last saw it
    return (lastPosition < 3500) ? 0 : 7000;
  }
  lastPosition = weighted / sum;
  return lastPosition;
}

// ---------------- Bluetooth commands ----------------
// Commands (end with newline):
//   r       print the 8 sensor values (0 = white .. 1023 = black)
//   c       calibrate (5 s, slide sensor over line and background)
//   p0.04   set Kp
//   i0.05   set Ki
//   d0.3    set Kd
//   b120    set base speed (0..MAX_PWM)
//   s       start / stop
//   ?       show settings and command list
void printSettings() {
  bt.print(running ? F("RUNNING ") : F("STOPPED "));
  bt.print(F("Kp=")); bt.print(Kp, 4);
  bt.print(F(" Ki=")); bt.print(Ki, 4);
  bt.print(F(" Kd=")); bt.print(Kd, 4);
  bt.print(F(" base=")); bt.println(baseSpeed);
}

void printHelp() {
  bt.println(F("Commands: r c p<Kp> i<Ki> d<Kd> b<base> s ?"));
}

void runCommand(char *cmd) {
  float val = atof(cmd + 1);
  char letter = tolower(cmd[0]);   // accept R as well as r (phone auto-capitalize)
  switch (letter) {
    case 'p': Kp = val; break;
    case 'i': Ki = val; iTerm = 0; break;
    case 'd': Kd = val; resetD = true; break;
    case 'b': baseSpeed = constrain((int)val, 0, MAX_PWM); break;
    case 's':
      noInterrupts();
      if (running) { running = false; stopArmed = false; stopMotors(); stopTime = millis(); }
      else running = true;   // stopArmed is set by loop() once the button reads released
      interrupts();
      break;
    case 'r': {
      int v[8]; readSensors(v);
      for (int k = 0; k < 8; k++) { bt.print(v[k]); bt.print(' '); }
      bt.println();
      return;
    }
    case 'c':
      if (running) { bt.println(F("Stop the robot first (s)")); return; }
      calibrate();
      return;
    case '?': printSettings(); printHelp(); return;
    default:
      bt.print(F("Unknown command: '")); bt.print(cmd);
      bt.print(F("' (first byte code ")); bt.print((int)(byte)cmd[0]); bt.println(')');
      printHelp();
      return;
  }
  printSettings();
}

// A command ends at a newline, OR when nothing new arrives for 50 ms
// (so apps that don't send a newline, like BLE Scanner, also work).
void handleBluetooth() {
  static char buf[16];
  static byte len = 0;
  static unsigned long lastCharAt = 0;
  while (bt.available()) {
    char c = bt.read();
    lastCharAt = millis();
    if (c == '\n' || c == '\r') {
      if (len > 0) { buf[len] = '\0'; runCommand(buf); len = 0; }
    } else if (c != ' ' && len < sizeof(buf) - 1) {
      buf[len++] = c;
    }
  }
  if (len > 0 && millis() - lastCharAt > 50) {
    buf[len] = '\0'; runCommand(buf); len = 0;
  }
}

// ---------------- Setup ----------------
void setup() {
  pinMode(PIN_AIN1, OUTPUT); pinMode(PIN_AIN2, OUTPUT);
  pinMode(PIN_BIN1, OUTPUT); pinMode(PIN_BIN2, OUTPUT);
  pinMode(PIN_SLEEP, OUTPUT); digitalWrite(PIN_SLEEP, HIGH);   // wake DRV8833
  pinMode(PIN_IR, OUTPUT);    digitalWrite(PIN_IR, HIGH);      // IR emitters on (not floating)
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUTTON, INPUT);          // external 10k pull-down, no internal pull-up
  stopMotors();
  resetCalibration();

  bt.begin(9600);
  bt.println(F("Line follower ready."));
  printSettings();
  printHelp();

  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), onButtonISR, RISING);
}

// ---------------- Main loop ----------------
void loop() {
  handleBluetooth();

  static unsigned long lastLoop = 0;
  unsigned long now = millis();
  if (now - lastLoop < LOOP_MS) return;
  lastLoop = now;

  // --- Start via button (stop is handled by the interrupt) ---
  static bool prevPressed = false;
  bool pressed = buttonPressedDebounced();
  noInterrupts();
  unsigned long sinceStop = now - stopTime;
  interrupts();
  if (pressed && !prevPressed && !running && sinceStop > 300) {
    noInterrupts();
    running = true;
    stopArmed = false;                 // wait until the button is released
    interrupts();
  }
  prevPressed = pressed;
  if (running && !pressed) stopArmed = true;   // button stably released -> next press stops

  // --- Run state transitions ---
  if (!running) {
    if (wasRunning) { stopMotors(); digitalWrite(PIN_LED, LOW); }
    wasRunning = false;
    return;
  }
  if (!wasRunning) {           // just started
    iTerm = 0;
    resetD = true;
    lastPosition = 3500;
    digitalWrite(PIN_LED, HIGH);
  }
  wasRunning = true;

  // --- Sensors ---
  long position = readPosition();
  int left, right;

  if (blackCount >= CROSS_COUNT) lastCrossSeen = now;

  if (now - lastCrossSeen < CROSS_HOLD_MS) {
    // Crossover: ignore the sensors, drive straight, freeze I and D
    left = right = baseSpeed;
    resetD = true;
  } else {
    // --- PID control ---
    long error = position - 3500;             // + = line is to the right

    iTerm += Ki * error * DT;
    iTerm = constrain(iTerm, -I_MAX, I_MAX);  // anti-windup

    // D: change in error since the previous loop (per 5 ms loop, so Kd stays a
    // practical number like 0.1-1.0). Skipped once after start/crossover.
    if (resetD) { lastError = error; resetD = false; }
    float dTerm = Kd * (error - lastError);
    lastError = error;

    float correction = Kp * error + iTerm + dTerm;

    left  = baseSpeed + (int)correction;
    right = baseSpeed - (int)correction;
  }

  if (running) setMotors(left, right);  // re-check: ISR may have fired meanwhile
}
