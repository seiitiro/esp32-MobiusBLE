/*!
 * Discover Mobius Devices
 *
 * This example shows some debugging and discovering methods for Mobius devices.
 * First this will scans for BLE enabled Mobius devices (expecting just one). Once
 * a device is discovered this will attempt connecting to the device. After 
 * successfully connecting this will:
 *   1. read the current "scene" ID
 *   2. start the default feed "scene"
 *   3. read the feed "scene" ID (should be 1)
 *   4. start the normal schedule
 *   5. read the current "scene" ID (should be 0)
 * 
 * 
 * This example code is released into the public domain.
 * 
 * Work in progress
 */
#include <esp_log.h>
#include "./src/ESP32_MobiusBLE.h"
#include "./src/ArduinoSerialDeviceEventListener.h"
#include <AntiDelay.h>
#include <string>
#include "MobiusSerialDecoder.h"

// Create an AntiDelay object with initial 0 millis (run on first execution)
AntiDelay scanMobius(0);
bool pendingScene = false;
std::string pendingSceneDetail;
int newSceneId;

// Define a device buffer to hold found Mobius devices
MobiusDevice deviceBuffer[20];
int deviceCount = 0;

/*!
 * Main Setup method
 */
void setup() {
  // connect the serial port for logs
  Serial.begin(115200);
  while (!Serial);

  // Initialize the library with a useful event listener
  MobiusDevice::init(new ArduinoSerialDeviceEventListener());

  Serial.println("Setup run");
}

/*!
 * Main Loop method
 */
void loop() {

	if (scanMobius) {
    //Run Mobius routine every x minutes defined below in [float minutes = ]
    if (!scanMobius.isRunning()){
      //Now that the scan has started on boot, set the timer so re-scan runs every 2 minutes
      float minutes = 5;
      scanMobius.setInterval(minutes*60000);
    }

    // create buffer to store the Mobius devices
    MobiusDevice device = deviceBuffer[0];

    int scanDuration = 10; // in seconds
    while (!deviceCount) {
      //Scan until at least one device is returned
      deviceCount = MobiusDevice::scanForMobiusDevices(scanDuration, deviceBuffer,10);
    }

    Serial.print("Devices Found: "); 
    Serial.println(deviceCount);

    for (int i = 0; i < deviceCount; i++) {
      // Loop through each device found, autoconfigure home assistant with the device, and update the current scene of the device
      device = deviceBuffer[i];
      Serial.println("\n========================  N E W   D E V I C E  ========================\n");

      Serial.print("Device is: "); 
      Serial.println(device._device->toString().c_str());

      // Get device address
      std::string addressString = device._device->getAddress().toString();

      Serial.print("Device address: ");
      Serial.println(addressString.c_str());
      
      int tries = 1;
      //loop until connected or exit after 10 tries
      while(tries<=10){
        if (device.connect()) {
          // Connect and get current scene
          ESP_LOGD(LOG_TAG, "Successfully connected\n");

          //Get Current scene
          uint16_t sceneId = device.getCurrentScene();
          ESP_LOGD(LOG_TAG, "Current scene:%d\n", sceneId);

          //Convert scene from int to friendly MQTT text
          char currScene[2];
          sprintf(currScene, "%u", sceneId);
          
          ESP_LOGD(LOG_TAG, "Current scene string:%s\n", sceneString);

          char* modelNumber = getModelName(device.getSerialNumber());
          const char* fwRev = device.getFWRev().c_str();
          const char* Manufa = device.getManufName().c_str();
          const char* serialNumber = device.getSerialNumber().c_str();

          Serial.print("Model Number: ");
          Serial.println(modelNumber);

          Serial.print("Serial #: ");
          Serial.println(serialNumber);

          Serial.print("FW Revision: ");
          Serial.println(fwRev);

          Serial.print("Manufacturer: ");
          Serial.println(Manufa);

          std::string jsonTest = createDeviceJson((char*) serialNumber, (char*) Manufa, (char*) modelNumber, (char*) fwRev );

          Serial.println("\n======== JSON DATA ===========\n");
          Serial.println(jsonTest.c_str());

					// delaying without sleeping
					unsigned long startMillis = millis();
					while (1000 > (millis() - startMillis)) {}

          device.disconnect();

          //If connection completed, break the loop
          break;
        }
        else {
          tries++;
        }
      }

      if (pendingScene) 
        //Clearing pending scene after looping thru all Mobius Devices
        pendingScene = false;

      //Print error message if didn't connect after 10 tries
      if (tries>9) {
        ESP_LOGE(LOG_TAG, "ERROR: Failed to connect to device");
      }
    }
    Serial.print("DONE...... ");

    ESP_LOGI(LOG_TAG, "Waiting for next run in 2 minutes....");
  }
}

/*
 * Function to create the json with device information (bottom part of HA MQTT config message)
*/
std::string createDeviceJson(char* serialNum, char* manufac, char* modelNum, char* fwRevision){

  JsonDocument jsonDoc;

  jsonDoc["name"] = nullptr;
  jsonDoc["unique_id"] = serialNum;
  jsonDoc["icon"] = "mdi:pump";

  char stateTopic[512];
  sprintf(stateTopic, "homeassistant/sensor/mobius/%s/scene", serialNum);
  jsonDoc["state_topic"] = stateTopic;

  jsonDoc["force_update"] = "true";

  JsonObject jsonDevice = jsonDoc["Device"].to<JsonObject>();
  jsonDevice["identifiers"][0] = serialNum;
  jsonDevice["name"] = serialNum;
  jsonDevice["serial_number"] = serialNum;
  jsonDevice["fw_version"] = fwRevision;

  if (sizeof(modelNum) > 1)
    jsonDevice["model"] = modelNum;
  else
    jsonDevice["model"] = "Mobius";

  if (sizeof(manufac)> 1)
    jsonDevice["manufacturer"] = manufac;
  else
    jsonDevice["manufacturer"] = "Ecotech";

  std::string output;

  jsonDoc.shrinkToFit();  // optional

  serializeJson(jsonDoc, output);

  return output;
}

