#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <SparkFunBME280.h>

// wifi configuration
const char* wifi_ssid = "Router";
const char* wifi_password = "*****";
const char* mqtt_user = "helium";
const char* mqtt_password = "*****";
const char* mqtt_server = "iot-kurs-mqtt.protohaus.org";

// Moisture Pins
const int moist_pin1 = 34;
const int moist_pin2 = 35;
const int moist_pin3 = 32;

// BME280 Pins
BME280 aQ_Sensor; // Air Quality Sensor
const int sda_pin = 27;
const int scl_pin = 26;

// Photoresistor
const int photo_pin1 = 33;

const char* le_root_ca= \
     "***Certificate***";

WiFiClientSecure wifi_client;
PubSubClient mqtt_client(wifi_client);

unsigned long last_msg_ms = 0;
const unsigned int msg_buffer_size = 50;
char msg[msg_buffer_size];
int counter = 0;

const int built_in_led_pin = 2;

/// Connect to the WiFi and block until connected
void setup_wifi() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  wifi_client.setCACert(le_root_ca);
}

/// Connect to the MQTT broker and block until connected
void reconnect() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String client_id = "ESP32Client-";
    client_id += WiFi.macAddress();
    // Attempt to connect
    if (mqtt_client.connect(client_id.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("connected");

      
    } else {
      Serial.printf("failed, rc=%d try again in 5 seconds", mqtt_client.state());
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(built_in_led_pin, OUTPUT);
  setup_wifi();
  mqtt_client.setServer(mqtt_server, 8883);


  // Moisture Pins Setup
  pinMode(moist_pin1, INPUT);
  pinMode(moist_pin2, INPUT);
  pinMode(moist_pin3, INPUT);

  // Air Quality Sensor
  Wire.begin(sda_pin, scl_pin);
  aQ_Sensor.setI2CAddress(0x76);
  aQ_Sensor.beginI2C(Wire);
}

void loop() {
  if (!mqtt_client.connected()) {
    reconnect();
  }
  mqtt_client.loop();

  unsigned long now = millis();
  if (labs(now - last_msg_ms) > 2000) {
    last_msg_ms = now;

    // air quality
    mqtt_client.publish("helium/air/temp", String(aQ_Sensor.readTempC()).c_str());

    // Moisture 
    mqtt_client.publish("helium/moisture/plant1/moisture1", String(100 - (analogRead(moist_pin1)*100/4095)).c_str());
    mqtt_client.publish("helium/moisture/plant1/moisture2", String(100 - (analogRead(moist_pin2)*100/4095)).c_str());
    mqtt_client.publish("helium/moisture/plant2", String(100 - (analogRead(moist_pin3)*100/4095)).c_str());

    // Photo resistor
    mqtt_client.publish("helium/photo/resistor1", String(analogRead(photo_pin1) * 100 / 4095).c_str());
      }
}
