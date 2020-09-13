#include<ESP8266WiFi.h>
#include"PubSubClient.h" //Version 2.8
#include<ArduinoOTA.h>
#include<Wire.h>

#define _WLAN_HOST "WemosGarage"
#define _WLAN_SSID "MySSID"
#define _WLAN_PASS "MyPassword"

#define _MQTT_BROKER "MyMQTTIP"
#define _MQTT_PORT 1883
#define _MQTT_USER "MyMQTTUser"
#define _MQTT_PASS "MyMQTTPassword"

#define DEBOUNCE_TIME       0.3
#define SAMPLE_FREQUENCY    10
#define MAX_DEBOUNCE        (DEBOUNCE_TIME * SAMPLE_FREQUENCY)

const char* ssid = _WLAN_SSID;
const char* password = _WLAN_PASS;

WiFiClient wlanClient;
PubSubClient mqttClient(wlanClient);

//D0 = pin 16
//D1 = pin 5
//D2 = pin 4
//D3 = pin 0
//D4 = pin 2
//D5 = pin 14
//D6 = pin 12
//D7 = pin 13
//D8 = pin 15
//TX = pin 1
//RX = pin 3

long lastMsg = 0;
char msg[50];
int switchState = -1;
int integratorSwitchState = 0;
int previousSwitchState = -1;
const int switchPin = 4;
const int relayPin = 5;
unsigned long sampleTimer;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.hostname("WemosGarage");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("GarageDoor", _MQTT_USER, _MQTT_PASS, "security/garage/LWT", 1, 1, "Disconnected")) {
      Serial.println("Connected");
      mqttClient.publish("security/garage/LWT", "Connected", true);
      mqttClient.subscribe("security/garage/set", 1);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(", trying again in 5 seconds");
      delay(5000);
    }
  }
  //force an update:
  publishState();
}

void setup_OTA()
{
  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

void setup() {

  pinMode(switchPin, INPUT_PULLUP);
  pinMode(relayPin, OUTPUT);

  Serial.begin(9600);
  setup_wifi();
  setup_OTA();
  mqttClient.setServer(_MQTT_BROKER, _MQTT_PORT);
  mqttClient.setCallback(callback);

}

void loop() {

  int rc = -1;
  unsigned long currentMillis = millis();
  int input;

  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

  ArduinoOTA.handle();

  if ((currentMillis - sampleTimer) > 100UL) {
    for (int pin = 0; pin <= 8; pin++) {
      //begin debounce
      input = digitalRead(switchPin);
      if (input == 0) {
        if (integratorSwitchState > 0) {
          integratorSwitchState--;
        }
      }
      else if (integratorSwitchState < MAX_DEBOUNCE) {
        integratorSwitchState++;
      }

      if (integratorSwitchState == 0) {
        switchState = 0;
      }
      else if (integratorSwitchState >= MAX_DEBOUNCE) {
        switchState = 1;
        integratorSwitchState = MAX_DEBOUNCE;
      }
      //end debounce
      if (switchState != previousSwitchState) {
        previousSwitchState = switchState;
        publishState();
      }
    }
    sampleTimer = currentMillis;
  }

}

void publishState()
{
  int rc = -1;
  String zone = "security/garage";
  String message = "{\"value\":" + String(switchState) + ", \"zoneName\":\"Garage Door\"}";

  rc = mqttClient.publish(zone.c_str(), message.c_str(), true);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if ((char)payload[0] == '1' && (strcmp(topic, "security/garage/set") == 0)) {
    Serial.println("Changing Garage State");
    digitalWrite(relayPin, HIGH);
    delay(500);
    digitalWrite(relayPin, LOW);
  }

}
