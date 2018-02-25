/**
  Garage Senso

  Checks if the garage door is opened or closed using an infrared distance
  sensor. The result is sent out to the Serial Console and to a MQTT
  broker. Additional this sensor sketch also reads the Temperature and
  Humidity values using a DHT22 sensor.

  The circuit is included as a fritzing file in `circuit/` folder.

  @author Frank Lazzarini
  @version 1.0 24/02/18
*/

#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <DHT_U.h>
#include <wifi_credentials.h>

// Constants
#define VERSION      "1.0"
#define SENSOR_PIN   A0
#define DHT_PIN      5
#define LED_RED      0
#define LED_GREEN    4
#define SLEEPMS      900
#define MQTT_SERVER  "192.168.0.9"
#define MQTT_TOPIC   "garagedoor"
#define MQTT_SENSORS "pong"
#define MQTT_TEMP    "garage_temp"
#define MQTT_HUMI    "garage_humi"
#define DHT_TYPE     DHT22


// Global variables
String       uid = "";
boolean      oldstate;
boolean      state;
float        olddistance;
float        newdistance;
WiFiClient   espClient;
PubSubClient client(espClient);
long         last_msg_sent = 0;
DHT          dht(DHT_PIN, DHT_TYPE);

/**
  Converts local network mac address to string

  @param mac Mac address
  @return mac address as string with `:` columns
*/
String macToStr(const uint8_t* mac) {
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

/**
  Reconnect to MQTT Broker

  @return void
*/
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    // Create a random client ID
    String clientId = "DistanceSenso-";
    clientId += String(random(0xffff), HEX);

    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");

      // Once connected, publish an announcement...
      client.publish(MQTT_TOPIC, "sensor_online");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


/**
  Main Setup, get's the microcontroller ready.

  @return void
*/
void setup() {
  Serial.begin(9600);
  delay(10);

  Serial.print("DistanceSenso v");
  Serial.print(VERSION);
  Serial.println(" booting up");
  Serial.print("[WiFi] Connecting to ");
  Serial.print(WLAN);
  Serial.print(" ");

  WiFi.begin(WLAN, WPA);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  Serial.print("[WiFi] WiFi connected: ");
  Serial.println(WiFi.localIP());

  // Get Unique device Id
  byte mac[6];
  WiFi.macAddress(mac);
  uid += macToStr(mac);
  Serial.print("[WiFi] Mac address: ");
  Serial.println(uid);

  // PIN Setup
  pinMode(SENSOR_PIN, INPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  // MQTT Stuff
  client.setServer(MQTT_SERVER, 1883);

  // State values initialized
  oldstate = true;
  state = true;
}

/**
  Main Loop

  @return void
*/
void loop() {
  // Check if MQTT is connected
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Send MQTT Ping And read DHT once every 10 seconds
  long now = millis();
  if (now - last_msg_sent > 10000) {
    last_msg_sent = now;    
    client.publish(MQTT_SENSORS, "ping");

    float humi = dht.readHumidity();
    float temp = dht.readTemperature();

    char hum[15];
    char tmp[15];
    dtostrf(humi, 5, 2, hum);
    dtostrf(temp, 5, 2, tmp);

    client.publish(MQTT_TEMP, tmp);
    client.publish(MQTT_HUMI, hum);
    Serial.print("Temperature ");
    Serial.println(tmp);
    Serial.print("Humidity ");
    Serial.println(hum);
  }

  // Read distance
  float sensor_read = analogRead(SENSOR_PIN);
  float cm = 10650.08 * pow(sensor_read, -0.935) - 10;
  float newdistance = roundf(cm);

  if (olddistance != newdistance) {
    Serial.print("New distance ");
    Serial.println(newdistance);
  }

  if (newdistance >= 20) {
    state = true;
  } else {
    state = false;
  }

  // Only Print if Status Changed
  if (state != oldstate) {
    if (state == true) {
      Serial.println("Sesame is Open");
      client.publish(MQTT_TOPIC, "garage_opened");
      digitalWrite(LED_GREEN, HIGH);
      digitalWrite(LED_RED, LOW);
    } else {
      Serial.println("Sesame is CLOSED");
      client.publish(MQTT_TOPIC, "garage_closed");
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_RED, HIGH);
    }
  }

  // Keep states of previous run
  oldstate = state;
  olddistance = newdistance;

  // Give it some rest
  delay(SLEEPMS);
}
