# air-quality-monitor (deprecated)
Constructed by Sensirion SCD30 and SPS30 sensors, Arduino Nano IoT and Raspberry Pi 4B. Used software is Mosquitto, Node-RED, InfluxDB and Grafana.

## Grafana with data from sensors
<img src="https://i.imgur.com/OfrikDk.png" width="800px" height="auto">

## Overview of system
<img src="https://i.imgur.com/0YbnKn8.png" width="631px" height="auto">

I recommend using [IOTstack](https://github.com/SensorsIot/IOTstack) for an easy setup of the Raspberry Pi and its software.

## Overview of Node-RED
<img src="https://i.imgur.com/wC6GP4O.png" width="612px" height="auto">

The output from the Arduino is in the following JSON format:
```
{
  "c": 478,
  "t": 24.02,
  "h": 38.39
}
```
Node-RED sets it to the following:
```
{
  "co2": 478,
  "temperature": 24.02,
  "humidity": 38.39
}
```
with a change node and the following code:
```
{
  "co2": msg.payload.c,
  "temperature": msg.payload.t,
  "humidity": msg.payload.h
}
```

## Libraries used
[SparkFun_SCD30_Arduino_Library](https://github.com/sparkfun/SparkFun_SCD30_Arduino_Library)

[sps30 by paulvha](https://github.com/paulvha/sps30)

[pubsubclient by knolleary](https://github.com/knolleary/pubsubclient)

[Adafruit_SleepyDog](https://github.com/adafruit/Adafruit_SleepyDog)

