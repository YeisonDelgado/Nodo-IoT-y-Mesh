#include <Adafruit_Sensor.h>
#include "painlessMesh.h"
#include <Arduino_JSON.h>
#include "DHT.h"
#include <esp_wifi.h>

#define MESH_PREFIX "new_red123"
#define MESH_PASSWORD "new_red123"
#define MESH_PORT 5555
#define DHTPIN 4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
int nodeNumber = 2;

painlessMesh mesh;
Scheduler userScheduler;
void sendMessage();

Task taskSendMessage(TASK_SECOND * 10, TASK_FOREVER, &sendMessage);

/*
  Function: getReadings
  Description: Reads temperature and humidity from the DHT11 sensor,
  creates a JSON object with those data, and returns it as a string.
  Parameters: None
  Return: A string in JSON format containing the DHT11 sensor readings.
  If an error occurs during reading, returns an empty string.
*/
String getReadings() {
  JSONVar jsonReadings;
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Error reading data from DHT11 sensor");
    return "";  
  }

  jsonReadings["node"] = nodeNumber;
  jsonReadings["temp"] = temperature;
  jsonReadings["hum"] = humidity;
  String readings = JSON.stringify(jsonReadings);
  return readings;
}

/*
  Function: sendMessage
  Description: Sends temperature and humidity readings over the mesh network.
  Also calculates the wake time and sends a synchronization message for deep sleep.
  Parameters: None
  Return: None
*/
void sendMessage() {
  String msg = getReadings();
  if (msg != "") {  
    mesh.sendBroadcast(msg);
    Serial.println("Temperature message sent: " + msg);
    long currentTime = mesh.getNodeTime();
    long wakeTime = currentTime + 5000000; 
    String syncMsg = String(wakeTime);
    mesh.sendBroadcast(syncMsg);
    Serial.println("Synchronization message sent: " + syncMsg);
    goToSleep(5000000); 
  } else {
    Serial.println("No data sent due to an error in sensor reading.");
  }
}

/*
  Function: receivedCallback
  Description: Handles messages received from other nodes.
  If the message contains a "brightness" property, it prints the brightness value.
  Parameters: 
    - from: ID of the node that sent the message.
    - msg: Received message.
  Return: None
*/
void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("Received from node %u message: %s\n", from, msg.c_str());
  JSONVar myObject = JSON.parse(msg.c_str());

  if (myObject.hasOwnProperty("brightness_avg")) {
    int brightness = myObject["brightness_avg"];
    Serial.printf("LED brightness received: %d\n", brightness);
  }
}

/*
  Function: goToSleep
  Description: Puts the device into deep sleep mode to save energy.
  Programs the wake time using esp_sleep_enable_timer_wakeup.
  Parameters: 
    - sleepTime: Wait time before the device wakes up, in microseconds.
  Return: None
*/
void goToSleep(long sleepTime) {
  Serial.println("Entering Deep Sleep mode...");
  esp_sleep_enable_timer_wakeup(sleepTime);  
}

/*
  Function: setup
  Description: Configures the device. Initializes the DHT11 sensor, sets up the mesh network, 
  and adds the task to send messages.
  Parameters: None
  Return: None
*/
void setup() {
  Serial.begin(115200);
  dht.begin();

  mesh.setDebugMsgTypes(ERROR | STARTUP);  
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable(); 
}
/*
  Function: loop
  Description: Main function that keeps the mesh network updated. Called repeatedly.
  Parameters: None
  Return: None
*/
void loop() {
  mesh.update(); 
}


