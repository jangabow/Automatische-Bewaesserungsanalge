#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Bounce2.h>


// Thresholds
const int pump_threshold = 30;
const int light_threshold = 60;
const int temp_threshold = 27;

// Wifi-Configuration
const char* wifi_ssid = "Router";
const char* wifi_password = "*****";
const char* mqtt_user = "helium";
const char* mqtt_password = "*****";
const char* mqtt_server = "iot-kurs-mqtt.protohaus.org";

// relays
const int relay1 = 27;
const int relay2 = 26;

// buttons to start the pumps
const int button1 = 32;
const int button2 = 33;
Bounce debouncer1 = Bounce();
Bounce debouncer2 = Bounce();

// LED's
const int ledYellow = 14;
const int ledGreen = 12;

//water quality
const int waterQSensor = 35;
#define Offset -8.6875           //deviation compensate
#define samplingInterval 20
#define printInterval 1000
#define ArrayLenth  40    //times of collection
int pHArray[ArrayLenth];   //Store the average value of the sensor feedback
int pHArrayIndex=0;

// define optimal circumstances for watering
bool light_ok = false;
bool moisture1_ok = false;
bool moisture2_ok = false;
bool temp_ok = false;


const char* le_root_ca= \
     "***Certififace***";

WiFiClientSecure wifi_client;
PubSubClient mqtt_client(wifi_client);

unsigned long last_msg_ms = 0;
unsigned long last_watering1 = -60000;
unsigned long last_watering2 = -60000;
const unsigned int msg_buffer_size = 50;
char msg[msg_buffer_size];

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

/// Handle the received message
void callback(char* topic, byte* payload, unsigned int length) {
  // Switch on the LED if a 1 was received as first character
  String strTopic = String(topic).c_str(); 

  if (strTopic.equals("helium/photo/resistor1")) {
    Serial.print("Licht: ");
    Serial.println(strtod((char*)payload, NULL));
    if (strtod((char*)payload, NULL) < light_threshold) {
      Serial.println("light-GUT");
      light_ok = true;
    } else {
      light_ok = false;
    }
  }

  if (strTopic.equals("helium/moisture/plant1/moisture1")) {
    if (strtod((char*)payload, NULL) < pump_threshold) {
      Serial.println("plant1-mousture1 GUT");
      moisture1_ok = true;
    } else {
      moisture1_ok = false;
    }
  }

  if (strTopic.equals("helium/moisture/plant1/moisture2")) {
    if (!(strtod((char*)payload, NULL) < pump_threshold) && moisture1_ok == true) {
      moisture1_ok = false;
    } else if(moisture1_ok) {
      Serial.println("plant1-mousture2 GUT");
    }
  }

  if (strTopic.equals("helium/moisture/plant2")) {
    if (strtod((char*)payload, NULL) < pump_threshold) {
      Serial.println("plant2- GUT");
      moisture2_ok = true;
    } else {
      moisture2_ok = false;
    }
  }

  if (strTopic.equals("helium/air/temp")) {
    if (strtod((char*)payload, NULL) < temp_threshold) {
      Serial.println("temp GUT");
      temp_ok = true;
    } else {
      temp_ok = false;
    }
  }
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
      // Once connected, publish an announcement...
      mqtt_client.subscribe("helium/moisture/+");
      mqtt_client.subscribe("helium/moisture/plant1/+");
      mqtt_client.subscribe("helium/photo/+");
      mqtt_client.subscribe("helium/air/+");
      
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
  mqtt_client.setCallback(callback);

  // relay-setup
  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);

  // button-setup
  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);
  debouncer1.attach(button1);
  debouncer2.attach(button2);
  debouncer1.interval(5);
  debouncer2.interval(5);

  // LED's
  pinMode(ledYellow, OUTPUT);
  pinMode(ledGreen, OUTPUT);

  // Water quality
  pinMode(waterQSensor, INPUT);
}

double avergearray(int* arr, int number){
  int i;
  int max,min;
  double avg;
  long amount=0;
  if(number<=0){
    Serial.println("Error number for the array to avraging!/n");
    return 0;
  }
  if(number<5){   //less than 5, calculated directly statistics
    for(i=0;i<number;i++){
      amount+=arr[i];
    }
    avg = amount/number;
    return avg;
  }else{
    if(arr[0]<arr[1]){
      min = arr[0];max=arr[1];
    }
    else{
      min=arr[1];max=arr[0];
    }
    for(i=2;i<number;i++){
      if(arr[i]<min){
        amount+=min;        //arr<min
        min=arr[i];
      }else {
        if(arr[i]>max){
          amount+=max;    //arr>max
          max=arr[i];
        }else{
          amount+=arr[i]; //min<=arr<=max
        }
      }//if
    }//for
    avg = (double)amount/(number-2);
  }//if
  return avg;
}

void pump1() {
  unsigned long now = millis();
  if (labs(now - last_watering1) > 60000) {
    last_watering1 = now;

    digitalWrite(ledGreen, HIGH);
    digitalWrite(relay1, HIGH);
    delay(5000);
    digitalWrite(ledGreen, LOW);
    digitalWrite(relay1, LOW);
  }
}

void pump2() {
  unsigned long now = millis();
  if (labs(now - last_watering2) > 60000) {
    last_watering2 = now;

    digitalWrite(ledGreen, HIGH);
    digitalWrite(relay2, HIGH);
    delay(5000);
    digitalWrite(ledGreen, LOW);
    digitalWrite(relay2, LOW);
  }
}

void loop() {
  if (!mqtt_client.connected()) {
    reconnect();
  }
  mqtt_client.loop();

  unsigned long now = millis();

  // calculate water quality
  static float pHValue,voltage;
  pHArray[pHArrayIndex++]=analogRead(waterQSensor);
  if(pHArrayIndex==ArrayLenth)pHArrayIndex=0;
  voltage = avergearray(pHArray, ArrayLenth)*5.0/1024;
  pHValue = 1.375*voltage+Offset;

  // button actions
    debouncer1.update();
  if(debouncer1.fell()) {
    Serial.println("");
    Serial.println("Button1 pressed");
    digitalWrite(ledGreen, HIGH);
    digitalWrite(relay1, HIGH);
    Serial.println("Pumpe 1 ON");
    Serial.println("");
  }
  if(debouncer1.rose()) {
    digitalWrite(ledGreen, LOW);
    digitalWrite(relay1, LOW);
  }

  debouncer2.update();
  if(debouncer2.fell()) {
    Serial.println("");
    Serial.println("Button2 pressed");
    digitalWrite(ledGreen, HIGH);
    digitalWrite(relay2, HIGH);
    Serial.println("Pumpe 2 ON");
    Serial.println("");
  }
  if(debouncer2.rose()) {
    digitalWrite(ledGreen, LOW);
    digitalWrite(relay2, LOW);
  }


  if (labs(now - last_msg_ms) > 1000) {
    last_msg_ms = now;

    // water output
    mqtt_client.publish("helium/waterQuality", String(pHValue,2).c_str());
  }

  if(light_ok && moisture1_ok && temp_ok) {
    pump2();
  } 

  if(light_ok && moisture2_ok && temp_ok) {
    pump1();
  } 

  // checking for bad ph
  if(pHValue < 5.5 || pHValue > 7.5 || !light_ok || !temp_ok) {
    digitalWrite(ledYellow, HIGH);
  } else {
    digitalWrite(ledYellow, LOW);
  }
}
