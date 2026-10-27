const int Knop = 13;
const int Led = 12;
bool run = false;
int vorigeKnopState = 1;                        // <-- remembers the previous button state

void setup() {
  pinMode(Knop, INPUT_PULLUP);
  pinMode(Led, OUTPUT);                         // <-- was INPUT
  Serial.begin(115200);
}

void loop() {
  int KnopState = digitalRead(Knop);
  Serial.println("knop:");
  Serial.println(KnopState);                    // <-- println instead of write

  if (KnopState == 0 && vorigeKnopState == 1) { // <-- only react at the moment of pressing
    if (run == false) {
      run = true;
      digitalWrite(Led, HIGH);
    }
    else {                                      // <-- else, so both don't run in the same pass
      run = false;
      digitalWrite(Led, LOW);
    }
    delay(50);                                  // <-- simple debounce
  }

  vorigeKnopState = KnopState;                  // <-- save state for the next pass     // <-- println instead of write
}