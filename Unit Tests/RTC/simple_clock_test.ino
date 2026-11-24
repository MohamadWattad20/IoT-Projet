
#include <Wire.h>
#include "RTClib.h"

RTC_DS1307 rtc;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  if (!rtc.begin()) {
    Serial.println("DS1307 not found!");
    while (1);
  }

  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  Serial.println("RTC time successfully set!");
}

void loop() {
  DateTime now = rtc.now();

  Serial.print(now.year());
  Serial.print('/');
  Serial.print(now.month());
  Serial.print('/');
  Serial.print(now.day());

  Serial.print("  ");

  Serial.print(now.hour());
  Serial.print(':');
  Serial.print(now.minute());
  Serial.print(':');
  Serial.println(now.second());

  delay(1000);
}
