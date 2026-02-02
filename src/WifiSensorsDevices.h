#ifndef WIFISENSORS_DEVICES_H
#define WIFISENSORS_DEVICES_H

/*
/config
* BUTTON - bounce=[int] default:20
* DHT22 - humid_adj[float] default:0.0, temp_adj[float] default:0.0
* GENERIC_ANALOG - min[float] default:0.0, max[float] default:1023, readcnt[byte] default:1, readdelay[int] default:0, removeminmax[true|false] default:false
* MOTION - bounce=[int] default:5
* RELAY - trigger=[HIGH|LOW] default:HIGH
* DEVICE_TEMP_DALLAS - temp_adj[float] default:0.0
*/

#include "WifiSensorsTypes.h"
#include "WifiSensorsUtils.h"

#include <Bounce2.h>
#include <DallasTemperature.h>
#include <DHT_U.h>
#include <OneWire.h>

extern unsigned long timeNow(ServerStats &stats);

extern WiFiClient wifiClient;
extern Array<DevicesValues, WS_MAX_DEVICES> devicesValues;
extern Bounce *debouncers[WS_MAX_DEVICES];
extern DHT_Unified *dht22s[WS_MAX_DEVICES];
extern DallasTemperature *dallasTemp[WS_MAX_DEVICES];
extern DeviceAddress dallasDeviceAddress[WS_MAX_DEVICES];
extern uint16_t dallasConversionTime[WS_MAX_DEVICES];
extern unsigned long dallasLastTempMeasurement[WS_MAX_DEVICES];

void deviceButtonAttachAnalogPin(Bounce *button, int pin)
{
  switch (pin)
  {
  case 0:
    button->attach(A0);
    break;
  case 1:
    button->attach(A1);
    break;
  case 2:
    button->attach(A2);
    break;
  case 3:
    button->attach(A3);
    break;
  case 4:
    button->attach(A4);
    break;
  case 5:
    button->attach(A5);
    break;
  case 6:
    button->attach(A6);
    break;
  case 7:
    button->attach(A7);
    break;
  }
}

bool deviceIsOutput(DeviceType type)
{
  return (type == DEVICE_RELAY);
}

bool configureButton(Hashtable<String, String> *config, Device *dev)
{
  dev->config.ints[DEVICE_CONFIG_INTS_DEBOUNCE] = 20;
  dev->config.bytes[DEVICE_CONFIG_BYTES_TRIGGER] = 0x1;

  if (config->containsKey("bounce"))
  {
    String bounce = *config->get("bounce");
    dev->config.ints[DEVICE_CONFIG_INTS_DEBOUNCE] = bounce.toInt();
  }
  return true;
}

bool configureDHT22(Hashtable<String, String> *config, Device *dev)
{
  dev->config.floats[DEVICE_CONFIG_FLOAT_HUMID_ADJ] = 0.0;
  dev->config.floats[DEVICE_CONFIG_FLOAT_TEMP_ADJ] = 0.0;

  if (config->containsKey("temp_adj"))
  {
    String adj = *config->get("temp_adj");
    dev->config.floats[DEVICE_CONFIG_FLOAT_TEMP_ADJ] = adj.toFloat();
  }
  if (config->containsKey("humid_adj"))
  {
    String adj = *config->get("humid_adj");
    dev->config.floats[DEVICE_CONFIG_FLOAT_HUMID_ADJ] = adj.toFloat();
  }

  return true;
}

bool configureGenericAnalog(Hashtable<String, String> *config, Device *dev)
{
  if (config->containsKey("min"))
  {
    String adj = *config->get("min");
    dev->config.floats[DEVICE_CONFIG_FLOAT_MIN] = adj.toFloat();
  }
  else
  {
    dev->config.floats[DEVICE_CONFIG_FLOAT_MIN] = 0.0f;
  }
  if (config->containsKey("max"))
  {
    String adj = *config->get("max");
    dev->config.floats[DEVICE_CONFIG_FLOAT_MAX] = adj.toFloat();
  }
  else
  {
    dev->config.floats[DEVICE_CONFIG_FLOAT_MAX] = 1023.0f;
  }
  if (config->containsKey("readcnt"))
  {
    String readcnt = *config->get("readcnt");
    dev->config.bytes[DEVICE_CONFIG_BYTES_ANALOG_READ_CNT] = (byte)readcnt.toInt();
  }
  else
  {
    dev->config.bytes[DEVICE_CONFIG_BYTES_ANALOG_READ_CNT] = 1;
  }
  if (config->containsKey("readdelay"))
  {
    String readdelay = *config->get("readdelay");
    dev->config.ints[DEVICE_CONFIG_INTS_ANALOG_READ_DELAY] = readdelay.toInt();
  }
  else
  {
    dev->config.ints[DEVICE_CONFIG_INTS_ANALOG_READ_DELAY] = 0;
  }
  if (config->containsKey("removeminmax"))
  {
    String removeminmax = *config->get("removeminmax");
    if (removeminmax == "true")
    {
      dev->config.bytes[DEVICE_CONFIG_BYTES_ANALOG_READ_REMOVE_MINMAX] = 0x1;
    }
    else
    {
      dev->config.bytes[DEVICE_CONFIG_BYTES_ANALOG_READ_REMOVE_MINMAX] = 0x0;
    }
  }
  else
  {
    dev->config.bytes[DEVICE_CONFIG_BYTES_ANALOG_READ_REMOVE_MINMAX] = 0x0;
  }

  return true;
}

bool configureGenericDigital(Hashtable<String, String> *config, Device *dev)
{
  return true;
}

bool configureMotion(Hashtable<String, String> *config, Device *dev)
{
  dev->config.ints[DEVICE_CONFIG_INTS_DEBOUNCE] = 5;
  if (config->containsKey("bounce"))
  {
    String bounce = *config->get("bounce");
    dev->config.ints[DEVICE_CONFIG_INTS_DEBOUNCE] = bounce.toInt();
  }

  return true;
}

bool configureRelay(Hashtable<String, String> *config, Device *dev)
{
  dev->pollInterval == -1L;
  dev->config.bytes[DEVICE_CONFIG_BYTES_TRIGGER] = 0x1;

  if (config->containsKey("trigger"))
  {
    String trigger = *config->get("trigger");
    if (trigger == "LOW")
    {
      dev->config.bytes[DEVICE_CONFIG_BYTES_TRIGGER] = 0x0;
    }
    else
    {
      dev->config.bytes[DEVICE_CONFIG_BYTES_TRIGGER] = 0x1;
    }
  }

  return true;
}

bool configureTempDallas(Hashtable<String, String> *config, Device *dev)
{
  dev->config.floats[DEVICE_CONFIG_FLOAT_HUMID_ADJ] = 0.0;
  if (config->containsKey("temp_adj"))
  {
    String adj = *config->get("temp_adj");
    dev->config.floats[DEVICE_CONFIG_FLOAT_TEMP_ADJ] = adj.toFloat();
  }
  return true;
}

bool deviceConfigUpdated(Hashtable<String, String> *config, Device *dev)
{
  if (config->containsKey("interval"))
  {
    String interval = *config->get("interval");
    dev->pollInterval = interval.toInt();
  }

  switch (dev->type)
  {
  case DEVICE_BUTTON:
    return configureButton(config, dev);
  case DEVICE_DHT22:
    return configureDHT22(config, dev);
  case DEVICE_GENERIC_ANALOG_INPUT:
    return configureGenericAnalog(config, dev);
  case DEVICE_GENERIC_DIGITAL_INPUT:
    return configureGenericDigital(config, dev);
  case DEVICE_MOTION:
    return configureMotion(config, dev);
  case DEVICE_RELAY:
    return configureRelay(config, dev);
  case DEVICE_SWITCH:
    return configureButton(config, dev);
  case DEVICE_TEMP_DALLAS:
    return configureTempDallas(config, dev);
  }

  return true;
}

byte deviceValuesNames(DeviceType type, byte deviceId)
{
  switch (type)
  {
  case DEVICE_BUTTON:
    devicesValues[deviceId].names[0] = "state";
    devicesValues[deviceId].units[0] = "on/off";
    return 1;
  case DEVICE_DHT22:
    devicesValues[deviceId].names[0] = "temp";
    devicesValues[deviceId].units[0] = "C";
    devicesValues[deviceId].names[1] = "humid";
    devicesValues[deviceId].units[1] = "%";
    return 2;
  case DEVICE_GENERIC_ANALOG_INPUT:
    devicesValues[deviceId].names[0] = "value";
    devicesValues[deviceId].units[0] = "conf(min)-conf(max)";
    return 1;
  case DEVICE_GENERIC_DIGITAL_INPUT:
    devicesValues[deviceId].names[0] = "value";
    devicesValues[deviceId].units[0] = "0/1";
    return 1;
  case DEVICE_MOTION:
    devicesValues[deviceId].names[0] = "state";
    devicesValues[deviceId].units[0] = "on/off";
    return 1;
  case DEVICE_RELAY:
    devicesValues[deviceId].names[0] = "state";
    devicesValues[deviceId].units[0] = "on/off";
    return 1;
  case DEVICE_SWITCH:
    devicesValues[deviceId].names[0] = "state";
    devicesValues[deviceId].units[0] = "on/off";
    return 1;
  case DEVICE_TEMP_DALLAS:
    devicesValues[deviceId].names[0] = "temp";
    devicesValues[deviceId].units[0] = "C";
    return 1;
  }
  return 0;
}

byte readAnalog(Device *dev, ServerStats *stats, byte readCnt, int readDelay, bool removeMinMax)
{
  if (readCnt < 1)
  {
    readCnt = 1;
  }

  float tmpVal, value = 0.0f;
  int min = 123;
  int max = 0;

  for (byte i = 0; i < readCnt; ++i)
  {
    switch (dev->pins[0].pin)
    {
    case 0:
      tmpVal = analogRead(A0);
      break;
    case 1:
      tmpVal = analogRead(A1);
      break;
    case 2:
      tmpVal = analogRead(A2);
      break;
    case 3:
      tmpVal = analogRead(A3);
      break;
    case 4:
      tmpVal = analogRead(A4);
      break;
    case 5:
      tmpVal = analogRead(A5);
      break;
    case 6:
      tmpVal = analogRead(A6);
      break;
    case 7:
      tmpVal = analogRead(A7);
      break;
    }
    min = (tmpVal < min ? tmpVal : min);
    max = (tmpVal > max ? tmpVal : max);
    value += tmpVal;
    delay(readDelay);
  }
  if (removeMinMax)
  {
    value -= min;
    value -= max;
  }
  value /= (1.0 * readCnt);

  value = 1.0 * map(value, 0, 1023, dev->config.floats[DEVICE_CONFIG_FLOAT_MIN], dev->config.floats[DEVICE_CONFIG_FLOAT_MAX]);
  devicesValues[dev->deviceId].values[0] = String(value, 2);

  if (dev->pushCallback.set)
  {
    String path;
    String strValue = String(value, 2);
    WifiSensorsUtils::prepareCallbackValues(dev->pushCallback.path, strValue, path, devicesValues[dev->deviceId].names[0]);
    return WifiSensorsUtils::sendHttpRequest(dev->pushCallback, path);
  }
  return 0;
}

byte readAnalog(Device *dev, ServerStats *stats)
{
  byte cnt = dev->config.bytes[DEVICE_CONFIG_BYTES_ANALOG_READ_CNT];
  int readDelay = dev->config.bytes[DEVICE_CONFIG_INTS_ANALOG_READ_DELAY];
  bool removeMinMax = dev->config.bytes[DEVICE_CONFIG_BYTES_ANALOG_READ_REMOVE_MINMAX];
  return readAnalog(dev, stats, cnt, readDelay, removeMinMax);
}

byte readDigital(Device *dev, ServerStats *stats)
{
  String value;
  if (digitalRead(dev->pins[0].pin) == HIGH)
  {
    value = "1";
  }
  else
  {
    value = "0";
  }
  devicesValues[dev->deviceId].values[0] = value;

  if (dev->pushCallback.set)
  {
    String path;
    WifiSensorsUtils::prepareCallbackValues(dev->pushCallback.path, value, path, devicesValues[dev->deviceId].names[0]);
    return WifiSensorsUtils::sendHttpRequest(dev->pushCallback, path);
  }
  return 0;
}

byte readButton(Device *dev, ServerStats *stats)
{
  Bounce *b = debouncers[dev->deviceId];
  if (b->update() && b->read() == LOW)
  {
    String value1 = devicesValues[dev->deviceId].values[0] == "on" ? "off" : "on";
    devicesValues[dev->deviceId].values[0] = value1;
    if (dev->pushCallback.set)
    {
      String path;
      WifiSensorsUtils::prepareCallbackValues(dev->pushCallback.path, value1, path, devicesValues[dev->deviceId].names[0]);
      return WifiSensorsUtils::sendHttpRequest(dev->pushCallback, path);
    }
  }
  return 0;
}

byte readDHT22(Device *dev, ServerStats *stats)
{
  sensors_event_t event;
  byte warnCnt = 0;
  String valueTemp;
  String valueHumid;

  DHT_Unified *dht = dht22s[dev->deviceId];
  dht->temperature().getEvent(&event);
  if (isnan(event.temperature))
  {
    warnCnt++;
    stats->lastWarning = "Reading DHT22 TEMP failed!";
    stats->lastWarning += " ";
    stats->lastWarning += timeNow(*stats);
    Serial.println(F("Reading DHT22 TEMP failed!"));
  }
  else
  {
    float adj = dev->config.floats[DEVICE_CONFIG_FLOAT_TEMP_ADJ];
    valueTemp = String(WifiSensorsUtils::adjustPercent(event.temperature, adj), 1);
    devicesValues[dev->deviceId].values[0] = valueTemp;
  }
  dht->humidity().getEvent(&event);
  if (isnan(event.relative_humidity))
  {
    warnCnt++;
    stats->lastWarning = "Reading DTH22 HUMID failed!";
    stats->lastWarning += " ";
    stats->lastWarning += timeNow(*stats);
    Serial.println(F("Reading DTH22 HUMID failed!"));
  }
  else
  {
    float adj = dev->config.floats[DEVICE_CONFIG_FLOAT_HUMID_ADJ];
    valueHumid = String(WifiSensorsUtils::adjustPercent(event.relative_humidity, adj), 1);
    devicesValues[dev->deviceId].values[1] = valueHumid;
  }

  if (dev->pushCallback.set && valueTemp != "" && valueHumid != "")
  {
    String path;
    WifiSensorsUtils::prepareCallbackValues(dev->pushCallback.path, valueTemp, valueHumid, path, devicesValues[dev->deviceId].names[0], devicesValues[dev->deviceId].names[1]);
    return WifiSensorsUtils::sendHttpRequest(dev->pushCallback, path);
  }
  return warnCnt;
}

byte readMotion(Device *dev, ServerStats *stats)
{
  Bounce *b = debouncers[dev->deviceId];
  if (b->update())
  {
    bool changed = false;
    String value1 = devicesValues[dev->deviceId].values[0];
    if (b->read() == HIGH && value1 == "off")
    {
      value1 = "on";
      changed = true;
    }
    else if (b->read() == LOW && value1 == "on")
    {
      value1 = "off";
      changed = true;
    }

    if (changed)
    {
      devicesValues[dev->deviceId].values[0] = value1;

      if (dev->pushCallback.set)
      {
        String path;
        WifiSensorsUtils::prepareCallbackValues(dev->pushCallback.path, value1, path, devicesValues[dev->deviceId].names[0]);
        return WifiSensorsUtils::sendHttpRequest(dev->pushCallback, path);
      }
    }
  }
  return 0;
}

byte readSwitch(Device *dev, ServerStats *stats)
{
  Bounce *b = debouncers[dev->deviceId];
  if (b->update())
  {
    String value1 = b->read() == LOW ? "on" : "off";
    devicesValues[dev->deviceId].values[0] = value1;

    if (dev->pushCallback.set)
    {
      String path;
      WifiSensorsUtils::prepareCallbackValues(dev->pushCallback.path, value1, path, devicesValues[dev->deviceId].names[0]);
      return WifiSensorsUtils::sendHttpRequest(dev->pushCallback, path);
    }
  }
  return 0;
}

byte readTempDallas(Device *dev, ServerStats *stats)
{
  DallasTemperature *tempSensor = dallasTemp[dev->deviceId];

  if (dallasConversionTime[dev->deviceId] == 0)
  {
    tempSensor->requestTemperatures();
    dallasConversionTime[dev->deviceId] = tempSensor->millisToWaitForConversion(tempSensor->getResolution());
  }

  if ((millis() - dallasLastTempMeasurement[dev->deviceId]) > dallasConversionTime[dev->deviceId])
  {
    dallasConversionTime[dev->deviceId] = 0;
    dallasLastTempMeasurement[dev->deviceId] = millis();

    float tempC = tempSensor->getTempC(dallasDeviceAddress[dev->deviceId]);
    if (tempC != DEVICE_DISCONNECTED_C && tempC != -127.00 && tempC != 85.00)
    {
      float adj = dev->config.floats[DEVICE_CONFIG_FLOAT_TEMP_ADJ];
      String temp = String(WifiSensorsUtils::adjustPercent(tempC, adj), 1);
      devicesValues[dev->deviceId].values[0] = temp;
      if (dev->pushCallback.set)
      {
        String path;
        WifiSensorsUtils::prepareCallbackValues(dev->pushCallback.path, temp, path, devicesValues[dev->deviceId].names[0]);
        return WifiSensorsUtils::sendHttpRequest(dev->pushCallback, path);
      }
    }
    else
    {
      stats->lastWarning = "WARN: Could not read TEMP for device: " + dev->deviceId;
      stats->lastWarning += " ";
      stats->lastWarning += timeNow(*stats);
      Serial.print(F("WARN: Could not read TEMP for device: "));
      Serial.println(dev->deviceId);
      return 1;
    }
  }
  return 0;
}

void setupButton(Device *dev)
{
  Bounce *button = new Bounce();
  if (dev->pins[0].type == 'D')
  {
    button->attach(dev->pins[0].pin);
  }
  else
  {
    deviceButtonAttachAnalogPin(button, dev->pins[0].pin);
  }

  button->interval(dev->config.ints[DEVICE_CONFIG_INTS_DEBOUNCE]);
  debouncers[dev->deviceId] = button;

  devicesValues[dev->deviceId].values[0] = "off";
}

void setupDHT22(Device *dev)
{
  DHT_Unified *dht = new DHT_Unified(dev->pins[0].pin, DHT22);
  dht22s[dev->deviceId] = dht;
  dht->begin();

  sensor_t sensor;
  dht->temperature().getSensor(&sensor);
  String msg = String("DHT22 TEMP: ") + String(sensor.name) + String(" ") + String(sensor.version);
  Serial.println(msg);
  dht->humidity().getSensor(&sensor);
  msg = String("DHT22 HUMID: ") + String(sensor.name) + String(" ") + String(sensor.version);
  Serial.println(msg);
  int32_t readDealay = (sensor.min_delay / 1000);
  msg = String("DHT22 DELAY: ") + String(readDealay);
  Serial.println(msg);
  msg = String("DHT22 RESOLUTION: ") + String(sensor.resolution);
  Serial.println(msg);

  devicesValues[dev->deviceId].values[0] = "0.0";
  devicesValues[dev->deviceId].values[1] = "0.0";

  if (readDealay > dev->pollInterval)
  {
    dev->pollInterval = readDealay;
  }
}

void setupGeneric(Device *dev)
{
  if (dev->type == DEVICE_GENERIC_DIGITAL_INPUT)
  {
    devicesValues[dev->deviceId].values[0] = "0";
  }
  else
  {
    devicesValues[dev->deviceId].values[0] = "0.0";
  }
}

void setupRelay(Device *dev)
{
  devicesValues[dev->deviceId].values[0] = "off";
  if (dev->config.bytes[DEVICE_CONFIG_BYTES_TRIGGER] == 0x1)
  {
    WifiSensorsUtils::setPinValue(dev->pins[0].type, dev->pins[0].pin, LOW);
  }
  else
  {
    WifiSensorsUtils::setPinValue(dev->pins[0].type, dev->pins[0].pin, HIGH);
  }
}

void setupTempDallas(Device *dev)
{
  OneWire *oneWire = new OneWire(dev->pins[0].pin);
  DallasTemperature *tempSensor = new DallasTemperature(oneWire);
  dallasTemp[dev->deviceId] = tempSensor;

  tempSensor->begin();

  tempSensor->setWaitForConversion(false);
  tempSensor->getAddress(dallasDeviceAddress[dev->deviceId], 0);

  devicesValues[dev->deviceId].values[0] = "0.0";
}

#endif
