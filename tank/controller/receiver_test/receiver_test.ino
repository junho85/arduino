#include <SoftwareSerial.h>

SoftwareSerial BTSerial(2, 3); //Connect HC-06. Use your (TX, RX) settings
//SoftwareSerial BTSerial(10, 11); //Connect HC-06. Use your (TX, RX) settings

int led_f = 6;
int led_b = 7;
int led_l = 8;
int led_r = 9;

void setup()  
{
  Serial.begin(9600);
  Serial.println("Hello Receiver Test!");

  BTSerial.begin(9600);  // set the data rate for the BT port
  //BTSerial.begin(115200);
}

void loop()
{
  byte data;
  // BT –> Data –> Serial
  if (BTSerial.available()) {
    data = BTSerial.read();
    Serial.println(data);


    switch (data) {
      case 'F':
        Serial.println("FORWARD");
        digitalWrite(led_f, HIGH);
        digitalWrite(led_b, LOW);
        digitalWrite(led_l, LOW);
        digitalWrite(led_r, LOW);
        break;
      case 'B':
        digitalWrite(led_f, LOW);
        digitalWrite(led_b, HIGH);
        digitalWrite(led_l, LOW);
        digitalWrite(led_r, LOW);
        break;
      case 'L':
        digitalWrite(led_f, LOW);
        digitalWrite(led_b, LOW);
        digitalWrite(led_l, HIGH);
        digitalWrite(led_r, LOW);
        break;
      case 'R':
        digitalWrite(led_f, LOW);
        digitalWrite(led_b, LOW);
        digitalWrite(led_l, LOW);
        digitalWrite(led_r, HIGH);
        break;
      default:
        digitalWrite(led_f, LOW);
        digitalWrite(led_b, LOW);
        digitalWrite(led_l, LOW);
        digitalWrite(led_r, LOW);
        break;
    }

  }
}

