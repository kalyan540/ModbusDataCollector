#include <ArduinoBleOTA.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "secrets.h"
#include <ModbusWifiAdapter.h>

//#define MQTT_MAX_PACKET_SIZE 2048

// Define the UART and pins for ESP32
#define MODBUS_SERIAL Serial1
#define TX_PIN GPIO_NUM_17
#define RX_PIN GPIO_NUM_16
#define DE_RE_PIN GPIO_NUM_4 // Optional: DE pin for RS485

// Modbus settings
#define MODBUS_BAUD 38400
#define MODBUS_CONFIG SERIAL_8N1
#define MODBUS_UNIT_ID 2

#define DEVICE_NAME "Modbus RTU"
#define MF_NAME "PrahariTechnologies"

#define HW_NAME "ESPRESSIF SYSTEMS ESP32"
#define HW_VER {1, 0, 0}
#define SW_NAME "Blank OTA"
#define SW_VER {0, 0, 0}

WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);

unsigned long lastPublishTime = 0;
unsigned long lastHealthPublishTime = 0;

String deviceid;

// ModbusRTUMaster instance
ModbusWifiAdapter modbus(MODBUS_SERIAL, DE_RE_PIN);

// Modbus data buffers
const uint8_t numHoldingRegisters = 8; // Number of holding registers to read
uint16_t holdingRegisters[numHoldingRegisters]; // Buffer to store holding registers

unsigned long transactionCounter = 0;
unsigned long errorCounter = 0;

const char* errorStrings[] = {
  "success",
  "invalid id",
  "invalid buffer",
  "invalid quantity",
  "response timeout",
  "frame error",
  "crc error",
  "unknown comm error",
  "unexpected id",
  "exception response",
  "unexpected function code",
  "unexpected response length",
  "unexpected byte count",
  "unexpected address",
  "unexpected value",
  "unexpected quantity"
};

void connectToWiFi() {
  Serial.print("Connecting to WiFi ");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");
}

void configureTLS() {
  secureClient.setCACert(ca_cert);

  if (strlen(client_cert) > 0 && strlen(client_key) > 0) {
    secureClient.setCertificate(client_cert);
    secureClient.setPrivateKey(client_key);
  } else {
    Serial.println("Skipping mTLS - using CA cert only");
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("Message on [%s]: ", topic);
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void mqttReconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    if (mqttClient.connect(MQTT_CLIENT_ID)) {
      Serial.println(" connected");
      mqttClient.subscribe(MQTT_TOPIC);
    } else {
      Serial.print(" failed, rc=");
      Serial.print(mqttClient.state());
      delay(3000);
    }
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  ArduinoBleOTA.begin(DEVICE_NAME, InternalStorage, MF_NAME, HW_NAME, HW_VER, SW_NAME, SW_VER);
  connectToWiFi();
  configureTLS();

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  // Initialize Modbus communication
  MODBUS_SERIAL.begin(MODBUS_BAUD, MODBUS_CONFIG, RX_PIN, TX_PIN);
  modbus.begin(MODBUS_BAUD, MODBUS_CONFIG);

  Serial.println("Modbus RTU Master Example for ESP32");
  Serial.println("Reading holding registers...");


}


void loop() {
#ifdef USE_ARDUINO_BLE_LIB
  BLE.poll();
#endif
  ArduinoBleOTA.pull();

  if (!mqttClient.connected()) {
    mqttReconnect();
  }

  mqttClient.loop();
  //Serial.println(client.isConnected());

  unsigned long currentMillis = millis();
  // Publish random values between 10 and 40 every 3 seconds
  if (currentMillis - lastPublishTime >= 2000) {
    // if(client.isWifiConnected()){
    //   ArduinoBleOTA.setWifiStatus("Connected");
    // }else{
    //   ArduinoBleOTA.setWifiStatus("Disconnected");
    // }
    // Serial.println(client.isWifiConnected());
    lastPublishTime = currentMillis;

    // Generate a random value between 10 and 40
    //double randomValue = random(10, 41);  // random(10, 41) gives values between 10 and 40

    uint8_t error = modbus.readHoldingRegisters(MODBUS_UNIT_ID, 0, holdingRegisters, numHoldingRegisters);

    // Print the result of the Modbus transaction
    printLog(MODBUS_UNIT_ID, 3, 0, numHoldingRegisters, error); // Function Code 03 for holding registers 

    // Publish the random value to the MQTT topic
    //String payload = "{\"value1\":" + String(randomValue) + "}";
    String payload;

    if (error == MODBUS_RTU_MASTER_SUCCESS) {
      Serial.print("Holding Registers: ");
      for (uint8_t i = 0; i < numHoldingRegisters; i++) {
          Serial.print(holdingRegisters[i]);
          Serial.print(" ");
      }
      Serial.println();

      // Prepare MQTT payload
      payload = "{";
      for (uint8_t i = 0; i < numHoldingRegisters; i++) {
          payload += "\"value" + String(i + 1) + "\":" + String(holdingRegisters[i]);
          if (i < numHoldingRegisters - 1) {
              payload += ",";
          }
      }
      payload += "}";

      // Print the payload for debugging
      Serial.println("MQTT Payload: " + payload);
      mqttClient.publish("devices/sensor_001/data", payload.c_str());
      mqttClient.publish("test/topic1", payload.c_str());

      // Here you can publish the payload to your MQTT broker
      // mqttClient.publish("your/topic", payload);
    }

    

    // Optionally print the value to Serial for debugging
    //Serial.println("Published random value: ");
    //Serial.print(randomValue);
  }
  // if (currentMillis - lastHealthPublishTime >= 30000) {
  //   if(client.isWifiConnected()){
  //     ArduinoBleOTA.setWifiStatus("Connected");
  //   }else{
  //     ArduinoBleOTA.setWifiStatus("Disconnected");
  //   }
  //   Serial.println(client.isWifiConnected());
  //   lastHealthPublishTime = currentMillis;

  //   String healthpayload = "{\"device_id\":\"" + String(deviceid) + 
  //                      "\",\"status\":\"connected\",\"battery_percentage\":100}";
  //   client.publish("device/health", healthpayload);

  //   // String thresholdpayload = "{\"device_id\":\"" + String(deviceid) + 
  //   //                    "\",\"message\":\"Hello from GasDetector\"}";
  //   // client.publish("device/threshold",thresholdpayload);


  // }

}

void printLog(uint8_t unitId, uint8_t functionCode, uint16_t startingAddress, uint16_t quantity, uint8_t error) {
  transactionCounter++;
  if (error) errorCounter++;
  char string[128];
  sprintf(string, "%ld %ld %02X %02X %04X %04X %s", transactionCounter, errorCounter, unitId, functionCode, startingAddress, quantity, errorStrings[error]);
  Serial.print(string);
  if (error == MODBUS_RTU_MASTER_EXCEPTION_RESPONSE) {
    sprintf(string, ": %02X", modbus.getExceptionResponse());
    Serial.print(string);
  }
  Serial.println();
}