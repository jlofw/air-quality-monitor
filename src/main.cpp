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

WiFiClient iot33Client;
PubSubClient client(iot33Client);
SCD30 scd30;
SPS30 sps30;

float co2, temp, humid;
float massPM2, massPM10;
//float massPM1, massPM4, numPM0, numPM1, numPM2, numPM4, numPM10, partSize; (SPS30 readings not used)
bool data_read;

void connect_wifi();
void reconnect();
void callback(char* topic, byte* payload, unsigned int length);
bool connect_scd30();
bool connect_sps30();
bool read_scd30_data();
bool read_sps30_data();
void send_scd30_data(char *topic, float  co2, float  temp, float  humid);
void send_sps30_data(char *topic, float massPM2, float massPM10);

void setup()
{
  Serial.begin(115200);
  Wire.begin();
  client.setServer(serverIPAddress, 1883);  //broker uses port 1883
  client.setCallback(callback);
  connect_wifi();
  reconnect();

  if (!connect_scd30()) {
    client.publish("sensorstatus","scd30 not connected, stopping...");
    while (true)
    ;
  }
  if (!connect_sps30()) {
    client.publish("sensorstatus","sps30 not connected, stopping...");
    while (true)
    ;
  }

  client.publish("sensorstatus","starting measurements...");
  delay(1000);
}

void loop()
{
  if (!client.connected()) {
    reconnect();
  }

  client.loop();
  delay(1000);

  if (read_scd30_data() && read_sps30_data()) {
    send_scd30_data("sensordata/scd30", co2, temp, humid);
    send_sps30_data("sensordata/sps30", massPM2, massPM10);
    data_read = true;
  }
  else if (!read_scd30_data())
  {
    client.publish("sensorstatus","cant read scd30 data...");
    delay(5000);
  }
  else if (!read_sps30_data())
  {
    client.publish("sensorstatus","cant read sps30 data...");
    delay(5000);
  }
  if (data_read)
  {
    Watchdog.sleep(50000); //sleep for 50s if data is read (disables usb and serial interfaces)
    data_read = false;
  }
}

//connect to wifi with credentials from credentials.h
void connect_wifi()
{
  delay(10);
  Serial.print("connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

//connect MQTT, subscribe to sensorstatus
void reconnect()
{
  while (!client.connected()) {
    Serial.println("connecting MQTT...");

    if (client.connect("arduinoClient")) {
      client.publish("sensorstatus","MQTT connected");
      client.subscribe("sensorstatus");
      Serial.println("MQTT connected");
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");
      delay(5000);
    }
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

//return true if SCD30 is connected
bool connect_scd30()
{
  Serial.println("connecting scd30...");
  client.publish("sensorstatus","connecting scd30...");

  if (scd30.begin() == false) {
    Serial.println("could not start scd30");
    client.publish("sensorstatus","could not start scd30");
    return false;
  }

  scd30.setMeasurementInterval(20);     //sets measurement interval
  scd30.setAutoSelfCalibration(true);   //auto self calibrate
  Serial.println("scd30 connected");
  client.publish("sensorstatus","scd30 connected");
  return true;
}

//return true if SPS30 is connected
bool connect_sps30()
{
  Serial.println("connecting sps30...");
  client.publish("sensorstatus","connecting sps30...");
  sps30.EnableDebugging(0);
  
  if (!sps30.begin(&Wire)) {
    Serial.println("could not connect sps30");
    client.publish("sensorstatus","could not connect sps30");
    return false;
  }
  if (!sps30.probe()) {
    Serial.println("could not probe sps30");
    client.publish("sensorstatus","could not probe sps30");
    return false;
  }
  if (!sps30.reset()) {
    Serial.println("could not reset sps30");
    client.publish("sensorstatus","could not reset sps30");
    return false;
  }
  if (!sps30.start()) {
    Serial.println("could not start sps30");
    client.publish("sensorstatus","could not start sps30");
    return false;
  }

  Serial.println("sps30 connected");
  client.publish("sensorstatus","sps30 connected");
  return true;
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
  else {
    return false;
  }
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
  client.publish(topic, data_a);
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
  client.publish(topic, data_a);
}