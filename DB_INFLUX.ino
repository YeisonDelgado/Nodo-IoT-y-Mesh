/**
 * Secure Write Example code for InfluxDBClient library for Arduino
 * Enter WiFi and InfluxDB parameters below
 *
 * Demonstrates connection to any InfluxDB instance accesible via:
 *  - unsecured http://...
 *  - secure https://... (appropriate certificate is required)
 *  - InfluxDB 2 Cloud at https://cloud2.influxdata.com/ (certificate is preconfigured)
 * Measures signal level of the actually connected WiFi network
 * This example demonstrates time handling, secure connection and measurement writing into InfluxDB
 * Data can be immediately seen in a InfluxDB 2 Cloud UI - measurement wifi_status
 **/

#include "painlessMesh.h"
#include <Arduino_JSON.h>
#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#define MESH_PREFIX "new_red123"
#define MESH_PASSWORD "new_red123"
#define MESH_PORT 5555
#include <WiFiMulti.h>
#define DEVICE "ESP32"
#include <InfluxDbCloud.h>

#define WIFI_SSID "Piso 1"
#define WIFI_PASSWORD "Yeimypiso1."  
#define INFLUXDB_URL "https://eu-central-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "UbAWpx6jgjhLkO0blTQ9JRwcVrL1rANVSNCDMMtN2yc1fi4elcF_wQdW34vKLC6Ew65DVGHxkseWm7N0yk5hkA=="
#define INFLUXDB_ORG "0ed9292e8f0e4c09"
#define INFLUXDB_BUCKET "ESP32"
#define TZ_INFO "UTC-5"


WiFiMulti wifiMulti;
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

painlessMesh mesh;
Scheduler userScheduler;

void readLED();
void sendMessage();

Task taskReadLED(TASK_SECOND * 1, TASK_FOREVER, &readLED);
Task taskSendMessage(TASK_SECOND * 5, TASK_FOREVER, &sendMessage);

Point ledSensor("led_status");
Point meshDataSensor("mesh_data");

int ledPin = 5;  
int nodeNumber = 1;
int brightnessSum = 0;
int readCount = 0;
long wakeTime = 0;
int brightness = 0;  
bool increasing = true;

void connectToWiFi() {
  if (wifiMulti.run() != WL_CONNECTED) {
    WiFi.mode(WIFI_STA);
    wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    while (wifiMulti.run() != WL_CONNECTED) {
      Serial.print(".");
      delay(500);
    }
    Serial.println(" Connected!");
    meshDataSensor.addTag("device", DEVICE);
    meshDataSensor.addTag("SSID", WiFi.SSID());
    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
    if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
    }  
  }
}

void pauseMesh() {
  Serial.println("Pausando red mesh...");
  mesh.stop();  
}

void resumeMesh() {
  Serial.println("Reanudando red mesh...");
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback); 
}

void sendDataToInfluxDB(float temp, float humidity) {
  meshDataSensor.clearFields();
  meshDataSensor.addField("temperature_v", temp);
  meshDataSensor.addField("humidity_v", humidity);
  connectToWiFi();


  if (client.validateConnection()) {
    if (!client.writePoint(meshDataSensor)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());
    } else {
      Serial.println("Mesh data sent to InfluxDB!");
    }
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}


void readLED() {
  int brightness = analogRead(ledPin);  
  brightnessSum += brightness;  
  readCount++;
  Serial.printf("Brightness read: %d\n", brightness);
}

String getLEDData() {
  int avgBrightness = brightnessSum / readCount;
  JSONVar jsonReadings;
  jsonReadings["node"] = nodeNumber;
  jsonReadings["brightness_avg"] = avgBrightness;
  String readings = JSON.stringify(jsonReadings);
  brightnessSum = 0;
  readCount = 0;

  return readings;
}


void sendMessage() {
  if (mesh.getNodeTime() >= wakeTime) {
    String msg = getLEDData();
    mesh.sendBroadcast(msg);
    Serial.println("LED message sent: " + msg);
  } else {
    Serial.println("It's not time to wake up yet, waiting for synchronization...");
  }
}

void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("Received from node %u message: %s\n", from, msg.c_str());

  JSONVar myObject = JSON.parse(msg.c_str());

  if (myObject.hasOwnProperty("temp") && myObject.hasOwnProperty("hum")) {
    float tempData = (double) myObject["temp"];
    float humidityData = (double) myObject["hum"];
    Serial.println("Temperature and humidity received.");
    pauseMesh();
    sendDataToInfluxDB(tempData, humidityData);
  
  // Si el mensaje tiene datos de brillo (LED)
  } else if (myObject.hasOwnProperty("brightness_avg")) {
    Serial.println("Brightness reading received.");
    float brightnessData = (double) myObject["brightness_avg"];
  
    // Enviar el dato de brillo a InfluxDB
    //sendDataToInfluxDB("brightness_avg", String(brightnessData).c_str());
    void resumeMesh();
  
  } else {
    long syncTime = atol(msg.c_str());
    if (syncTime > 0) {
      wakeTime = syncTime;
      Serial.printf("Wake time synchronized: %ld\n", wakeTime);
    } else {
      Serial.println("Synchronization failed. Retrying...");
    }
  }
}

void setup() {
  Serial.begin(115200);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback); 
  userScheduler.addTask(taskReadLED);
  taskReadLED.enable();
  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();
  pinMode(ledPin, OUTPUT); 
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
  mesh.update();
  
  if (increasing) {
    brightness++;
    if (brightness >= 255) increasing = false;
  } else {
    brightness--;
    if (brightness <= 0) increasing = true;
  }

  analogWrite(ledPin, brightness);
  delay(15);
  userScheduler.execute();
}
