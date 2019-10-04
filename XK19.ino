#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <xCore.h>
#include <xSW01.h>
#include <xOD01.h>
#include <xOC03.h>
#include <ArduinoJson.h>    // https://github.com/bblanchon/ArduinoJson
#include <xProvision.h>     // https://github.com/xinabox/arduino-Provision    
#include <ATT_IOT.h>
#include <SPI.h>  // required to have support for signed/unsigned long type.
// include to access SDK functions

struct sensor_acks {
  uint8_t SW01_ACK;
  uint8_t OD01_ACK;
  uint8_t OC03_ACK;
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
boolean WiFiconnect = true;
boolean ATTconnect = true;
boolean Dateget = true;
boolean Locget = true;

String lat;
String lon;
String country;
String city;
String Date;
String Time;
String thermal_zone;

unsigned long prevTime;
unsigned int prevVal = 0;

float tempC, pres, hum;
float dew, alt, cloudBase;
long rssi;

// Callback functions MQTT
void callback(char* topic, byte* payload, unsigned int length);

//Web clients
WiFiClient espClient;
HTTPClient http;

// Constuctors
PubSubClient pubSub(MQTT, 1883, callback, espClient);
ATTDevice device(DEVICE_ID, TOKEN);

//Sensors and actuators
xSW01 SW01;
xOD01 OD01;
xOC03 OC03;

void setup()
{
  Serial.begin(115200);  // Init serial link for debugging

  // Start the I2C Comunication
  Wire.begin(); // no need to input pins included in board file

  //Start xchips
  START_XCHIPS();

  if (c.OD01_ACK == 0xFF)
  {
    OD01.println("Provisioning your device...");
  }

  prv.begin();
  prv.addWiFi();
  prv.addVariable("DEVICE_ID", "ATT_DEVICE_ID");
  prv.addVariable("TOKEN", "ATT_TOKEN");
  prv.transmit();
  prv.receive();
  if (prv.success())
  {
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
    setupWiFi(ssid.c_str(), password.c_str());

    mac = WiFi.macAddress();

    if (WiFiconnect)
    {
      unsigned int count = 0;
      device.setCredentials(DEVICE_ID, TOKEN);
      while (!device.connect(&espClient, HTTP))  // Connect to AllThingsTalk
      {
        Serial.println("retrying");
        delay(100);
        count++;
        if (count > 100)
        {
          ATTconnect = false;
          count = 0;
          Serial.println("Incorrect device credentials!");

          if (c.OD01_ACK == 0xFF) {
            OD01.clear();
            OD01.println("Incorrect cred.");
            delay(3000);
          }
          break;
        }
      }
    }

    if (c.OD01_ACK == 0xFF)
    {
      OD01.clear();
      OD01.println("XinaBox XK19");
      OD01.println("Loading please wait...");
    }

    if (ATTconnect)
    {
      createATTAssets();
    }

    if (ATTconnect)
      while (!device.subscribe(pubSub))  // Subscribe to mqtt
      {
        Serial.println("subscribing");
        delay(100);
      }
  } else {
    if (c.OD01_ACK) {
      OD01.println("Provisioning failed...");
    }
    prv.fail();
  }

  if (c.OD01_ACK) {
    OD01.clear();
  }
}

void loop()
{
  unsigned int Month;
  boolean hemi;

  //Wi-Fi signal strength
  rssi = WiFi.RSSI();

  //Get SW01 measures
  if (c.SW01_ACK)
  {
    SW01.poll();
    tempC = SW01.getTempC();
    pres = SW01.getPressure();
    hum = SW01.getHumidity();
    alt = SW01.getQNE();
    dew = SW01.getDewPoint();
    cloudBase = (((tempC - dew) / 2.5) * 1000) * 0.3048 + alt;
  } else {
    tempC = random(100);
    pres = random(100);
    hum = random(100);
    alt = random(100);
    dew = random(100);
    cloudBase = random(100);
  }

  //Get thermal zone
  if (Dateget && Locget)
  {
    thermal_zone = onlineZones(tempC, hum, hemi, Month);
  } else {
    thermal_zone = offlineZones(tempC, hum);
  }

  if (c.OC03_ACK == 0xFF)
  {
    if (thermal_zone != "comfort zone") {
      OC03.write(HIGH);
    } else if (thermal_zone == "comfort zone") {
      OC03.write(LOW);
    }
  }

  unsigned long curTime = millis();
  if (curTime > (prevTime + 5000))
  {
    // lower TX power may need to check if we are connected
    // will skip if already connected
    if (WiFi.status() != WL_CONNECTED) setupWiFi(ssid.c_str(), password.c_str());

    //Get Latittude, Longitude, Country and city
    getLocation();

    //Get Date and time
    getDateTime();

    Month = (Date.substring(5, 7)).toInt();

    if (lat.toInt() > 0)
    {
      hemi = true;
    } else {
      hemi = false;
    }

    if (ATTconnect)
    {
      sendDatatoATT();
    }
    if (c.OD01_ACK == 0xFF) {
      displayOnOD01();
    }
    prevTime = curTime;
  }
  device.process();
}



/*
  *************************************************************************
  *************************************************************************
   User defines functions
  *************************************************************************
  *************************************************************************
*/



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
    Serial.println("Parsing JSON failed");
}

boolean getDateTime()
{
  String payload;
  const char* datetime;
  String datetimeStr;

  //Fetch time and approx. location
  http.begin("http://worldtimeapi.org/api/ip");

  if (http.GET())
  {
    payload = http.getString();
    Serial.println("Response");
    Serial.println(payload);
  } else {
    Serial.println("Error on request");
    Dateget = false;
  }

  http.end();

  int n = payload.length();

  // declaring character array
  char json[n + 1];

  // copying the contents of the
  // string to char array
  strcpy(json, payload.c_str());


  StaticJsonBuffer<600> jsonBuffer;
  JsonObject& object = jsonBuffer.parseObject(json);
  datetime = object["datetime"];

  datetimeStr = String(datetime);

  Date = datetimeStr.substring(0, 10);
  Time = datetimeStr.substring(11, 19);

  return false;
}

boolean getLocation()
{
  String payload;
  const char * latChr;
  const char * lonChr;
  const char * countryChr;
  const char * cityChr;


  //Fetch time and approx. location
  http.begin("http://ip-api.com/json/");

  if (http.GET())
  {
    payload = http.getString();
    Serial.println("Response");
    Serial.println(payload);
  } else {
    Serial.println("Error on request");
    Locget = false;
    return false;
  }

  http.end();

  int n = payload.length();

  // declaring character array
  char json[n + 1];

  // copying the contents of the
  // string to char array
  strcpy(json, payload.c_str());


  StaticJsonBuffer<600> jsonBuffer;
  JsonObject& object = jsonBuffer.parseObject(json);
  latChr = object["lat"];
  lonChr = object["lon"];
  countryChr = object["country"];
  cityChr = object["city"];

  lat = String(latChr);
  lon = String(lonChr);
  country = String(countryChr);
  city = String(cityChr);

  return true;
}

void createATTAssets()
{
  // Create device assets
  device.addAsset("2", "Temperature", "", "sensor", "{\"type\": \"number\"}");
  device.addAsset("3", "Pressure", "", "sensor", "{\"type\": \"number\"}");
  device.addAsset("4", "Humidity", "", "sensor", "{\"type\": \"number\"}");
  device.addAsset("5", "RSSI", "", "sensor", "{\"type\": \"number\"}");
  device.addAsset("6", "MAC", "", "sensor", "{\"type\": \"string\"}");
  device.addAsset("4", "Humidity", "", "sensor", "{\"type\": \"number\"}");
  device.addAsset("7", "OLED", "", "actuator", "{\"type\": \"string\"}");
  device.addAsset("9", "Altitude", "", "sensor", "{\"type\": \"number\"}");
  device.addAsset("8", "Dew point", "", "sensor", "{\"type\": \"number\"}");
  device.addAsset("10", "Cloud base", "", "sensor", "{\"type\": \"number\"}");
  device.addAsset("city", "City", "", "sensor", "{\"type\": \"string\"}");
  device.addAsset("country", "Country", "", "sensor", "{\"type\": \"string\"}");
  device.addAsset("latitude", "Latitude", "", "sensor", "{\"type\": \"string\"}");
  device.addAsset("longitude", "Longitude", "", "sensor", "{\"type\": \"string\"}");
  device.addAsset("thermal_zone", "Thermal zone", "", "sensor", "{\"type\": \"string\"}");
}

void START_XCHIPS(void)
{
  //OD01
  if ( xCore.ping(0x3C)) {  // if you can ping sensor set ack
    c.OD01_ACK = 0xFF;
    OD01.begin();
  } else {        // don't set
    c.OD01_ACK = 0;
  }

  //SW01
  if ( xCore.ping(0x76)) {  // if you can ping sensor set ack
    c.SW01_ACK = 0xFF;
    SW01.begin();
  } else {        // don't set
    c.SW01_ACK = 0;
  }

  //OC03
  if ( xCore.ping(0x38)) {  // if you can ping sensor set ack
    c.OC03_ACK = 0xFF;
    OC03.begin();
  } else {        // don't set
    c.OC03_ACK = 0;
  }

}
boolean setupWiFi(const char* ssid , const char* password)
{
  unsigned int count = 0;
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
    count++;

    if (count > 100)
    {
      WiFiconnect = false;

      Serial.println();
      Serial.println("WiFi not connected");

      if (c.OD01_ACK == 0xFF) {
        OD01.println("No Wi-Fi!");
        delay(3000);
      }
      count = 0;
      return false;
    }
  }
  Serial.println();
  Serial.println("WiFi connected");
  if (c.OD01_ACK == 0xFF)
  {
    OD01.println("WiFi Connected");
    delay(3000);
  }
  return true;
}

void sendDatatoATT()
{
  device.send(String(alt), "9");
  device.send(String(tempC), "2");
  device.send(String(pres), "3");
  device.send(String(hum), "4");
  device.send(String(rssi), "5");
  device.send(mac, "6");
  device.send(String(dew), "8");
  device.send(String(cloudBase), "10");
  device.send(city, "city");
  device.send(country, "country");
  device.send(lat, "latitude");
  device.send(lon, "longitude");
  device.send(thermal_zone, "thermal_zone");
}

void displayOnOD01()
{
  OD01.clear();
  OD01.home();
  OD01.println("Temp.(C) / RH(%)");
  OD01.print(tempC); OD01.print(" / "); OD01.println(hum);
  OD01.println("City / Country");
  OD01.print(city); OD01.print(" / "); OD01.println(country);
  OD01.println("Thermal zone");
  OD01.println(thermal_zone);
}

String onlineZones(float temp, float hum, boolean tz, unsigned int month)
{
  String zone;
  boolean season;
  month = month % 12;

  if (tz)
  {
    if ((month >= 0) && (month < 6))
    {
      season = true;
    } else {
      season = false;
    }
  } else {
    if ((month >= 6) && (month <= 11))
    {
      season = true;
    } else {
      season = false;
    }
  }

  //Winter
  if (season)
  {
    if ((temp >= 20.5) && (temp <= 23.5) && (hum >= 29.3) && (hum <= 58.3))
    {
      zone = "comfort zone";
    }
    //
    else if ((temp >= 23.5) && (temp < 24.0) && (hum <= 58.3)) {
      zone = "comfort zone";
    } else if ((temp >= 24.0) && (temp < 24.5) && (hum <= 33.3)) {
      zone = "comfort zone";
    } else if ((temp >= 24.5) && (hum < 50.0))
    {
      zone = "hot and dry zone";
    } else if ((temp >= 24.5) && (hum >= 50.0))
    {
      zone = "hot and humid zone";
    }
    //
    else if ((temp >= 20) && (temp < 20.5) && (hum >= 29.3)) {
      zone = "comfort zone";
    } else if ((temp >= 19.5) && (temp < 20) && (hum >= 33.3)) {
      zone = "comfort zone";
    } else if ((temp < 19.5) && (hum < 50.0))
    {
      zone = "cold and dry zone";
    } else if ((temp < 19.5) && (hum >= 50.0))
    {
      zone = "cold and humid zone";
    }
  }
  //Summer
  else {
    if ((temp >= 23.5) && (temp <= 26.0) && (hum >= 24.3) && (hum <= 57.3))
    {
      zone = "comfort zone";
    }
    //
    else if ((temp >= 26.0) && (temp < 26.5) && (hum <= 57.3)) {
      zone = "comfort zone";
    } else if ((temp >= 26.5) && (temp < 27.0) && (hum <= 42.3)) {
      zone = "comfort zone";
    } else if ((temp >= 27.0) && (hum < 50.0))
    {
      zone = "hot and dry zone";
    } else if ((temp >= 27.0) && (hum >= 50.0))
    {
      zone = "hot and humid zone";
    }
    //
    else if ((temp >= 22.5) && (temp < 23.0) && (hum >= 51.95)) {
      zone = "comfort zone";
    } else if ((temp >= 23.0) && (temp < 23.5) && (hum >= 24.4)) {
      zone = "comfort zone";
    } else if ((temp < 22.5) && (hum < 50.0))
    {
      zone = "cold and dry zone";
    } else if ((temp < 22.5) && (hum >= 50.0))
    {
      zone = "cold and humid zone";
    }
  }

  return zone;

}

String offlineZones(float temp, float hum)
{
  String zone;
  //Full comfort zone range
  if ((temp >= 24.5) && (temp <= 26))
  {
    zone = "comfort zone";
  }
  //Comfort zones for temp > 26
  else if ((temp > 26) && (temp <= 26.5) && (hum <= 100.0))
  {
    zone = "comfort zone";
  } else if ((temp > 26.5) && (temp <= 27) && (hum <= 90.0))
  {
    zone = "comfort zone";
  } else if ((temp > 27) && (temp <= 27.25) && (hum <= 80.0))
  {
    zone = "comfort zone";
  } else if ((temp > 27.25) && (temp <= 27.5) && (hum <= 70.0))
  {
    zone = "comfort zone";
  } else if ((temp > 27.5) && (temp <= 28) && (hum <= 60.0))
  {
    zone = "comfort zone";
  } else if ((temp > 28) && (temp <= 28.5) && (hum <= 50.0))
  {
    zone = "comfort zone";
  } else if ((temp > 28.5) && (temp <= 29) && (hum <= 40.0))
  {
    zone = "comfort zone";
  } else if ((temp > 29) && (temp <= 29.5) && (hum <= 30.0))
  {
    zone = "comfort zone";
  } else if ((temp > 29.5) && (temp <= 30.0) && (hum <= 20.0))
  {
    zone = "comfort zone";
  } else if ((temp > 30.0) && (temp <= 30.5) && (hum <= 10.0))
  {
    zone = "comfort zone";
  } else if ((temp > 30.5) && (hum <= 50.0))
  {
    zone = "hot and dry zone";
  } else if ((temp > 30.5) && (hum > 50.0))
  {
    zone = "hot and humid zone";
  }

  //Comfort Zones for temp < 24.5
  else if ((temp >= 24.0) && (temp < 24.5) && (hum >= 10.0))
  {
    zone = "comfort zone";
  } else if ((temp >= 23.5) && (temp < 24) && (hum >= 20.0))
  {
    zone = "comfort zone";
  } else if ((temp >= 23.0) && (temp < 23.5) && (hum >= 30.0))
  {
    zone = "comfort zone";
  } else if ((temp >= 22.75) && (temp < 23.0) && (hum >= 40.0))
  {
    zone = "comfort zone";
  }  else if ((temp >= 22.5) && (temp < 22.75) && (hum >= 50.0))
  {
    zone = "comfort zone";
  }  else if ((temp >= 22.0) && (temp < 22.5) && (hum >= 60.0))
  {
    zone = "comfort zone";
  }  else if ((temp >= 21.75) && (temp < 22.0) && (hum >= 70.0))
  {
    zone = "comfort zone";
  }
  else if ((temp >= 21.5) && (temp < 21.75) && (hum >= 80.0))
  {
    zone = "comfort zone";
  } else if ((temp >= 21.25) && (temp < 21.5) && (hum >= 90.0))
  {
    zone = "comfort zone";
  } else if ((temp >= 21.0) && (temp < 21.25) && (hum >= 95.0))
  {
    zone = "comfort zone";
  }
  else if ((temp < 21.0) && (hum <= 50.0 ))
  {
    zone = "cold and dry zone";
  }
  else if ((temp < 21.0) && (hum > 50.0 ))
  {
    zone = "cold and humid zone";
  }

  return zone;
}
