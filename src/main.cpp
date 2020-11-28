/*
Nov 2020 jlofw

Hardware used: 
Sensirion SCD30   (CO2, humidity and temperature sensor)
Sensirion SPS30   (PM2.5, PM10 sensor)
Arduino Nano 33 IOT

I2C is used to communicate between sensors and Arduino
Arduino sends data over MQTT and wifi to broker.
*/

#include <Wire.h>
#include <SparkFun_SCD30_Arduino_Library.h>
#include <sps30.h>
#include <SPI.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include <Adafruit_SleepyDog.h>

#include "credentials.h"

//wifi credentials from credentials.h 
const char* ssid = networkSSID;
const char* password = networkPASSWORD;

//reserved dhcp address for the MQTT broker (rpi)
const IPAddress serverIPAddress(192, 168, 1, 101);

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
SCD30 scd30;
SPS30 sps30;

enum : byte {
  WLAN_DOWN_MQTT_DOWN,
  WLAN_STARTING_MQTT_DOWN,
  WLAN_UP_MQTT_DOWN,
  WLAN_UP_MQTT_STARTING,
  WLAN_UP_MQTT_UP
} connectionState;

bool flag_connected = false;
const int intervalWLAN = 5000; // (re)connection interval (s)
const int intervalMQTT = 3000; // (re)connection interval (s)
int timeNow;

float co2, temp, humid;
float massPM2, massPM10;
bool data_read;

bool connect_scd30();
bool connect_sps30();
void connect_wifi_mqtt();
void callback(char* topic, byte* payload, unsigned int length);
bool read_scd30_data();
bool read_sps30_data();
void send_scd30_data(char *topic, float  co2, float  temp, float  humid);
void send_sps30_data(char *topic, float massPM2, float massPM10);

void setup()
{
  Serial.begin(115200);
  Wire.begin();
  mqttClient.setServer(serverIPAddress, 1883);  //broker uses port 1883
  mqttClient.setCallback(callback);
  delay(1000);

  if (!connect_scd30()) {
    Serial.println("scd30 not connected, stopping...");
    while (true)
    ;
  }
  if (!connect_sps30()) {
    Serial.println("sps30 not connected, stopping...");
    while (true)
    ;
  }
}

void loop()
{  
  connect_wifi_mqtt();
  mqttClient.loop();

  if (flag_connected)
  {
    if (read_scd30_data() && read_sps30_data()) {
    send_scd30_data("sensordata/scd30", co2, temp, humid);
    send_sps30_data("sensordata/sps30", massPM2, massPM10);
    data_read = true;
    }
    else if (!read_scd30_data())
    {
      mqttClient.publish("sensorstatus", "cant read scd30 data...");
      delay(5000);
    }
    else if (!read_sps30_data())
    {
      mqttClient.publish("sensorstatus", "cant read sps30 data...");
      delay(5000);
    }
    if (data_read)
    {
      Watchdog.sleep(50000); //sleep for 50s if data is read (disables usb and serial interfaces)
      data_read = false;
    }
  }
}

//return true if SCD30 is connected
bool connect_scd30()
{
  Serial.println("connecting scd30...");

  if (scd30.begin() == false) {
    Serial.println("could not start scd30");
    return false;
  }

  scd30.setMeasurementInterval(10);     //sets measurement interval
  Serial.println("scd30 connected");
  return true;
}

//return true if SPS30 is connected
bool connect_sps30()
{
  Serial.println("connecting sps30...");
  sps30.EnableDebugging(0);
  
  if (!sps30.begin(&Wire)) {
    Serial.println("could not connect sps30");
    return false;
  }
  if (!sps30.probe()) {
    Serial.println("could not probe sps30");
    return false;
  }
  if (!sps30.reset()) {
    Serial.println("could not reset sps30");
    return false;
  }
  if (!sps30.start()) {
    Serial.println("could not start sps30");
    return false;
  }

  Serial.println("sps30 connected");
  return true;
}

//(re)connect wifi and mqtt
void connect_wifi_mqtt()
{
  static byte connectionState = WLAN_DOWN_MQTT_DOWN;

  switch (connectionState)
  {
    case WLAN_DOWN_MQTT_DOWN:
      if (WiFi.status() != WL_CONNECTED)
      {
        Serial.println("(re)starting WiFi...");
        WiFi.begin(ssid, password);
        timeNow = millis();
        connectionState = WLAN_STARTING_MQTT_DOWN;
      }
      break;

    case WLAN_STARTING_MQTT_DOWN:
      if (millis() - timeNow >= intervalWLAN)
      {
        Serial.println("wait for WiFi connection...");
        if (WiFi.status() == WL_CONNECTED)
        {
          Serial.println("WiFi connected");
          Serial.println("IP address: ");
          Serial.println(WiFi.localIP());
          connectionState = WLAN_UP_MQTT_DOWN;
        }
        else
        {
          Serial.println("retry wifi connection");
          WiFi.disconnect();
          connectionState = WLAN_DOWN_MQTT_DOWN;
        }
      }
      break;

    case WLAN_UP_MQTT_DOWN:
      if ((WiFi.status() == WL_CONNECTED) && !mqttClient.connected())
      {
        Serial.println("(re)starting MQTT...");
        timeNow = millis();
        connectionState = WLAN_UP_MQTT_STARTING;
      }
      break;

    case WLAN_UP_MQTT_STARTING:
      if (millis() - timeNow >= intervalMQTT)
      {
        Serial.println("wait for MQTT connection...");
        if (mqttClient.connect("arduinoClient"))
        {
          Serial.println("WiFi and MQTT connected");
          mqttClient.subscribe("sensorstatus");
          mqttClient.publish("sensorstatus", "WiFi and MQTT connected");
          connectionState = WLAN_UP_MQTT_UP;
        }
        else
        {
          Serial.println("retry MQTT connection");
          connectionState = WLAN_UP_MQTT_DOWN;
        }
      }
      break;

    case WLAN_UP_MQTT_UP:
      if (WiFi.status() != WL_CONNECTED)
      {
        flag_connected = false;
        connectionState = WLAN_DOWN_MQTT_DOWN;  //reconnect wifi and mqtt
      }
      else {
        flag_connected = true;
      }
      break;
  }
}

//read MQTT messages in subscribed topics
void callback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  for (int i=0; i<length; i++) {
    Serial.print((char)payload[i]);
  }

  Serial.println();
}

//return true if SCD30 data is read
bool read_scd30_data()
{
  if (scd30.dataAvailable()) {
    co2 = scd30.getCO2();
    temp = scd30.getTemperature();
    humid = scd30.getHumidity();
    return true;
  }
  return false;
}

//return true if SPS30 data is read
bool read_sps30_data()
{
  uint8_t ret, error_cnt = 0;
  struct sps_values val;      //read data is saved in this struct

  do {
    ret = sps30.GetValues(&val);
    if (ret == ERR_DATALENGTH){
        if (error_cnt++ > 3) {
          return(false);    //error during reading sps30 values
        }
        delay(1000);
    }
    else if(ret != ERR_OK) {
      return(false);    //error during reading sps30 values
    }
  } while (ret != ERR_OK);

  massPM2 = val.MassPM2;
  massPM10 = val.MassPM10 - val.MassPM2;    //PM10 - PM2 to see all particles between 2.5 to 10 micrometer
  return true;
}

//send SCD30 data via MQTT in JSON format
void send_scd30_data(char *topic, float co2, float temp, float humid)
{
  char data_a[50];
  String msg_buffer = "{ \"c\": " + String(co2);
  String msg_buffer2 = ", \"t\": " + String(temp);
  String msg_buffer3 = ", \"h\": " + String(humid);
  String msg_buffer4 = "}";
  
  msg_buffer = msg_buffer + msg_buffer2 + msg_buffer3 + msg_buffer4;
  msg_buffer.toCharArray(data_a, msg_buffer.length() +1);
  mqttClient.publish(topic, data_a);
}

//send SPS30 data via MQTT in JSON format
void send_sps30_data(char *topic, float massPM2, float massPM10)
{
  char data_a[50];
  String msg_buffer = "{ \"m\": " + String(massPM2);
  String msg_buffer2 = ", \"n\": " + String(massPM10);
  String msg_buffer3 = "}";
  
  msg_buffer = msg_buffer + msg_buffer2 + msg_buffer3;
  msg_buffer.toCharArray(data_a, msg_buffer.length() +1);
  mqttClient.publish(topic, data_a);
}