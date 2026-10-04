const int IN1 = 5;   // Motor A pin 1
const int IN2 = 6;   // Motor A pin 2
const int IN3 = 9;   // Motor B pin 1
const int IN4 = 10;  // Motor B pin 2

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  Serial.begin(115200);
  Serial.println("DC-Motor test run");
  pinMode(8, OUTPUT);
  digitalWrite(8, HIGH);  // chip wakker maken
}

void loop() {
  // motor A vooruit, half snelheid
  analogWrite(IN1, 128);
  analogWrite(IN2, 0);
  delay(2000);

  // motor A achteruit, vol snelheid
  analogWrite(IN1, 0);
  analogWrite(IN2, 255);
  delay(2000);

  // motor A stop
  analogWrite(IN1, 0);
  analogWrite(IN2, 0);
  delay(1000);

  // motor B vooruit, half snelheid
  analogWrite(IN3, 128);
  analogWrite(IN4, 0);
  delay(2000);

  // motor B achteruit, vol snelheid
  analogWrite(IN3, 0);
  analogWrite(IN4, 255);
  delay(2000);

  // motor B stop
  analogWrite(IN3, 0);
  analogWrite(IN4, 0);
  delay(2000);

  // motoren samen vooruit
  analogWrite(IN1, 255);
  analogWrite(IN2, 0);
  analogWrite(IN3, 255);
  analogWrite(IN4, 0);
  delay(2000);

  // motoren samen achteruit
  analogWrite(In1, 0);
  analogWrite(IN2, 255);
  analogWrite(IN3, 0);
  analogWrite(IN4, 255);
  delay(2000);
}
