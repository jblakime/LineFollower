#include <SoftwareSerial.h>

SoftwareSerial bt(12, 11);   // RX D12 <- HC-08 TXD, TX D11 -> HC-08 RXD (via divider)
const int Led = 13;          // built-in LED on the Nano

void setup() {
  pinMode(Led, OUTPUT);
  Serial.begin(9600);
  bt.begin(9600);            // HC-08 default baud rate

  Serial.println("HC-08 test ready.");
  Serial.println("Not connected: type AT in the Serial Monitor, expect OK.");
  Serial.println("Connected from phone: send 1 = LED on, 0 = LED off.");
}

void loop() {
  // phone -> Arduino
  if (bt.available()) {
    char c = bt.read();
    Serial.write(c);                     // show everything received

    if (c == '1') {
      digitalWrite(Led, HIGH);
      bt.println("LED ON");
    }
    else if (c == '0') {
      digitalWrite(Led, LOW);
      bt.println("LED OFF");
    }
  }

  // Serial Monitor -> HC-08 (AT commands or messages to the phone)
  if (Serial.available()) {
    bt.write(Serial.read());
  }
}