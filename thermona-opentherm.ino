#include <opentherm.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#define WIFI_SSID "SSID"
#define WIFI_PASS "PASSWORD"

HTTPClient https;
BearSSL::WiFiClientSecure client;

// Wemos D1 R2
// Wemos D1 <-> Opentherm D3
#define BOILER_IN 5
// Wemos D5 <-> Opentherm D5
#define BOILER_OUT 14

OpenthermData message;

// 00:11:52.935 -> <- ReadAck 0 0 A
// 00:11:52.935 ->    0 1010
// 					0  0: fault indication [no fault, fault]
// 					1  1: CH mode [CH not active, CH active]
// 					0  2: DHW mode [DHW not active, DHW active]
// 					1  3: Flame status [ flame off, flame on]

// 00:11:58.043 -> <- ReadAck 3 19 0
// 00:11:58.043 ->    11001 0
// 					1   0: DHW present [dhw not present, dhw is present]
// 					0   1: Control type [modulating, on/off]
// 					0   2: Cooling config [cooling not supported, cooling supported]
// 					1   3: DHW config [instantaneous or not-specified, storage tank]
// 					1   4: Master low-off&pump control function [allowed, not allowed]

// 00:12:03.130 -> <- ReadAck 5 0 0
// 00:12:03.130 ->    0 0
// 00:12:03.130 -> 
// 00:12:08.234 -> <- ReadAck 17 16 0
// 00:12:08.234 ->    22.00
// 00:12:08.234 -> 
// 00:12:13.348 -> <- ReadAck 18 1 0
// 00:12:13.348 ->    1.00
// 00:12:13.348 -> 
// 00:12:23.517 -> <- ReadAck 25 18 40
// 00:12:23.517 ->    24.25
// 00:12:23.517 -> 
// 00:12:28.622 -> <- ReadAck 26 41 80
// 00:12:28.622 ->    65.50
// 00:12:28.622 -> 
// 00:12:33.741 -> <- ReadAck 27 7 0
// 00:12:33.741 ->    7.00
// 00:12:33.741 -> 
// 00:12:38.840 -> <- ReadAck 48 46 32
// 00:12:38.840 ->    70 50
// 00:12:38.840 -> 
// 00:12:43.943 -> <- ReadAck 49 28 F
// 00:12:43.943 ->    40 15
// 00:12:43.943 -> 
// 00:12:54.123 -> <- ReadAck 56 41 0
// 00:12:54.123 ->    65.00
// 00:12:54.123 -> 
// 00:12:59.242 -> <- ReadAck 57 28 0
// 00:12:59.242 ->    40.00

int heaterStatuses[] = {
  OT_MSGID_STATUS,
  OT_MSGID_FAULT_FLAGS,
  OT_MSGID_MODULATION_LEVEL,
  OT_MSGID_CH_WATER_PRESSURE,
  OT_MSGID_FEED_TEMP,
  OT_MSGID_DHW_TEMP,
  OT_MSGID_OUTSIDE_TEMP,
  OT_MSGID_DHW_SETPOINT
};
unsigned int currentStatus = 0;

void sendData(int sensor_id, String data) {
  String url = "HOME_ASSISTANT_WEBHOOK";
  url += sensor_id;

  Serial.print(F("[HTTPS] send '"));
  Serial.print(data);
  Serial.print(F("' to "));
  Serial.print(url);
  Serial.print("...");

  if (https.begin(client, url)) { 
    https.addHeader("Content-Type", "application/json");
    
    String body = "{\"data\":";
    body += data;
    body += "}";

    int httpCode = https.POST(body);
    Serial.println(httpCode);

    https.end();
  } else {
    Serial.println(F("[HTTPS] Unable to connect"));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print(F("Connecting to "));
  Serial.print(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    Serial.print(F("."));
  }

  Serial.println();
  Serial.print(F("Connected! IP address: "));
  Serial.println(WiFi.localIP());

  // ignore the SSL certificate
  client.setInsecure();

  pinMode(BOILER_IN, INPUT);
  digitalWrite(BOILER_IN, HIGH); // pull up
  digitalWrite(BOILER_OUT, HIGH);
  pinMode(BOILER_OUT, OUTPUT); // low output = high voltage, high output = low voltage
}

void loop() {
  if (OPENTHERM::isIdle()) {
    message.type = OT_MSGTYPE_READ_DATA;
    message.id = heaterStatuses[currentStatus];
    message.valueHB = 0;
    message.valueLB = 0;
    Serial.print(F("-> ")); 
    OPENTHERM::printToSerial(message); 
    Serial.println();
    OPENTHERM::send(BOILER_OUT, message); // send message to boiler
  }
  else if (OPENTHERM::isSent()) {
    OPENTHERM::listen(BOILER_IN, 800); // wait for boiler to respond
  }
  else if (OPENTHERM::getMessage(message)) { // boiler responded
    OPENTHERM::stop();
    Serial.print(F("<- "));
    OPENTHERM::printToSerial(message);
    Serial.println();
    if (message.id < 10) {
      Serial.print(F("   "));
      Serial.print(message.valueHB, BIN);
      Serial.print(F(" "));
      Serial.print(message.valueLB, BIN);
      Serial.println();
      if (message.id == OT_MSGID_STATUS) {
        sendData(message.id, String(message.valueLB));
      } else {
        sendData(message.id, String(message.valueHB));
      }
    } else {
      Serial.print(F("   "));
      Serial.print(message.f88());
      Serial.println();
      sendData(message.id, String(message.f88()));
    }
    Serial.println();
    currentStatus += 1;
    if (currentStatus >= (sizeof(heaterStatuses) / sizeof(heaterStatuses[0]))) {
      currentStatus = 0;
    }
    delay(1000);
  }
  else if (OPENTHERM::isError()) {
    OPENTHERM::stop();
    Serial.println(F("<- Timeout"));
    Serial.println();
    delay(10000);
  }
}
