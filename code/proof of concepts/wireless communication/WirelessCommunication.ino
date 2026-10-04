#include <SoftwareSerial.h>

const int pinBtRx = 2;          // Arduino RX  <- HC-08 TXD
const int pinBtTx = 3;          // Arduino TX  -> HC-08 RXD (via spanningsdeler)
const int pinLed  = LED_BUILTIN;

SoftwareSerial bt(pinBtRx, pinBtTx);

String ontvangen = "";
unsigned long laatsteZending = 0;
const unsigned long zendInterval = 1000;   // ms

void setup() {
  Serial.begin(115200);
  bt.begin(9600);               // standaard baudrate van de HC-08
  pinMode(pinLed, OUTPUT);

  Serial.println("HC-08 duplex test gestart");
  bt.println("Arduino klaar. Commando's: LED ON, LED OFF, STATUS");
}

void loop() {
  // ---------- RX: smartphone -> Arduino ----------
  while (bt.available()) {
    char c = bt.read();
    if (c == '\n' || c == '\r') {
      if (ontvangen.length() > 0) {
        verwerkCommando(ontvangen);
        ontvangen = "";
      }
    } else {
      ontvangen += c;
    }
  }

  // ---------- TX: Arduino -> smartphone (gelijktijdig, niet-blokkerend) ----------
  if (millis() - laatsteZending >= zendInterval) {
    laatsteZending = millis();
    bt.print("A0=");
    bt.println(analogRead(A0));
  }
}

void verwerkCommando(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  Serial.print("Ontvangen: ");
  Serial.println(cmd);

  if (cmd == "LED ON") {
    digitalWrite(pinLed, HIGH);
    bt.println("OK: LED aan");
  } else if (cmd == "LED OFF") {
    digitalWrite(pinLed, LOW);
    bt.println("OK: LED uit");
  } else if (cmd == "STATUS") {
    bt.print("LED=");
    bt.println(digitalRead(pinLed) ? "AAN" : "UIT");
  } else {
    bt.print("Onbekend commando: ");
    bt.println(cmd);
  }
}