#include <Wire.h>
#include <SparkFun_SCD30_Arduino_Library.h>
//#include <sps30.h>

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
//SPS30 sps30;

void connect_wifi();
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void connect_sensors();
bool read_sensors();
void send_scd30_data(char *topic, float  co2, float  temp, float  humid);

float  co2, temp, humid;
//int massPM1, massPM2, massPM4, massPM10;
//int numPM0, numPM1, numPM2, numPM4, numPM10, partSize;

void setup()
{
  Serial.begin(115200);
  Wire.begin();
  client.setServer(serverIPAddress, 1883);
  client.setCallback(callback);
  connect_wifi();
  connect_sensors();
  delay(1500);
}

void loop()
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  delay(4000);
  if (read_sensors()) {
    send_scd30_data("sensordata/scd30", co2, temp, humid);
  }
}

void connect_wifi() 
{
  delay(10);
  Serial.print("Connecting to ");
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
  Serial.print("Message arrived [");
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
    Serial.println("Attempting MQTT connection...");

    if (client.connect("arduinoClient")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("sensorstatus","connected");
      // ... and resubscribe
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

void connect_sensors()
{
  Serial.println("connecting sensors...");
  if (scd30.begin() == false) {
    Serial.println("scd30 not detected. Please check wiring. ...");
  }
  /*
  if (sps30.begin(&Wire) == false) {
    Serial.println("sps30 not detected. Please check wiring. ...");
  }
  if (! sps30.probe()) {
    Serial.println("could not probe sps30...");
  }
  */
}

bool read_sensors()
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

  delay(500);

  /*
  uint8_t ret = 0, error_cnt = 0;
  struct sps_values val;
  while (ret != ERR_OK) {
    ret = sps30.GetValues(&val);
    if (ret == ERR_DATALENGTH){
      if (error_cnt++ > 3) {
        Serial.println("Error during reading values: ");
      }
      delay(1000);
    }
    else if (ret != ERR_OK) {
      Serial.println("Error during reading values: ");
    }
  }
  Serial.print(val.MassPM1);
  Serial.print(val.MassPM2);
  Serial.print(val.MassPM4);
  Serial.print(val.MassPM10);
  Serial.print(val.NumPM0);
  Serial.print(val.NumPM1);
  Serial.print(val.NumPM2);
  Serial.print(val.NumPM4);
  Serial.print(val.NumPM10);
  Serial.print(val.PartSize);
  */
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
  client.publish(topic, data_a); //money shot
}