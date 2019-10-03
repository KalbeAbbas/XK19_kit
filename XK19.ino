//#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <xCore.h>
#include <xSW01.h>
#include <xOD01.h>
#include <ArduinoJson.h>    // https://github.com/bblanchon/ArduinoJson
#include <xProvision.h>     // https://github.com/xinabox/arduino-Provision    
#include <ATT_IOT.h>
#include <SPI.h>  // required to have support for signed/unsigned long type.
// include to access SDK functions

struct sensor_acks {
  uint8_t SW01_ACK;
  uint8_t OD01_ACK;
};
struct sensor_acks c;

#define JSON
xProvision prv;
// Define http and mqtt endpoints
#define HTTP "api.allthingstalk.io"  // API endpoint
#define MQTT "api.allthingstalk.io"  // broker

String TOKEN = "";
String DEVICE_ID = "";
String ssid = "";
String password = "";
String mac = "";
long rssi;
String payload;

const char* lat;
const char* lon;

unsigned long prevTime;
unsigned int prevVal = 0;

// Callback functions MQTT
void callback(char* topic, byte* payload, unsigned int length);
// Constuctors
WiFiClient espClient;
HTTPClient http;
/*PubSubClient pubSub(mqtt, 1883, callback, espClient);
  ATTDevice device(DEVICE_ID, TOKEN);*/
xSW01 SW01;
xOD01 OD01;
//void callback(char* topic, byte* payload, unsigned int length);
void setup()
{
  Serial.begin(115200);  // Init serial link for debugging
  // Start the I2C Comunication

  // Set the I2C Pins for CW01
#ifdef ESP8266
  Wire.pins(2, 14);
  Wire.setClockStretchLimit(15000);
#endif

  Wire.begin(); // no need to input pins included in board file
  START_XCHIPS();

  if (c.OD01_ACK == 0xFF)
  {
    OD01.println("Provisioning your device...");
  }

  //prv.begin();
  //prv.addWiFi();
  //prv.addVariable("DEVICE_ID", "ATT_DEVICE_ID");
  //prv.addVariable("TOKEN", "ATT_TOKEN");
  //prv.transmit();
  //prv.receive();
  //if (prv.success())
  //{
  prv.getWiFi(ssid, password);
  if (!password)password = "";
  prv.getVariable("DEVICE_ID", DEVICE_ID);
  prv.getVariable("TOKEN", TOKEN);
  if (c.OD01_ACK == 0xFF)
  {
    OD01.clear();
    OD01.println("Connecting to Wi-Fi");
    OD01.println("please wait...");
  }
  // Enter your WiFi credentials here!
  setupWiFi("INTERACTIVE-BRAINS3", "AllahMohammad110");

  //Get Latittude and Longitude
  getLocation();


  /*system_phy_set_max_tpw(10); // set transmit power to 10 (min:1 , max:23
    mac = WiFi.macAddress();
    device.setCredentials(DEVICE_ID, TOKEN);
    //Serial.println(DEVICE_ID, TOKEN);
    while (!device.connect(&espClient, http))  // Connect to AllThingsTalk
    {
    Serial.println("retrying");
    delay(100);
    }

    if (c.OD01_ACK == 0xFF)
    {
    OD01.clear();
    OD01.println("Weather Station");
    OD01.println("Loading please wait...");
    }
    // Create device assets
    device.addAsset("1", "Temperature", "", "sensor", "{\"type\": \"number\"}");
    device.addAsset("2", "Pressure", "", "sensor", "{\"type\": \"number\"}");
    device.addAsset("3", "Humidity", "", "sensor", "{\"type\": \"number\"}");
    device.addAsset("4", "Lux", "", "sensor", "{\"type\": \"number\"}");
    device.addAsset("5", "UVA", "", "sensor", "{\"type\": \"number\"}");
    device.addAsset("6", "UVB", "", "sensor", "{\"type\": \"number\"}");
    device.addAsset("7", "RSSI", "", "sensor", "{\"type\": \"number\"}");
    device.addAsset("8", "MAC", "", "sensor", "{\"type\": \"string\"}");
    device.addAsset("9", "OLED", "", "actuator", "{\"type\": \"string\"}");
    device.addAsset("10", "Altitude", "", "sensor", "{\"type\": \"number\"}");
    device.addAsset("11", "Dew point", "", "sensor", "{\"type\": \"number\"}");
    device.addAsset("12", "Cloud base", "", "sensor", "{\"type\": \"number\"}");
    while (!device.subscribe(pubSub))  // Subscribe to mqtt
    {
    Serial.println("retrying");
    delay(100);
    }*/
  /*} else {
    if (c.OD01_ACK) {
      OD01.println("Provisioning failed...");
    }
    prv.fail();
    }*/
  //wifi_set_sleep_type(MODEM_SLEEP_T);
  if (c.OD01_ACK) {
    OD01.clear();
  }
}
void START_XCHIPS(void)
{
  if ( xCore.ping(0x3C)) {  // if you can ping sensor set ack
    c.OD01_ACK = 0xFF;
    OD01.begin();
  } else {        // don't set
    c.OD01_ACK = 0;
  }
  if ( xCore.ping(0x76)) {  // if you can ping sensor set ack
    c.SW01_ACK = 0xFF;
    SW01.begin();
  } else {        // don't set
    c.SW01_ACK = 0;
  }
  /* if ( xCore.ping(0x10) && xCore.ping(0x29)) {  // if you can ping sensor set ack
     c.SL01_ACK = 0xFF;
     SL01.begin();
    } else {        // don't set
     c.SL01_ACK = 0;
    }*/
}
void setupWiFi(const char* ssid , const char* password)
{
  delay(10);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  if (c.OD01_ACK == 0xFF)
  {
    OD01.print("Connecting to ");
    OD01.println(ssid);
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  if (c.OD01_ACK == 0xFF)
  {
    OD01.println("WiFi Connected");
    delay(3000);
  }
}
void loop()
{
  Serial.println("lat, lon");
  
  Serial.println(lat);
  Serial.println(lon);

  Serial.println(payload);

  delay(1000);
  
  /*unsigned long curTime = millis();
    if (curTime > (prevTime + 5000))  // Update and send counter value every 5 seconds
    {
    // lower TX power may need to check if we are connected
    // will skip if already connected
    if(WiFi.status() != WL_CONNECTED) setupWiFi(ssid.c_str(), password.c_str());
    float tempC, pres, hum;
    float lux, uva, uvb;
    float alt, dew, cloudBase;
    rssi = WiFi.RSSI();
    if (c.SW01_ACK)
    {
      SW01.poll();
      tempC = SW01.getTempC();
      pres = SW01.getPressure();
      hum = SW01.getHumidity();
      alt = SW01.getQNE();
      dew = SW01.getDewPoint();
      cloudBase = ((tempC - dew) / 4.4) * 1000 + alt;
    } else {
      tempC = random(100);
      pres = random(100);
      hum = random(100);
      alt = random(100);
      dew = random(100);
      cloudBase = random(100);
    }
    if (c.SL01_ACK)
    {
      SL01.poll();
      lux = SL01.getLUX();
      uva = SL01.getUVA();
      uvb = SL01.getUVB();
    } else {
      lux = random(100);
      uva = random(100);
      uvb = random(100);
    }
    device.send(String(alt), "10");
    device.send(String(tempC), "1");
    device.send(String(pres), "2");
    device.send(String(hum), "3");
    device.send(String(lux), "4");
    device.send(String(uva), "5");
    device.send(String(uvb), "6");
    device.send(String(rssi), "7");
    device.send(mac, "8");
    device.send(String(dew), "11");
    device.send(String(cloudBase), "12");
    if (c.OD01_ACK == 0xFF) {
      OD01.home();
      OD01.print("Temp.: ");
      OD01.print(tempC);
      OD01.println(" C");
      OD01.print("Press.: ");
      OD01.print(pres);
      OD01.println(" Pa");
      OD01.print("Hum.: ");
      OD01.print(hum);
      OD01.println(" %");
      OD01.print("LUX: ");
      OD01.print(lux);
      OD01.println("LUX");
      OD01.print("UVA: ");
      OD01.print(uva);
      OD01.println(" uW/cm^2");
      OD01.print("UVB: ");
      OD01.print(uvb);
      OD01.println(" uW/cm^2");
    }
    prevTime = curTime;
    }
    device.process();
    yield(); // this helps with CW01
    }
    void callback(char* topic, byte* payload, unsigned int length)
    {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    // Convert payload to json
    StaticJsonBuffer<500> jsonBuffer;
    char json[500];
    for (int i = 0; i < length; i++) {
    json[i] = (char)payload[i];
    }
    json[length] = '\0';
    JsonObject& root = jsonBuffer.parseObject(json);
    // Do something
    if (root.success())
    {
    const char* value = root["value"];
    if (c.OD01_ACK == 0xFF) {
      OD01.clear();
      OD01.println(value);
    }
    device.send(value, "toggle");  // Send command back as ACK using JSON
    delay(2000);
    }
    else
    Serial.println("Parsing JSON failed");*/
}

void getLocation()
{
    //Fetch time and approx. location
  http.begin("http://ip-api.com/json/");

  if (http.GET())
  {
    payload = http.getString();
    Serial.println("Response");
    Serial.println(payload);
  } else {
    Serial.println("Error on request");
  }

  http.end();

    int n = payload.length(); 
  
    // declaring character array 
    char json[n + 1]; 
  
    // copying the contents of the 
    // string to char array 
    strcpy(json, payload.c_str()); 
  

  StaticJsonBuffer<500> jsonBuffer;
  JsonObject& object = jsonBuffer.parseObject(json);
  lat = object["lat"];
  lon = object["lon"];
}
