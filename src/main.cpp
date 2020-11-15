#include <Wire.h>
#include <SparkFun_SCD30_Arduino_Library.h>
#include <sps30.h>

#include <SPI.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>

#include "credentials.h"

const char* ssid = networkSSID;
const char* password = networkPASSWORD;
const char* mqttServer = mqttSERVER;
const char* mqttUsername = mqttUSERNAME;
const char* mqttPassword = mqttPASSWORD;

const IPAddress serverIPAddress(192, 168, 1, 101);

WiFiClient iot33Client;
PubSubClient client(iot33Client);
SCD30 scd30;
SPS30 sps30;

void connect_wifi();
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
bool connect_scd30();
bool connect_sps30();
bool read_scd30_data();
bool read_sps30_data();
void send_scd30_data(char *topic, float  co2, float  temp, float  humid);
void send_sps30_data(char *topic, float massPM2, float massPM10);

float  co2, temp, humid;
float massPM2, massPM10;
//float massPM1, massPM4, numPM0, numPM1, numPM2, numPM4, numPM10, partSize;

void setup()
{
  Serial.begin(115200);
  Wire.begin();
  client.setServer(serverIPAddress, 1883);
  client.setCallback(callback);
  connect_wifi();
  if (!connect_scd30()) {
    Serial.println("could not connect scd30, stopping...");
    while (true)
    ;
  }
  if (!connect_sps30()) {
    Serial.println("could not connect sps30, stopping...");
    while (true)
    ;
  }
  delay(1500);
}

void loop()
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  delay(4000);
  if (read_scd30_data()) {
    send_scd30_data("sensordata/scd30", co2, temp, humid);
  }
  else {
    Serial.println("no new scd30 data");
  }
  delay(1000);
  if (read_sps30_data()) {
    send_sps30_data("sensordata/sps30", massPM2, massPM10);
  }
  else
  {
    Serial.println("no new sps30 data");
  }
}

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

void callback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect()
{
  while (!client.connected()) {
    Serial.println("attempting MQTT connection...");

    if (client.connect("arduinoClient")) {
      Serial.println("connected");
      client.publish("sensorstatus","connected");
      client.subscribe("sensorstatus");
    }

    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

bool connect_scd30()
{
  Serial.println("connecting scd30...");
  if (scd30.begin() == false) {
    Serial.println("scd30 not detected. Please check wiring. ...");
    return false;
  }
  Serial.println("scd30 connected");
  return true;
}

bool connect_sps30()
{
  Serial.println("connecting sps30...");
  sps30.EnableDebugging(0);
  if (!sps30.begin(&Wire)) {
    Serial.println("sps30 not detected. Please check wiring. ...");
    return false;
  }
  if (!sps30.probe()) {
    Serial.println("could not probe sps30...");
    return false;
  }
  if (!sps30.reset()) {
    Serial.println("could not reset sps30...");
    return false;
  }
  if (!sps30.start()) {
    Serial.println("could not start sps30...");
    return false;
  }
  Serial.println("sps30 connected");
  return true;
}

bool read_scd30_data()
{
  if (scd30.dataAvailable()) {
    co2 = scd30.getCO2();
    temp = scd30.getTemperature();
    humid = scd30.getHumidity();
    return true;
  }
  else {
    Serial.println("Waiting for new data");
    return false;
  }
}

bool read_sps30_data()
{
  uint8_t ret, error_cnt = 0;
  struct sps_values val;
  do {
    ret = sps30.GetValues(&val);
    if (ret == ERR_DATALENGTH){
        if (error_cnt++ > 3) {
          Serial.println("error during reading sps30 values");
          return(false);
        }
        delay(1000);
    }
    else if(ret != ERR_OK) {
      Serial.println("error during reading sps30 values");
      return(false);
    }
  } while (ret != ERR_OK);
  massPM2 = val.MassPM2;
  massPM10 = val.MassPM10;
  

  Serial.println(val.MassPM2);
  Serial.println(massPM2);
  Serial.println(val.MassPM10);
  Serial.println(massPM10);
  
  /*
  Serial.println(val.PartSize);
  Serial.println(partSize);
  Serial.println(val.MassPM1);
  Serial.println(val.MassPM2);
  Serial.println(val.MassPM4);
  Serial.println(val.MassPM10);
  Serial.println(val.NumPM0);
  Serial.println(val.NumPM1);
  Serial.println(val.NumPM2);
  Serial.println(val.NumPM4);
  Serial.println(val.NumPM10);
  Serial.println(val.PartSize);
  */
  return true;
}

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

void send_sps30_data(char *topic, float massPM2, float massPM10)
{
  char data_a[50];
  String msg_buffer = "{ \"m\": " + String(massPM2);
  String msg_buffer2 = ", \"n\": " + String(massPM10);
  String msg_buffer3 = "}";
  
  msg_buffer = msg_buffer+ msg_buffer2 + msg_buffer3;
  msg_buffer.toCharArray(data_a, msg_buffer.length() +1);
  client.publish(topic, data_a);
}