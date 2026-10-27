/*
  Commando's (afsluiten met CR/LF):
    set cycle <us>   cyclustijd in microseconden (bv. set cycle 500000)
    debug on|off     debug-uitvoer aan/uit
    start            programma starten
    stop             programma stoppen
    params           huidige parameters tonen

  Bedrading (zelfde pinnen als het schema / line_follower_pid.ino):
    HC-08 TXD -> Nano D12 (SoftwareSerial RX)
    HC-08 RXD <- Nano D11 (SoftwareSerial TX) via spanningsdeler (5 V -> 3,3 V)
    HC-08 VCC 3V3, GND
    Drukknop tussen D3 en VCC, 10 kOhm pull-down van D3 naar GND (ingedrukt = HIGH)
    LED: ingebouwde LED (D13)

  De HC-08 zit niet meer op D0/D1: uploaden lukt zonder iets los te koppelen
  en de USB-seriele poort (Serial) blijft vrij voor debug.
*/

#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <SerialCommands.h>

// ---------------- Instellingen ----------------
#define PIN_BT_RX    12          // naar HC-08 TXD
#define PIN_BT_TX    11          // naar HC-08 RXD
#define BAUDRATE     9600        // standaard baudrate HC-08
#define PIN_BUTTON   3           // externe interrupt INT1, pull-down -> RISING
#define PIN_LED      LED_BUILTIN

SoftwareSerial bt(PIN_BT_RX, PIN_BT_TX);
#define SerialPort   bt          // HC-08 zit op SoftwareSerial

const unsigned long MIN_CYCLE_US   = 500;    // ondergrens cyclustijd
const unsigned long DEBOUNCE_MS    = 200;    // ontdendertijd drukknop
const unsigned long RELEASE_MS     = 50;     // knop zo lang stabiel LOW = losgelaten
const unsigned long DEBUG_PERIOD_MS = 500;   // debug-uitvoer max. 2x per seconde
const uint16_t      EEPROM_MAGIC   = 0xA5C3; // herkent geldige EEPROM-inhoud

// ---------------- Parameters (EEPROM) ----------------
struct Params {
  uint16_t      magic;
  unsigned long cycleTime;   // us
  bool          debug;
};
Params params;

// ---------------- Runtime-variabelen ----------------
volatile bool running = false;          // aangepast in ISR -> volatile
volatile unsigned long lastPressMs = 0;
volatile bool buttonArmed = true;       // pas na loslaten mag een nieuwe druk tellen

unsigned long previousCycle = 0;
unsigned long calculationTime = 0;      // duur van één cyclus (us)
unsigned long lastDebugMs = 0;

// ---------------- SerialCommands ----------------
char serialBuffer[32];
SerialCommands sCmd(&SerialPort, serialBuffer, sizeof(serialBuffer), "\r\n", " ");

void onSet(SerialCommands* sender);
void onDebug(SerialCommands* sender);
void onStart(SerialCommands* sender);
void onStop(SerialCommands* sender);
void onParams(SerialCommands* sender);
void onUnknown(SerialCommands* sender, const char* cmd);

SerialCommand cmdSet("set", onSet);
SerialCommand cmdDebug("debug", onDebug);
SerialCommand cmdStart("start", onStart);
SerialCommand cmdStop("stop", onStop);
SerialCommand cmdParams("params", onParams);

// ================================================================
void setup() {
  SerialPort.begin(BAUDRATE);

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUTTON, INPUT);   // externe pull-down, geen interne pull-up
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), onButton, RISING);

  loadParams();

  sCmd.SetDefaultHandler(onUnknown);
  sCmd.AddCommand(&cmdSet);
  sCmd.AddCommand(&cmdDebug);
  sCmd.AddCommand(&cmdStart);
  sCmd.AddCommand(&cmdStop);
  sCmd.AddCommand(&cmdParams);

  SerialPort.println(F("Klaar. Typ 'params' voor de instellingen."));
  printParams();

  previousCycle = micros();
}

// ================================================================
void loop() {
  // Commando's altijd verwerken, ook buiten de cyclus
  sCmd.ReadSerial();

  // Knop opnieuw "wapenen" zodra hij stabiel losgelaten is.
  // Zo kunnen dender-flanken bij het loslaten (ook RISING) niet opnieuw togglen.
  static unsigned long lowSince = 0;
  if (digitalRead(PIN_BUTTON) == HIGH) {
    lowSince = millis();
  } else if (!buttonArmed && millis() - lowSince >= RELEASE_MS) {
    buttonArmed = true;
  }

  unsigned long now = micros();
  if (now - previousCycle >= params.cycleTime) {
    previousCycle = now;

    // ---------- begin cyclus ----------
    if (running) {
      // Voorbeeld: LED knippert, elke cyclus wisselt de toestand
      digitalWrite(PIN_LED, !digitalRead(PIN_LED));
    } else {
      digitalWrite(PIN_LED, LOW);
    }
    // ---------- einde cyclus ----------

    calculationTime = micros() - now;

    if (params.debug && millis() - lastDebugMs >= DEBUG_PERIOD_MS) {
      lastDebugMs = millis();
      SerialPort.print(F("run="));      SerialPort.print(running);
      SerialPort.print(F(" calc_us=")); SerialPort.println(calculationTime);
    }
  }
}

// ================================================================
// Externe interrupt: drukknop wisselt start/stop
void onButton() {
  unsigned long nowMs = millis();          // millis() lezen mag in een ISR
  if (buttonArmed && nowMs - lastPressMs > DEBOUNCE_MS) {
    running = !running;
    lastPressMs = nowMs;
    buttonArmed = false;                   // wacht op loslaten (zie loop)
  }
}

// ================================================================
// EEPROM
void loadParams() {
  EEPROM.get(0, params);
  if (params.magic != EEPROM_MAGIC || params.cycleTime < MIN_CYCLE_US) {
    // Lege of ongeldige EEPROM -> standaardwaarden
    params.magic = EEPROM_MAGIC;
    params.cycleTime = 500000UL;   // 0,5 s
    params.debug = false;
    saveParams();
    SerialPort.println(F("EEPROM leeg/ongeldig: standaardwaarden geladen."));
  }
}

void saveParams() {
  EEPROM.put(0, params);   // put() schrijft enkel gewijzigde bytes
}

// ================================================================
// Commando-handlers
void onSet(SerialCommands* sender) {
  char* name  = sender->Next();
  char* value = sender->Next();
  if (name == NULL || value == NULL) {
    sender->GetSerial()->println(F("Gebruik: set cycle <us>"));
    return;
  }

  if (strcmp(name, "cycle") == 0) {
    unsigned long v = strtoul(value, NULL, 10);
    if (v < MIN_CYCLE_US) {
      sender->GetSerial()->print(F("Te klein, minimum = "));
      sender->GetSerial()->println(MIN_CYCLE_US);
      return;
    }
    params.cycleTime = v;
    saveParams();
    sender->GetSerial()->print(F("cycle = "));
    sender->GetSerial()->println(params.cycleTime);
  } else {
    sender->GetSerial()->print(F("Onbekende parameter: "));
    sender->GetSerial()->println(name);
  }
}

void onDebug(SerialCommands* sender) {
  char* arg = sender->Next();
  if (arg != NULL && strcmp(arg, "on") == 0)       params.debug = true;
  else if (arg != NULL && strcmp(arg, "off") == 0) params.debug = false;
  else {
    sender->GetSerial()->println(F("Gebruik: debug on|off"));
    return;
  }
  saveParams();
  sender->GetSerial()->print(F("debug = "));
  sender->GetSerial()->println(params.debug ? F("on") : F("off"));
}

void onStart(SerialCommands* sender) {
  running = true;
  sender->GetSerial()->println(F("Gestart"));
}

void onStop(SerialCommands* sender) {
  running = false;
  sender->GetSerial()->println(F("Gestopt"));
}

void onParams(SerialCommands* sender) {
  printParams();
}

void onUnknown(SerialCommands* sender, const char* cmd) {
  sender->GetSerial()->print(F("Onbekend commando: "));
  sender->GetSerial()->println(cmd);
}

void printParams() {
  SerialPort.print(F("cycle = "));   SerialPort.print(params.cycleTime); SerialPort.println(F(" us"));
  SerialPort.print(F("debug = "));   SerialPort.println(params.debug ? F("on") : F("off"));
  SerialPort.print(F("running = ")); SerialPort.println(running ? F("ja") : F("nee"));
}
