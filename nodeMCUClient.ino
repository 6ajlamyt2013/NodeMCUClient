#include <Adafruit_BME280.h>
#include <Adafruit_VL53L0X.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266HTTPClient.h>
#include <NTPClient.h>
#include <TimeLib.h>

//Client data
#define potID  4
int preferencePlants = 1;

///Network connects
const char* ssid = "MTS_GPON_DFF3";
const char* password = "HGPAYAtY";
char* host = "http://192.168.1.4:8888/";

///Pin Relay
#define LIGHT_PIN_RELAY 14
#define WATER_PIN_RELAY 16
String lightRelayState, pompRelayState;

///Soil Moistures
#define  SENSOR         A0  
#define  MIN            950                    
#define  MAX            515                    
uint16_t soilMoisture;   

///BME280
#define SEALEVELPRESSURE_HPA (989.25)
#define BME_PIN 0x76
Adafruit_BME280 bme;
unsigned status, temperature, pressure, altitude, humidity;

///Settings for get time 
WiFiUDP udp;
NTPClient timeClient(udp);
int secs, h, m, s= 0;

///Laser sensor
#define LASER_PIN       0x29
Adafruit_VL53L0X laser = Adafruit_VL53L0X();
VL53L0X_RangingMeasurementData_t measure;
int waterLevel, distantce;

///Local time 
#define TIMER_PIN       0x57
#define TIMER_DATA_PIN  0x68

void setup() {  
  //Open serial port
  Serial.begin(9600); 
  
  //Pin relay
  pinMode(LIGHT_PIN_RELAY, OUTPUT);
  pinMode(WATER_PIN_RELAY, OUTPUT);
  
  //wi-fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting...");
  }
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  //Init NTP timer object
  timeClient.begin();

  //Init wire object (i2c)
  Wire.begin(4, 5);
  
  Serial.println("BME Start...");
  if (!bme.begin(BME_PIN)) {                                                  
    Serial.println("Could not find a valid BME280 sensor, check wiring!"); // Печать, об ошибки инициализации.
    while (1);                                                             // Зацикливаем
  }

  Serial.println("Adafruit VL53L0X Start...");
  if (!laser.begin(0x29)) {
    Serial.println(F("Failed to boot VL53L0X"));
    while(1);
  }
}

void loop() {
  //bme 
  temperature = bme.readTemperature();
  pressure = bme.readPressure() / 100.0F;
  altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
  humidity = bme.readHumidity();
  Serial.print("temperature: ");
  Serial.println(temperature);
  Serial.print("pressure: ");
  Serial.println(pressure);
  Serial.print("altitude: ");
  Serial.println(altitude);
  Serial.print("humidity: ");
  Serial.println(humidity);
  
  //Wi-Fi
    WiFiClient client;
    HTTPClient http;
  
    String getData = ((String)"?method=updateData&pot_id=" + potID + "" + "&soil_moisture=" + soilMoisture + "&temperature=" + temperature + "&pressure=" + pressure + "&altitude=" + altitude + "&humidity=" + humidity + "&water_level=" + waterLevel + "&light_relay=" + lightRelayState + "&pomp_relay=" + pompRelayState);
    String request = host + getData;
    http.begin(client, request); 
    if (int httpCode = http.GET() != 200) {
      Serial.println("httpCode: " + httpCode);
    } 
    Serial.println(request);
    String json = http.getString();
    
    StaticJsonDocument<500> doc;
    DeserializationError error = deserializeJson(doc, json);
    int settingSoilMoisture = doc["settingSoilMoisture"];
    String settingTimeOn = doc["settingTimeOn"];
    String settingTimeOff = doc["settingTimeOff"];
    Serial.println(json);
    
    getSoilMoisture();
    Serial.print("Current soil moistures: ");
    Serial.println(soilMoisture);
    
    gitWaterLevel();
    Serial.print("Distance to bottom (mm): ");
    Serial.println(waterLevel);
    
    lightTimerLogic(settingTimeOn, settingTimeOff);
    Serial.println("Return to main, lightRelayState: " + lightRelayState);
    
    pump(soilMoisture,  settingSoilMoisture);
    Serial.println("Return to main, pompRelayState: " + pompRelayState);

    http.end();  
    delay(10000); 
}

int gitWaterLevel() {
  ///Laser sensor VL53L0X
  laser.rangingTest(&measure, false); // pass in 'true' to get debug data printout!
  return waterLevel = measure.RangeMilliMeter;
}

int getSoilMoisture() {
//Soil Moistures
  soilMoisture = analogRead(SENSOR);                                          
  return soilMoisture = map(soilMoisture, MIN, MAX, 0, 100);                        
 }

int converterDataTime (String dataTime) {
  //Conerting to sec
  if (sscanf(dataTime.c_str(), "%d:%d:%d", &h, &m, &s) >= 2) {
    int secs = h *3600 + m*60 + s;
    Serial.println("Data time: " + dataTime + " in seconds " +  secs);
    return secs; 
  } 
}

String lightTimerLogic(String settingTimeOn, String settingTimeOff) {
  // Get current time HH:MM:SS
  timeClient.update();
  String currentTimeHHMMSS = timeClient.getFormattedTime();
      
  if (converterDataTime(settingTimeOn) < converterDataTime(currentTimeHHMMSS) < converterDataTime(settingTimeOff)) { 
    digitalWrite(LIGHT_PIN_RELAY,HIGH);
    lightRelayState = "On";
  } else {
    digitalWrite(LIGHT_PIN_RELAY,LOW);
    lightRelayState = "Off";
  }
  return lightRelayState;
}

String pump( int soilMoisture, int settingSoilMoisture ) {
  if(soilMoisture < settingSoilMoisture  || soilMoisture > 60000) {                   
    digitalWrite(WATER_PIN_RELAY,HIGH);
    pompRelayState = "On";
  } else if (soilMoisture > settingSoilMoisture) {
    digitalWrite(WATER_PIN_RELAY,LOW);
    pompRelayState = "Off";
  }
  
  return pompRelayState;
}
