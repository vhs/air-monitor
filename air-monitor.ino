#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>


// Define these in the config.h file
//#define WIFI_SSID "yourwifi"
//#define WIFI_PASSWORD "yourpassword"
//#define INFLUX_HOSTNAME "data.example.com"
//#define INFLUX_PORT 8086
//#define INFLUX_PATH "/write?db=<database>&u=<user>&p=<pass>"
#include "config.h"

#define AMBER_LED_PIN 16
#define GREEN_LED_PIN 14

#include "libdcc/influx.h"

// https://www.dfrobot.com/wiki/index.php/PM2.5_laser_dust_sensor_SKU:SEN0177
SoftwareSerial PMSerial(4, 5); // RX, TX
#define LENG 31   //0x42 + 31 bytes equal to 32 bytes

void setup() {
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(AMBER_LED_PIN, OUTPUT);
  digitalWrite(AMBER_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, LOW);

  Serial.begin(115200);

  PMSerial.begin(9600);   
  PMSerial.setTimeout(1500);    

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}


unsigned long lastIteration;
void loop() {

  // Drain the PMSerial so we ensure a fresh reading
  while (PMSerial.available() > 0) {
     PMSerial.read();
    yield();
  }

  delay(100);

  if (millis() < lastIteration + 10000) return;
  lastIteration = millis();

  String sensorBody = String(DEVICE_NAME) + " uptime=" + String(millis()) + "i";

  digitalWrite(GREEN_LED_PIN, HIGH);

  unsigned char buf[LENG];
  int PM2_5Value=0;
  int PM10Value=0;
  if (PMSerial.find(0x42)) {
    PMSerial.readBytes(buf, LENG);
    for (int i=0; i<LENG; i++) {
      Serial.printf("%02x ", buf[i]);
    }
    Serial.println("");

    if (buf[0] == 0x4d) {
      if (checkValue(buf, LENG)) {
        PM2_5Value = ((buf[5]<<8) + buf[6]);   //count PM2.5 value of the air detector module
        PM10Value = ((buf[7]<<8) + buf[8]);    //count PM10 value of the air detector module  
        Serial.printf("PM: %d %d\n", PM2_5Value, PM10Value);
        sensorBody += String(",pm25=" + String(PM2_5Value));
        sensorBody += String(",pm100=" + String(PM10Value));
      } else {
        Serial.println("Checksum failed");
      }
    } else {
      Serial.println("Unexpected header");
    }
  }

  Serial.println(sensorBody);

  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(AMBER_LED_PIN, HIGH);
    delay(1000);
    Serial.println("Connecting to wifi...");
    return;
  }
  digitalWrite(AMBER_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);
  Serial.println("Wifi connected to " + WiFi.SSID() + " IP:" + WiFi.localIP().toString());

  WiFiClient client;
  if (client.connect(INFLUX_HOSTNAME, INFLUX_PORT)) {
    Serial.println(String("Connected to ") + INFLUX_HOSTNAME + ":" + INFLUX_PORT);
    delay(50);

    postRequest(sensorBody, client);

    client.stop();
  } else {
    digitalWrite(AMBER_LED_PIN, HIGH);
  }
  digitalWrite(GREEN_LED_PIN, LOW);
  delay(100);
}

char checkValue(unsigned char *thebuf, char leng) {
  char receiveflag = 0;
  int receiveSum = 0;

  for (int i=0; i<(leng-2); i++) {
    receiveSum=receiveSum+thebuf[i];
  }
  receiveSum=receiveSum + 0x42;

  //check the serial data 
  if (receiveSum == ((thebuf[leng-2]<<8)+thebuf[leng-1])) {
    receiveSum = 0;
    receiveflag = 1;
  }
  return receiveflag;
}