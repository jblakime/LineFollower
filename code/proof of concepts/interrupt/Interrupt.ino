const int Knop = 13;
const int Led = 12;
bool run = false;

void setup() {
  pinMode(Knop, INPUT);
  pinMode(Led, INPUT);

}

void loop() {
  int KnopState = digitalRead(Knop);

  if (KnopState == 1 && run == false) {
    run = true;
    digitalWrite(Led, HIGH);
  }

  if (KnopState == 1 && run == true) {
    run = false;
    digitalWrite(Led, LOW);
  }

}
