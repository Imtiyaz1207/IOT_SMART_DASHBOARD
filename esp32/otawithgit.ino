#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h> // new 18
#define FIRMWARE_VERSION "2"
Preferences preferences;
// final working esp32 code 16-06-26 4pm

// ================= WIFI =================

String wifiSSID = "";
String wifiPassword = "";

bool wifiConfigured = false;
bool wifiConnectedOnce = false;
bool bleModeStarted = false;

const char* mqtt_server = "broker.hivemq.com";
String VERSION_URL =
"https://imtiyaz1207.github.io/OTA_Update/version.json";

String OTA_URL = "";
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"

#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-cba987654321"

String receivedData = "";



// ================= MQTT =================

WiFiClient espClient;

PubSubClient client(espClient);

String ACTION_TOPIC =
"iot/smarttank/actions";

String STATUS_TOPIC =
"iot/smarttank/status";

String ACK_TOPIC =
"iot/smarttank/system_ack";


// NEW 15 NYT===================================================NEW-NYT=======================================================
#define LIGHT_LED 13
#define FAN_LED   12
#define PUMP_LED  14
#define WIFI_RESET_BUTTON 27  // wifi reset
// ================= VARIABLES =================

String currentCommand =
"Neutral";

unsigned long lastPublish = 0;


 // new 10/06
String lastCommandId = "";

void executeCommand(
    String cmd,
    String commandId
){
   
//=================================================================NEW-15-NYT==========================================================================

if(cmd == "Left_light_on")          // Light ON
{
    digitalWrite(LIGHT_LED, HIGH);
}

else if(cmd == "Left_light_off")     // Light OFF
{
    digitalWrite(LIGHT_LED, LOW);
}

else if(cmd == "Left_fan_on")   // Fan ON
{
    digitalWrite(FAN_LED, HIGH);
}

else if(cmd == "Left_fan_off")     // Fan OFF
{
    digitalWrite(FAN_LED, LOW);
}

else if(cmd == "Left_pump_on")     // Pump ON
{
    digitalWrite(PUMP_LED, HIGH);
}

else if(cmd == "Left_pump_off")    // Pump OFF
{
    digitalWrite(PUMP_LED, LOW);
}

//change   new 17



StaticJsonDocument<300> ackDoc;


// Device information
ackDoc["device_id"] = "ESP32_001";


// Command details
ackDoc["command_id"] = commandId;
ackDoc["command"] = cmd;


// Result
ackDoc["result"] = "SUCCESS";


// Current device state
ackDoc["light"] =
digitalRead(LIGHT_LED) ? "ON" : "OFF";


ackDoc["fan"] =
digitalRead(FAN_LED) ? "ON" : "OFF";


ackDoc["pump"] =
digitalRead(PUMP_LED) ? "ON" : "OFF";


// Device time (uptime)
unsigned long seconds =
millis()/1000;

String deviceTime =
String(seconds/3600) + ":" +
String((seconds%3600)/60) + ":" +
String(seconds%60);


ackDoc["device_time"] = deviceTime;

ackDoc["firmware"] =
FIRMWARE_VERSION;

char ackBuffer[300];


serializeJson(
    ackDoc,
    ackBuffer
);


client.publish(
    ACK_TOPIC.c_str(),
    ackBuffer
);
}
// NEW 

class WiFiCallback : public BLECharacteristicCallbacks {

  void onWrite(BLECharacteristic *pCharacteristic) {

    String chunk = String(pCharacteristic->getValue());

    if (chunk.length() > 0) {

      receivedData += chunk;

      int commaIndex =
      receivedData.indexOf(',');

      if (commaIndex > 0) {

        wifiSSID =
        receivedData.substring(
          0,
          commaIndex
        );

        wifiPassword =
        receivedData.substring(
          commaIndex + 1
        );

        wifiSSID.trim();
        wifiPassword.trim();

        Serial.println();
        Serial.println("WiFi Data Received");

        Serial.print("SSID: ");
        Serial.println(wifiSSID);

        Serial.print("PASSWORD: ");
        Serial.println(wifiPassword);

        wifiConfigured = true;
        wifiConnectedOnce = false;
        bleModeStarted = false;
        preferences.begin("wifi", false);

        preferences.putString("ssid", wifiSSID);
        preferences.putString("pass", wifiPassword);

        preferences.end();

        Serial.println("WiFi Credentials Saved");
        
        WiFi.disconnect(true);
        receivedData = "";
      }
    }
  }
};


// ================= WIFI new preference funtion=================
void loadWiFiCredentials()
{
    preferences.begin("wifi", true);

    wifiSSID = preferences.getString("ssid", "");
    wifiPassword = preferences.getString("pass", "");

    preferences.end();

    if(wifiSSID.length() > 0)
    {
        wifiConfigured = true;

        Serial.println("Saved WiFi Credentials Found");
        Serial.print("SSID: ");
        Serial.println(wifiSSID);
    }
}

void setup_wifi()
{
    Serial.println("Connecting WiFi...");

    WiFi.begin(
        wifiSSID.c_str(),
        wifiPassword.c_str()
    );

    int attempts = 0;

    while(
        WiFi.status() != WL_CONNECTED &&
        attempts < 20
    )
    {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if(WiFi.status() == WL_CONNECTED)
    {
        Serial.println();
        Serial.println("Connected");
        Serial.println(WiFi.localIP());
    }
    else
{
    Serial.println();
    Serial.println("WiFi Failed");

    wifiConfigured = false;
    wifiConnectedOnce = false;

    setupBLE();
}
}
void checkForUpdates()
{
    HTTPClient http;

    http.begin(VERSION_URL);

    int httpCode = http.GET();

    if(httpCode == 200)
    {
        String payload = http.getString();

        StaticJsonDocument<256> doc;

        deserializeJson(doc, payload);

        String latestVersion =
            doc["version"].as<String>();

        String firmwareURL =
            doc["firmware_url"].as<String>();

        Serial.print("Current Version: ");
        Serial.println(FIRMWARE_VERSION);

        Serial.print("Latest Version: ");
        Serial.println(latestVersion);

        if(latestVersion != FIRMWARE_VERSION)
        {
            Serial.println("New Firmware Found");
            
            Serial.print("Firmware URL: ");
            Serial.println(firmwareURL);


            OTA_URL = firmwareURL;

            performOTA();
        }
        else
        {
            Serial.println("Already Latest Version");
        }
    }
    else
    {
        Serial.println("Version Check Failed.HTTP Code =");
        Serial.println(httpCode);
    }

    http.end();
}
 // ota function
 void performOTA()
{
    if(WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi Not Connected");
        return;
    }

    Serial.println("Starting OTA Update...");

    
    WiFiClientSecure otaClient; // new 18
    otaClient.setInsecure();

    t_httpUpdate_return ret =
    httpUpdate.update(
    otaClient,
    OTA_URL,
    FIRMWARE_VERSION
    );

    switch(ret)
    {
        case HTTP_UPDATE_FAILED:

            Serial.printf(
                "Update Failed (%d): %s\n",
                httpUpdate.getLastError(),
                httpUpdate.getLastErrorString().c_str()
            );

            break;

        case HTTP_UPDATE_NO_UPDATES:

            Serial.println(
                "No Updates Available"
            );

            break;

        case HTTP_UPDATE_OK:

            Serial.println(
                "Update Successful"
            );

            break;
    }
}

// ================= CALLBACK =================

void callback(

char* topic,

byte* payload,

unsigned int length

){

String msg="";

for(

int i=0;

i<length;

i++

){

msg +=

(char)

payload[i];

}

Serial.println(
msg
);

StaticJsonDocument<200> doc;

DeserializationError error =

deserializeJson(

doc,

msg

);

if(error){

Serial.println(
"JSON Error"
);

return;

}

String command =
doc["command"] | "";
// OTA commad detection
if(command == "OTA_UPDATE")
{
    checkForUpdates();
    return;
}
String commandId =
doc["command_id"] | "";

if(commandId == lastCommandId)
{
    Serial.println(
        "Duplicate Command Ignored"
    );

    return;
}

lastCommandId = commandId;

executeCommand(
    command,
    commandId
);

}

// ================= RECONNECT =================

void reconnect(){

while(!client.connected()){

Serial.println("Trying MQTT Connection...");

String id =
"ESP32-" +
String(random(9999));

if(
client.connect(id.c_str())
){

Serial.println("MQTT Connected");

client.subscribe(
ACTION_TOPIC.c_str()
);

}
else{

Serial.print("MQTT Failed, rc=");
Serial.println(client.state());

delay(2000);

}

}

}

// ================= SETUP =================
void setupBLE(){

BLEDevice::init(
"IMTIYAZ_ESP32"
);

BLEServer *pServer =
BLEDevice::createServer();

BLEService *pService =
pServer->createService(
SERVICE_UUID
);

BLECharacteristic *pCharacteristic =
pService->createCharacteristic(
CHARACTERISTIC_UUID,
BLECharacteristic::PROPERTY_WRITE
);

pCharacteristic->setCallbacks(
new WiFiCallback()
);

pService->start();

BLEAdvertising *pAdvertising =
BLEDevice::getAdvertising();

pAdvertising->addServiceUUID(
SERVICE_UUID
);

pAdvertising->start();

delay(1000);

Serial.println("BLE Advertising Started");

Serial.println("BLE Ready");
Serial.println("Device Name: IMTIYAZ_ESP32");
Serial.println("Waiting for WiFi Credentials...");
}

//----------------------------------------------------up new--

void setup(){

Serial.begin(
115200
);

pinMode(
LIGHT_LED,
OUTPUT
);

pinMode(
FAN_LED,
OUTPUT
);

pinMode(
PUMP_LED,
OUTPUT
);

pinMode(
    WIFI_RESET_BUTTON,
    INPUT_PULLUP
);


loadWiFiCredentials();

if(wifiConfigured)
{
    setup_wifi();

    if(WiFi.status() == WL_CONNECTED)
    {
        wifiConnectedOnce = true;
    }
    else
    {
        Serial.println("WiFi Failed -> Starting BLE");

        wifiConfigured = false;

        setupBLE();
    }
}
else
{
    setupBLE();
}

client.setServer(

mqtt_server,

1883

);

client.setCallback(

callback

);

}

// ================= LOOP =================

void loop(){
//new reset=====================
static unsigned long buttonPressTime = 0;


if(digitalRead(WIFI_RESET_BUTTON)==LOW)
{

    if(buttonPressTime==0)
    {
        buttonPressTime = millis();
    }


    if(millis()-buttonPressTime > 3000)
    {

        Serial.println("Entering BLE Provisioning Mode");


        WiFi.disconnect(true);
        
        wifiConfigured=false;
        wifiConnectedOnce=false;


        setupBLE();


        while(digitalRead(WIFI_RESET_BUTTON)==LOW)
        {
            delay(10);
        }


        buttonPressTime=0;

    }

}
else
{
    buttonPressTime=0;
}


static bool mqttConfigured = false;


if(
wifiConfigured &&
!wifiConnectedOnce
){

WiFi.disconnect(true);

delay(1000);

WiFi.mode(WIFI_STA);

setup_wifi();
wifiConnectedOnce = true;

if(!mqttConfigured){

client.setServer(
mqtt_server,
1883
);

client.setCallback(
callback
);

mqttConfigured = true;
}

}


if(
WiFi.status()==WL_CONNECTED
){

if(
!client.connected()
){
reconnect();
}

client.loop();

}

if(millis() - lastPublish > 2000)
{
    lastPublish = millis();

    StaticJsonDocument<300> doc;

    doc["system"] = "ONLINE";

    doc["light"] =
        digitalRead(LIGHT_LED) ? "ON" : "OFF";

    doc["fan"] =
        digitalRead(FAN_LED) ? "ON" : "OFF";

    doc["pump"] =
        digitalRead(PUMP_LED) ? "ON" : "OFF";

    char buffer[300];

    serializeJson(doc, buffer);

    if(client.connected())
    {
        client.publish(
            STATUS_TOPIC.c_str(),
            buffer
        );
    }
}
}


//WORKING OTA FROM GIT-HUB UPLOAD FIRMWARE
