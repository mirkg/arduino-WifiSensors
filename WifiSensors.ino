
#define DEBUG 0
#define WAIT_FOR_SERIAL 0

#define WS_AP_NAME_PREFIX "ARDUINO_"
#define WS_SERVER_PORT 80
#define VALUES_PROCESSING_TIME 50
#define SERVER_PROCESSING_TIME 100
#define LOW_MEMORY 3000
#define STATUS_PIN 13

#include "arduino_secrets.h"
#include "src/WifiSensorsDevices.h"
#include "src/WifiSensorsUtils.h"

#include <algorithm>
#include <FlashStorage.h>
#include <SPI.h>
#include <WiFiNINA.h>
#include <Wire.h>

FlashStorage(conf_flash_store, ServerConfig);
FlashStorage(pinout_flash_store, Pinout);
FlashStorage(devices_flash_store, Devices);

volatile RunningMode runMode = RUN_MODE_SERVER;
volatile byte runStatus = 0x0;
int status = WL_IDLE_STATUS;
bool restatPending = false;
unsigned long then;
String currentLine;
String requestPath;

Pinout pinout;
Devices devices;
DevicesValues devicesValues[WS_MAX_DEVICES];

// devices specific
Bounce *debouncers[WS_MAX_DEVICES];
DHT_Unified *dht22s[WS_MAX_DEVICES];
DallasTemperature *dallasTemp[WS_MAX_DEVICES];
DeviceAddress dallasDeviceAddress[WS_MAX_DEVICES];
uint16_t dallasConversionTime[WS_MAX_DEVICES];
unsigned long dallasLastTempMeasurement[WS_MAX_DEVICES];
// end devices specific

ServerStats stats;
ServerConfig serverConfig;
String authHeader;
WiFiClient wifiClient;
WiFiServer server(WS_SERVER_PORT);

void setup()
{
  factoryReset();

  setupSerial();

  setupPins();

  Wire.begin();

  setupDevices();

  setupServer();
}

void loop()
{
  showRunStatus();

  checkAPStatus();

  restart(false, 0);

  handleInputDevices();

  handleSerwer();

  handleMemory();
}

void addDevice(DeviceType deviceType, byte requiredPins, Array<DevicePin, WS_MAX_DEVICE_PINS> &pins, long pollInterval, Hashtable<String, String> *config)
{
  WifiSensorsUtils::sendHeader("200 OK", "application/json");

  if (deviceType == DEVICE_UNKNOWN)
  {
    String msg = String("unknown device type: ") + deviceType;
    WifiSensorsUtils::WifiSensorsUtils::sendError(msg);
    return;
  }

  Device dev;
  dev.deviceId = devices.count;
  dev.type = deviceType;
  dev.active = true;

  for (byte i = 0; i < requiredPins; i++)
  {
    char t = pins[i].type;
    int pin = pins[i].pin;

    bool invalid = false;
    if (t == 'A')
    {
      if (pin < 0 || pin > WS_ANALOG_PINS - 1)
      {
        invalid = true;
      }
    }
    else if (t == 'D')
    {
      if (pin < 0 || pin > WS_DIGITAL_PINS - 1)
      {
        invalid = true;
      }
    }
    else
    {
      invalid = true;
    }

    if (invalid)
    {
      String msg = String("invalid pin: ") + t + pin;
      WifiSensorsUtils::WifiSensorsUtils::sendError(msg);
      return;
    }

    if ((t == 'D' && pinout.used[pin]) || (t == 'A' && pinout.used[WS_DIGITAL_PINS + pin]))
    {
      String msg = String("pin already in use: ") + t + pin;
      WifiSensorsUtils::WifiSensorsUtils::sendError(msg);
      return;
    }

    DevicePin dpin;
    dpin.type = t;
    dpin.pin = pin;
    dpin.mode = pins[i].mode;
    dev.pins[i] = dpin;
  }

  if (!deviceConfigUpdated(config, &dev))
  {
    return;
  }

  dev.pollInterval = pollInterval;

  devices.devices[dev.deviceId] = dev;
  devices.count++;
  stats.devices = devices.count;

  setupNewDevice(dev.deviceId, true);

  devices_flash_store.write(devices);

  wifiClient.println();
  wifiClient.print("{\"status\":\"ok\",\"device\":");
  getDevice(devices.devices[dev.deviceId]);
  wifiClient.println("}");
  wifiClient.println();
}

void checkAPStatus()
{
  if (runMode == RUN_MODE_AP)
  {
    if (status != WiFi.status())
    {
      status = WiFi.status();

      if (status == WL_AP_CONNECTED)
      {
        Serial.println(F("Urządzenie połączone z AP"));
      }
      else
      {
        Serial.println(F("Urządzenie rozłączone z AP"));
      }
    }
  }
}

bool configureNetwork()
{
  if (WiFi.status() == WL_NO_MODULE)
  {
    Serial.println(F("Nie udało się odnaleźć sprzętu Wi-Fi."));
    return false;
  }
  if (WiFi.firmwareVersion() < WIFI_FIRMWARE_LATEST_VERSION)
  {
    Serial.println(F("Zaktualizuj oprogramowanie modułu Wi-Fi."));
  }

  byte mac[6];
  WiFi.macAddress(mac);
  snprintf(stats.macStr, sizeof(stats.macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  serverConfig = conf_flash_store.read();
  authHeader = String(serverConfig.serverauth);

  if (!serverConfig.set)
  {
    if (SECRET_SSID != "")
    {
      String ssid = String(SECRET_SSID);
      String pass = String(SECRET_PASS);
      String serverauth = String(SECRET_SERVER_AUTH);
      writeConfig(ssid, pass, serverauth);
    }
    else
    {
      runMode = RUN_MODE_AP;
      runStatus = 0x2;

      WiFi.config(IPAddress(10, 10, 10, 1));

      String ssid = String(WS_AP_NAME_PREFIX) + stats.macStr;
      std::replace(ssid.begin(), ssid.end(), ':', '_');

      Serial.print(F("Uruchamiam AP: "));
      Serial.println(ssid);

      status = WiFi.beginAP(ssid.c_str());
      if (status != WL_AP_LISTENING)
      {
        Serial.println(F("Błąd uruchomienia w trybie AP!"));
        runStatus = 0x3;
        while (true)
        {
          showRunStatus();
        }
      }

      // wait 10 seconds for connection:
      delay(10000);
    }
  }

  if (runMode == RUN_MODE_SERVER)
  {
    for (int i = 0; i < 6; i++)
    {
      Serial.print(F("Próba połączenia z siecią Wi-Fi: "));
      Serial.println(serverConfig.ssid);
      status = WiFi.begin(serverConfig.ssid, serverConfig.pass);
      if (status == WL_CONNECTED)
      {
        if (!serverConfig.valid)
        {
          serverConfig.valid = true;
          conf_flash_store.write(serverConfig);
        }
        runStatus = 0x1;
        return true;
      }
      delay(5000);
    }

    // if not connected switch back to AP mode
    Serial.print(F("Próba połączenia z siecią Wi-Fi nieudana!"));

    if (!serverConfig.valid)
    {
      runMode = RUN_MODE_AP;
      serverConfig.set = false;
      conf_flash_store.write(serverConfig);
      runStatus = 0x3;
    }

    restart(true, 3000);
  }
  return true;
}

void factoryReset()
{
  pinMode(STATUS_PIN, INPUT_PULLUP);
  digitalWrite(STATUS_PIN, HIGH);
  delay(10);
  if (digitalRead(STATUS_PIN) == LOW)
  {
    delay(1000);
    if (digitalRead(STATUS_PIN) == LOW)
    {
      Serial.println(F("FACTORY RESET!"));
      serverConfig.set = false;
      conf_flash_store.write(serverConfig);
      pinout.set = false;
      pinout_flash_store.write(pinout);
      devices.set = false;
      devices_flash_store.write(devices);
      restart(true, 3000);
    }
  }
  
  pinMode(STATUS_PIN, OUTPUT);
  runStatus = 0x0;
  showRunStatus();
}

void getDevice(Device &dev)
{
  wifiClient.print("{\"id\":\"");
  wifiClient.print(dev.deviceId);
  wifiClient.print("\",\"active\":");
  wifiClient.print(dev.active == 0 ? "false" : "true");
  wifiClient.print(",\"type\":\"");
  wifiClient.print(deviceTypetoStr(dev.type));
  wifiClient.print("\",\"poll\":");
  wifiClient.print(dev.pollInterval);
  wifiClient.print(",\"callback\":\"");
  String callbackStr;
  WifiSensorsUtils::pushCallbackToString(dev.pushCallback, callbackStr);
  wifiClient.print(callbackStr);
  String configStr;
  WifiSensorsUtils::configToString(dev, configStr);
  wifiClient.print("\",\"config\":");
  wifiClient.print(configStr);
  wifiClient.print(",\"pins\":{");
  for (byte j = 0; j < WifiSensorsUtils::deviceRequirePins(dev.type); j++)
  {
    if (j > 0)
    {
      wifiClient.print(",");
    }
    String pinId = String("pin") + (j + 1);
    wifiClient.print("\"" + pinId + "\":{");
    DevicePin dpin = dev.pins[j];
    wifiClient.print("\"pin\":\"");
    wifiClient.print(dpin.type);
    wifiClient.print(dpin.pin);
    wifiClient.print("\",\"mode\":\"");
    wifiClient.print(pinModeToStr(dpin.mode));
    wifiClient.print("\"}");
  }
  wifiClient.print("},\"values\":{");
  for (byte j = 0; j < dev.valuesCount; j++)
  {
    if (j > 0)
    {
      wifiClient.print(",");
    }
    wifiClient.print("\"");
    wifiClient.print(devicesValues[dev.deviceId].names[j]);
    wifiClient.print("\":\"");
    wifiClient.print(devicesValues[dev.deviceId].values[j]);
    wifiClient.print("\"");
  }
  wifiClient.print("},\"units\":{");
  for (byte j = 0; j < dev.valuesCount; j++)
  {
    if (j > 0)
    {
      wifiClient.print(",");
    }
    wifiClient.print("\"");
    wifiClient.print(devicesValues[dev.deviceId].names[j]);
    wifiClient.print("\":\"");
    wifiClient.print(devicesValues[dev.deviceId].units[j]);
    wifiClient.print("\"");
  }
  wifiClient.print("}}");
}

void getDevices()
{
  wifiClient.print("{\"devices\":[");
  for (byte i = 0; i < devices.count; i++)
  {
    if (i > 0)
    {
      wifiClient.print(",");
    }
    getDevice(devices.devices[i]);
  }
  wifiClient.println("]}");
}

void getDevicesTypes()
{
  wifiClient.print("{\"types\":[");
  for (int i = 0; i != DEVICE_UNKNOWN; i++)
  {
    if (i > 0)
    {
      wifiClient.print(",");
    }

    DeviceType t = static_cast<DeviceType>(i);
    wifiClient.print("{\"name\":\"");
    wifiClient.print(deviceTypetoStr(t));
    wifiClient.print("\"}");
  }
  wifiClient.println("]}");
}

void getDevicesValues()
{
  wifiClient.print("{\"values\":{");
  for (byte i = 0; i < devices.count; i++)
  {
    if (i > 0)
    {
      wifiClient.print(",");
    }
    wifiClient.print("\"");
    wifiClient.print(devices.devices[i].deviceId);
    wifiClient.print("\":{");
    for (byte j = 0; j < devices.devices[i].valuesCount; j++)
    {
      if (j > 0)
      {
        wifiClient.print(",");
      }
      wifiClient.print("\"");
      wifiClient.print(devicesValues[devices.devices[i].deviceId].names[j]);
      wifiClient.print("\":\"");
      wifiClient.print(devicesValues[devices.devices[i].deviceId].values[j]);
      wifiClient.print("\"");
    }
    wifiClient.print("}");
  }
  wifiClient.println("}}");
}

void getPinout()
{
  wifiClient.print("{");
  for (int pin = 0; pin < WS_ANALOG_PINS; pin++)
  {
    if (pin > 0)
    {
      wifiClient.print(",");
    }
    wifiClient.print("\"A");
    wifiClient.print(pin);
    wifiClient.print("\":\"");
    wifiClient.print(pinModeToStr(pinout.analog[pin]));
    wifiClient.print("\"");
  }
  for (int pin = 2; pin < WS_DIGITAL_PINS; pin++)
  {
    wifiClient.print(",\"D");
    wifiClient.print(pin);
    wifiClient.print("\":\"");
    wifiClient.print(pinModeToStr(pinout.digital[pin]));
    wifiClient.print("\"");
  }
  wifiClient.println("}");
}

void getPinsValues()
{
  wifiClient.print("{");
  for (int pin = 0; pin < WS_ANALOG_PINS; pin++)
  {
    if (pin > 0)
    {
      wifiClient.print(",");
    }
    wifiClient.print("\"A");
    wifiClient.print(pin);
    wifiClient.print("\":");
    switch (pin)
    {
    case 0:
      wifiClient.print(analogRead(A0));
      break;
    case 1:
      wifiClient.print(analogRead(A1));
      break;
    case 2:
      wifiClient.print(analogRead(A2));
      break;
    case 3:
      wifiClient.print(analogRead(A3));
      break;
    case 4:
      wifiClient.print(analogRead(A4));
      break;
    case 5:
      wifiClient.print(analogRead(A5));
      break;
    case 6:
      wifiClient.print(analogRead(A6));
      break;
    case 7:
      wifiClient.print(analogRead(A7));
      break;
    }
  }
  for (int pin = 2; pin < WS_DIGITAL_PINS; pin++)
  {
    wifiClient.print(",\"D");
    wifiClient.print(pin);
    wifiClient.print("\":");
    wifiClient.print(digitalRead(pin));
  }
  wifiClient.println("}");
}

void handleInputDevices()
{
  then = millis();

  for (byte i = 0; i < devices.count; i++)
  {
    Device *dev = &(devices.devices[i]);

    if (!dev->active || dev->pollInterval == -1 || deviceIsOutput(dev->type))
    {
      continue;
    }
    if (dev->pollInterval == 0 || ((devicesValues[dev->deviceId].lastPoll + dev->pollInterval) < then))
    {
      devicesValues[dev->deviceId].lastPoll = then;
      switch (dev->type)
      {
      case DEVICE_BUTTON:
        stats.processingWarnings += readButton(dev, stats.lastWarning);
        break;
      case DEVICE_DHT22:
        stats.processingWarnings += readDHT22(dev, stats.lastWarning);
        break;
      case DEVICE_MOTION:
        stats.processingWarnings += readMotion(dev, stats.lastWarning);
        break;
      case DEVICE_SWITCH:
        stats.processingWarnings += readSwitch(dev, stats.lastWarning);
        break;
      case DEVICE_TEMP_DALLAS:
        stats.processingWarnings += readTempDallas(dev, stats.lastWarning);
        break;
      }
    }
  }

  unsigned long took = millis() - then;
  if (took > VALUES_PROCESSING_TIME)
  {
    stats.devicesProcessingThresold++;
    Serial.print(F("Devices processing time:"));
    Serial.println(took);
  }
}

bool handleDelete(HttpRequest &req)
{
  if (req.path == "/device")
  {
    String deviceId;
    if (!WifiSensorsUtils::readParam(req, "id", deviceId))
    {
      WifiSensorsUtils::sendHeader("200 OK", "application/json");
      WifiSensorsUtils::sendError("missing params: id");
      return true;
    }

    int id = deviceId.toInt();
    if (id >= devices.count)
    {
      WifiSensorsUtils::sendHeader("200 OK", "application/json");
      WifiSensorsUtils::sendError("device does not exist");
      return true;
    }

    devices.devices[id].active = false;
    devices_flash_store.write(devices);

    byte requiredPins = WifiSensorsUtils::deviceRequirePins(devices.devices[id].type);
    for (byte i = 0; i < requiredPins; i++)
    {
      unsetPinMode(devices.devices[id].pins[i]);
    }

    WifiSensorsUtils::sendHeader("200 OK", "application/json");
    WifiSensorsUtils::sendStatusOk();

    return true;
  }
  return false;
}

bool handleGet(HttpRequest &req)
{
  if (req.path == "/")
  {
    if (runMode == RUN_MODE_AP)
    {
      WifiSensorsUtils::sendHeader("200 OK", "text/html");
      WifiSensorsUtils::sendConfigHtml();
    }
    else
    {
      if (WifiSensorsUtils::statusAuthorizationForbidden(authHeader, req))
      {
        return true;
      }
      WifiSensorsUtils::sendHeader("200 OK", "application/json");
      wifiClient.println();
      getDevicesValues();
      wifiClient.println();
    }
    return true;
  }
  else if (req.path == "/devices")
  {
    if (WifiSensorsUtils::statusAuthorizationForbidden(authHeader, req))
    {
      return true;
    }
    WifiSensorsUtils::sendHeader("200 OK", "application/json");
    wifiClient.println();
    getDevices();
    wifiClient.println();
    return true;
  }
  else if (req.path == "/devicestypes")
  {
    if (WifiSensorsUtils::statusAuthorizationForbidden(authHeader, req))
    {
      return true;
    }
    WifiSensorsUtils::sendHeader("200 OK", "application/json");
    wifiClient.println();
    getDevicesTypes();
    wifiClient.println();
    return true;
  }
  else if (req.path == "/pinout")
  {
    if (WifiSensorsUtils::statusAuthorizationForbidden(authHeader, req))
    {
      return true;
    }
    WifiSensorsUtils::sendHeader("200 OK", "application/json");
    wifiClient.println();
    getPinout();
    wifiClient.println();
    return true;
  }
  else if (req.path == "/status")
  {
    WifiSensorsUtils::sendHeader("200 OK", "application/json");
    wifiClient.println();
    String status;
    WifiSensorsUtils::getStatusStr(status, &stats);
    wifiClient.println(status);
    wifiClient.println();
    return true;
  }
  else if (req.path == "/pinsvalues")
  {
    WifiSensorsUtils::sendHeader("200 OK", "application/json");
    wifiClient.println();
    getPinsValues();
  }

  return false;
}

bool handlePost(HttpRequest &req, String &payload)
{
  if (req.path == "/config")
  {
    if (WifiSensorsUtils::statusAuthorizationForbidden(authHeader, req))
    {
      return true;
    }
    String deviceId;
    if (!WifiSensorsUtils::readParam(req, "id", deviceId))
    {
      WifiSensorsUtils::sendHeader("200 OK", "application/json");
      WifiSensorsUtils::sendError("missing params: id");
      return true;
    }

    Hashtable<String, String> config;
    WifiSensorsUtils::parseConfigData(payload, &config);

    WifiSensorsUtils::sendHeader("200 OK", "application/json");
    if (deviceConfigUpdated(&config, &devices.devices[deviceId.toInt()]))
    {
      devices_flash_store.write(devices);
      WifiSensorsUtils::sendStatusOk();
    }

    return true;
  }
  else if (req.path == "/creds")
  {
    WifiSensorsUtils::sendHeader("200 OK", "text/html");
    wifiClient.println();
    wifiClient.println("<html><body>");

    int pos = payload.indexOf("ssid=");
    int pos2 = payload.indexOf("&");
    if (pos > -1)
    {
      pos = payload.indexOf("serverauth=");

      String ssid = payload.substring(5, pos2);
      String pass = payload.substring(pos2 + 6, pos - 1);
      String serverauth;
      if (pos > -1)
      {
        serverauth = payload.substring(pos + 11);
        serverauth.replace("+", " ");
      }

      writeConfig(ssid, pass, serverauth);

      wifiClient.println("Zapisano. Restart za 3 sekundy ...");
      wifiClient.println("</html></body>");
      wifiClient.println();

      runMode = RUN_MODE_SERVER;
      restart(true, 3000);
    }
    else
    {
      wifiClient.println("Bledne dane ...");
      wifiClient.println("</html></body>");
      wifiClient.println();
    }
    return true;
  }
  else if (req.path == "/device")
  {
    if (WifiSensorsUtils::statusAuthorizationForbidden(authHeader, req))
    {
      return true;
    }
    if (devices.count == WS_MAX_DEVICES)
    {
      WifiSensorsUtils::sendHeader("200 OK", "application/json");
      WifiSensorsUtils::sendError("maximum number of devices added");
      return true;
    }

    String deviceType;
    if (!WifiSensorsUtils::readParam(req, "type", deviceType))
    {
      WifiSensorsUtils::sendHeader("200 OK", "application/json");
      WifiSensorsUtils::sendError("missing params: type");
      return true;
    }

    String pinId;
    String pinM;
    Array<DevicePin, WS_MAX_DEVICE_PINS> pins;
    DeviceType type = deviceTypeFromStr(deviceType);
    byte requiredPins = WifiSensorsUtils::deviceRequirePins(type);
    for (byte i = 0; i < requiredPins; i++)
    {
      pinId = String("pin") + i;
      pinM = String("pin") + i + "type";
      if (WifiSensorsUtils::readParam(req, pinId.c_str(), pinId) && WifiSensorsUtils::readParam(req, pinM.c_str(), pinM))
      {
        DevicePin dpin;
        dpin.pin = pinId.substring(1).toInt();
        dpin.mode = pinModeFromStr(pinM);
        dpin.type = pinId.substring(0, 1).charAt(0);
        pins[i] = dpin;
      }
      else
      {
        WifiSensorsUtils::sendHeader("200 OK", "application/json");
        String msg = String("missing params: ") + pinId + "," + pinM;
        WifiSensorsUtils::WifiSensorsUtils::sendError(msg);
        return true;
      }
    }

    long pollInterval = 0L;
    String poll;
    if (WifiSensorsUtils::readParam(req, "interval", poll))
    {
      pollInterval = poll.toInt();
      if (pollInterval < 0)
      {
        WifiSensorsUtils::sendHeader("200 OK", "application/json");
        WifiSensorsUtils::sendError("interval < 0!");
        return true;
      }
    }

    Hashtable<String, String> config;
    WifiSensorsUtils::parseConfigData(payload, &config);
    addDevice(type, requiredPins, pins, pollInterval, &config);

    return true;
  }
  else if (req.path == "/pinout")
  {
    if (WifiSensorsUtils::statusAuthorizationForbidden(authHeader, req))
    {
      return true;
    }
    String pinId;
    String pinM;
    if (WifiSensorsUtils::readParam(req, "id", pinId) && WifiSensorsUtils::readParam(req, "mode", pinM))
    {
      DevicePin dpin;
      dpin.pin = pinId.substring(1).toInt();
      dpin.mode = pinModeFromStr(pinM);
      dpin.type = pinId.substring(0, 1).charAt(0);
      setPinMode(dpin);

      WifiSensorsUtils::sendHeader("200 OK", "application/json");
      WifiSensorsUtils::sendStatusOk();
    }
    else
    {
      WifiSensorsUtils::sendHeader("200 OK", "application/json");
      WifiSensorsUtils::sendError("missing params: id,type");
    }
    return true;
  }
  else if (req.path == "/set")
  {
    if (WifiSensorsUtils::statusAuthorizationForbidden(authHeader, req))
    {
      return true;
    }
    String pinId;
    if (WifiSensorsUtils::readParam(req, "id", pinId))
    {
      if (WifiSensorsUtils::pinUsedByDevice(pinout, pinId))
      {
        String msg = String("pin is used by device");
        WifiSensorsUtils::WifiSensorsUtils::sendError(msg);
        return true;
      }

      WifiSensorsUtils::setPinValue(pinId.charAt(0), pinId.substring(1).toInt(), HIGH);

      WifiSensorsUtils::sendHeader("200 OK", "application/json");
      WifiSensorsUtils::sendStatusOk();
    }
    else
    {
      WifiSensorsUtils::sendHeader("200 OK", "application/json");
      WifiSensorsUtils::sendError("missing params: id");
    }
    return true;
  }
  else if (req.path == "/turnoff")
  {
    if (WifiSensorsUtils::statusAuthorizationForbidden(authHeader, req))
    {
      return true;
    }
    String deviceId;
    if (WifiSensorsUtils::readParam(req, "id", deviceId))
    {
      byte id = deviceId.toInt();
      if (devices.devices[id].active && (devices.devices[id].type == DEVICE_RELAY || devices.devices[id].type == DEVICE_BUTTON))
      {
        DevicePin dpin = devices.devices[id].pins[0];
        devicesValues[id].values[0] = "off";

        if (devices.devices[id].config.bytes[DEVICE_CONFIG_BYTES_TRIGGER] == 0x0)
        {
          WifiSensorsUtils::setPinValue(dpin.type, dpin.pin, HIGH);
        }
        else
        {
          WifiSensorsUtils::setPinValue(dpin.type, dpin.pin, LOW);
        }

        WifiSensorsUtils::sendHeader("200 OK", "application/json");
        WifiSensorsUtils::sendStatusOk();
      }
      else
      {
        WifiSensorsUtils::sendHeader("200 OK", "application/json");
        WifiSensorsUtils::sendError("wrong device type");
      }
    }
    else
    {
      WifiSensorsUtils::sendHeader("200 OK", "application/json");
      WifiSensorsUtils::sendError("missing params: id");
    }
    return true;
  }
  else if (req.path == "/turnon")
  {
    if (WifiSensorsUtils::statusAuthorizationForbidden(authHeader, req))
    {
      return true;
    }
    String deviceId;
    if (WifiSensorsUtils::readParam(req, "id", deviceId))
    {
      byte id = deviceId.toInt();
      if (devices.devices[id].active && (devices.devices[id].type == DEVICE_RELAY || devices.devices[id].type == DEVICE_BUTTON))
      {
        DevicePin dpin = devices.devices[id].pins[0];
        devicesValues[id].values[0] = "on";
        if (devices.devices[id].config.bytes[DEVICE_CONFIG_BYTES_TRIGGER] == 0x0)
        {
          WifiSensorsUtils::setPinValue(dpin.type, dpin.pin, LOW);
        }
        else
        {
          WifiSensorsUtils::setPinValue(dpin.type, dpin.pin, HIGH);
        }

        WifiSensorsUtils::sendHeader("200 OK", "application/json");
        WifiSensorsUtils::sendStatusOk();
      }
      else
      {
        WifiSensorsUtils::sendHeader("200 OK", "application/json");
        WifiSensorsUtils::sendError("wrong device type");
      }
    }
    else
    {
      WifiSensorsUtils::sendHeader("200 OK", "application/json");
      WifiSensorsUtils::sendError("missing params: id");
    }
    return true;
  }
  else if (req.path == "/unset")
  {
    if (WifiSensorsUtils::statusAuthorizationForbidden(authHeader, req))
    {
      return true;
    }
    String pinId;
    if (WifiSensorsUtils::readParam(req, "id", pinId))
    {
      if (WifiSensorsUtils::pinUsedByDevice(pinout, pinId))
      {
        String msg = String("pin is used by device");
        WifiSensorsUtils::WifiSensorsUtils::sendError(msg);
        return true;
      }

      WifiSensorsUtils::setPinValue(pinId.charAt(0), pinId.substring(1).toInt(), LOW);

      WifiSensorsUtils::sendHeader("200 OK", "application/json");
      WifiSensorsUtils::sendStatusOk();
    }
    else
    {
      WifiSensorsUtils::sendHeader("200 OK", "application/json");
      WifiSensorsUtils::sendError("missing params: id");
    }
    return true;
  }

  return false;
}

void handleMemory()
{
  stats.freeMem = WifiSensorsUtils::memoryFree();
  if (stats.freeMem < LOW_MEMORY)
  {
    stats.processingWarnings++;
    stats.lastWarning = "Low memory: ";
    stats.lastWarning += stats.freeMem;
    stats.lastWarning += " ";
    stats.lastWarning += millis();
    Serial.print(F("Low memory: "));
    Serial.println(stats.freeMem);
  }
}

void handleResponse(String &resp)
{
  if (resp.indexOf(" 200 ") < 0)
  {
    stats.processingWarnings++;
    stats.lastWarning = "Request error: ";
    stats.lastWarning += resp;
    stats.lastWarning += " ";
    stats.lastWarning += millis();
  }
}

void handleSerwer()
{
  then = millis();
  wifiClient = server.available();
  if (wifiClient)
  {
    currentLine = "";
    requestPath = "";
    HttpRequest req;
    byte headerCnt = 0;

    while (wifiClient.connected())
    {
      if (wifiClient.available())
      {
        char c = wifiClient.read();
#if DEBUG
        Serial.write(c);
#endif
        if (c == '\n')
        {
          // that's the end of the wifiClient HTTP request, so send a response:
          if (currentLine.length() == 0)
          {
            if (requestPath.startsWith("HTTP/"))
            {
              Serial.print(F("Got response: "));
              Serial.println(requestPath);
              handleResponse(requestPath);
            }
            else
            {
              Serial.print(F("Handling: "));
              Serial.println(requestPath);

              WifiSensorsUtils::parseRequestString(requestPath, req);

              bool served = false;
              if (req.method == "GET")
              {
                served = handleGet(req);
              }
              else if (req.method == "POST")
              {
                String payload;
                WifiSensorsUtils::readPayloadData(payload);
                served = handlePost(req, payload);
              }
              else if (req.method == "DELETE")
              {
                served = handleDelete(req);
              }

              if (!served)
              {
                WifiSensorsUtils::sendStatusForbidden();
              }
            }

            delay(10);
            wifiClient.stop();
            break;
          }
          else
          {
            currentLine = "";
          }
        }
        else if (c != '\r')
        {
          currentLine += c;
        }
        else
        {
          if (currentLine.length() > 0)
          {
            if (currentLine.startsWith("Authorization:"))
            {
              req.headersNames[headerCnt] = "Authorization";
              req.headersValues[headerCnt] = currentLine.substring(15);
              headerCnt++;
            }
          }
        }

        if (currentLine.startsWith("GET /") || currentLine.startsWith("POST /") || currentLine.startsWith("PUT /") || currentLine.startsWith("HEAD /") || currentLine.startsWith("DELETE /"))
        {
          requestPath = currentLine;
        }
        else if (currentLine.startsWith("HTTP/")) //response header
        {
          requestPath = currentLine;
        }
      }
    }
  }

  unsigned long took = millis() - then;
  if (took > SERVER_PROCESSING_TIME)
  {
    String msg = "Server processing time: ";
    msg += took;
    Serial.println(msg);
  }
}

void restart(bool set, long rdelay)
{
  if (restatPending)
  {
    restatPending = false;
    WiFi.end();
    Serial.println("Restart...");
    delay(3000);
    runStatus = 0x0;
    setupServer();
  }

  if (set)
  {
    delay(rdelay);
    restatPending = true;
  }
}

void setPinMode(DevicePin &pin)
{
  if (pin.type == 'D')
  {
    pinout.used[pin.pin] = true;
    pinout.digital[pin.pin] = pin.mode;
    switch (pin.mode)
    {
    case 0:
      pinMode(pin.pin, INPUT);
      break;
    case 1:
      pinMode(pin.pin, OUTPUT);
      break;
    case 2:
      pinMode(pin.pin, INPUT_PULLUP);
      break;
    }
  }
  else
  {
    pinout.used[WS_DIGITAL_PINS + pin.pin] = true;
    pinout.analog[pin.pin] = pin.mode;
    WifiSensorsUtils::setAnalogPinMode(pin.pin, pin.mode);
  }

  pinout.set = true;
  pinout_flash_store.write(pinout);
}

void setupDevices()
{
  Serial.println("Setup devices");
  devices = devices_flash_store.read();
  if (!devices.set)
  {
    devices.set = true;
    devices.count = 0;
    devices_flash_store.write(devices);
  }

  stats.devices = devices.count;
  for (byte i = 0; i < devices.count; i++)
  {
    setupNewDevice(i, false);
  }
}

void setupNewDevice(byte deviceId, bool update)
{
  Device *dev = &(devices.devices[deviceId]);

  devicesValues[deviceId].lastPoll = 0L;
  devices.devices[deviceId].valuesCount = deviceValuesNames(dev->type, dev->deviceId);

  Serial.print(F("Setup: "));
  Serial.print(dev->deviceId);
  Serial.print(F(" -> "));
  Serial.print(deviceTypetoStr(dev->type));
  Serial.print(F(" -> "));
  Serial.println(dev->pins[0].pin);

  byte requiredPins = WifiSensorsUtils::deviceRequirePins(dev->type);
  for (byte i = 0; i < requiredPins; i++)
  {
    setPinMode(dev->pins[i]);
  }

  switch (dev->type)
  {
  case DEVICE_BUTTON:
    setupButton(dev);
    break;
  case DEVICE_DHT22:
    setupDHT22(dev);
    break;
  case DEVICE_MOTION:
    setupButton(dev);
    break;
  case DEVICE_RELAY:
    setupRelay(dev);
    break;
  case DEVICE_SWITCH:
    setupButton(dev);
    break;
  case DEVICE_TEMP_DALLAS:
    setupTempDallas(dev);
    break;
  }

  if (update)
  {
    devices_flash_store.write(devices);
  }
}

void setupPins()
{
  Serial.println("Setup pins");
  pinout = pinout_flash_store.read();
  if (!pinout.set)
  {
    for (int pin = 0; pin < WS_ANALOG_PINS; pin++)
    {
      pinout.analog[pin] = 0;
      WifiSensorsUtils::setAnalogPinMode(pin, 0);
    }
    for (int pin = 2; pin < WS_DIGITAL_PINS; pin++)
    {
      pinout.digital[pin] = 0;
      pinMode(pin, INPUT);
    }

    pinout.set = true;
    pinout_flash_store.write(pinout);
  }
  else
  {
    for (int pin = 0; pin < WS_ANALOG_PINS; pin++)
    {
      WifiSensorsUtils::setAnalogPinMode(pin, pinout.analog[pin]);
    }
    for (int pin = 2; pin < WS_DIGITAL_PINS; pin++)
    {
      switch (pinout.digital[pin])
      {
      case 0:
        pinMode(pin, INPUT);
        break;
      case 1:
        pinMode(pin, OUTPUT);
        break;
      case 2:
        pinMode(pin, INPUT_PULLUP);
        break;
      }
    }
  }
}

void setupSerial()
{
  Serial.begin(9600);
#if WAIT_FOR_SERIAL
  while (!Serial)
  {
    // blank
  }
#endif
}

void setupServer()
{
  Serial.println("Setup server");
  if (!configureNetwork())
  {
    Serial.println("Błąd wifi");
    runStatus = 0x3;
    while (true)
    {
      showRunStatus();
    }
  }

  server.begin();

  WifiSensorsUtils::printWifiStatus(&stats);
}

void showRunStatus()
{
  if (runStatus == 0x0)
  {
    digitalWrite(STATUS_PIN, LOW);
  }
  else if (runStatus == 0x1)
  {
    digitalWrite(STATUS_PIN, HIGH);
  }
  else if (runStatus == 0x2)  //AP MODE
  {
    digitalWrite(STATUS_PIN, LOW);
    delay(200);
    digitalWrite(STATUS_PIN, HIGH);
  }
  else if (runStatus == 0x3)  //ERROR
  {
    digitalWrite(STATUS_PIN, LOW);
    delay(500);
    digitalWrite(STATUS_PIN, HIGH);
  }
}

void writeConfig(String &ssid, String &pass, String &serverauth)
{
  serverConfig.set = true;
  serverConfig.valid = false;
  memset(serverConfig.ssid, 0, sizeof(serverConfig.ssid));
  strncpy(serverConfig.ssid, ssid.c_str(), strlen(ssid.c_str()));
  memset(serverConfig.pass, 0, sizeof(serverConfig.pass));
  strncpy(serverConfig.pass, pass.c_str(), strlen(pass.c_str()));
  memset(serverConfig.serverauth, 0, sizeof(serverConfig.serverauth));
  strncpy(serverConfig.serverauth, serverauth.c_str(), strlen(serverauth.c_str()));
  authHeader = serverauth;

  conf_flash_store.write(serverConfig);
  Serial.print("Zapisano SSID: ");
  Serial.println(serverConfig.ssid);
}

void unsetPinMode(DevicePin &pin)
{
  if (pin.type == 'D')
  {
    pinout.used[pin.pin] = false;
  }
  else
  {
    pinout.used[WS_DIGITAL_PINS + pin.pin] = false;
  }
  pinout_flash_store.write(pinout);
}
