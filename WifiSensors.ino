
#define DEBUG 0
#define WAIT_FOR_SERIAL 0
#define LOW_MEMORY_RESTART 1

#define WS_AP_NAME_PREFIX "ARDUINO_"
#define WS_SERVER_PORT 80
#define VALUES_PROCESSING_TIME 50
#define SERVER_PROCESSING_TIME 100
#define LOW_MEMORY 2000
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
volatile RunStatus runStatus = RUN_STATUS_BOOT;
volatile bool runStatuChanged = false;
int status = WL_IDLE_STATUS;
bool restatPending = false;
unsigned long then;
String currentLine;
String requestPath;

Pinout pinout;
Devices devices;
Array<DevicesValues, WS_MAX_DEVICES> devicesValues;

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

  if (!WifiSensorsUtils::isCallbackUrlValid(config, dev.pushCallback, true))
  {
    return;
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
  WifiSensorsUtils::sendDevice(devices.devices[dev.deviceId], devicesValues[dev.deviceId], false);
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
      Callback callback;
      callback.set = false;
      WifiSensorsUtils::writeServerConfig(serverConfig, ssid, pass, serverauth, callback);
      authHeader = serverauth;
      conf_flash_store.write(serverConfig);
    }
    else
    {
      runMode = RUN_MODE_AP;
      setRunStatus(RUN_STATUS_AP_MODE);

      WiFi.config(IPAddress(10, 10, 10, 1));

      String ssid = String(WS_AP_NAME_PREFIX) + stats.macStr;
      std::replace(ssid.begin(), ssid.end(), ':', '_');

      Serial.print(F("Uruchamiam AP: "));
      Serial.println(ssid);

      status = WiFi.beginAP(ssid.c_str());
      if (status != WL_AP_LISTENING)
      {
        Serial.println(F("Błąd uruchomienia w trybie AP!"));
        setRunStatus(RUN_STATUS_ERROR);
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
        setRunStatus(RUN_STATUS_OK);
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
      setRunStatus(RUN_STATUS_ERROR);
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
  setRunStatus(RUN_STATUS_BOOT);
  showRunStatus();
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
      byte warnCnt = 0;
      switch (dev->type)
      {
      case DEVICE_BUTTON:
        warnCnt += readButton(dev, stats.lastWarning);
        break;
      case DEVICE_DHT22:
        warnCnt += readDHT22(dev, stats.lastWarning);
        break;
      case DEVICE_GENERIC_ANALOG_INPUT:
        warnCnt += readAnalog(dev, stats.lastWarning);
        break;
      case DEVICE_GENERIC_DIGITAL_INPUT:
        warnCnt += readDigital(dev, stats.lastWarning);
        break;
      case DEVICE_MOTION:
        warnCnt += readMotion(dev, stats.lastWarning);
        break;
      case DEVICE_SWITCH:
        warnCnt += readSwitch(dev, stats.lastWarning);
        break;
      case DEVICE_TEMP_DALLAS:
        warnCnt += readTempDallas(dev, stats.lastWarning);
        break;
      }

      if (warnCnt > 0)
      {
        stats.processingWarnings += warnCnt;
        WifiSensorsUtils::processWarning(serverConfig.callback, stats.lastWarning);
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
      WifiSensorsUtils::unsetPinMode(pinout, devices.devices[id].pins[i]);
    }
    pinout_flash_store.write(pinout);

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
      sendDevicesValues();
      wifiClient.println();
    }
    return true;
  }
  else if (req.path == "/backup")
  {
    if (WifiSensorsUtils::statusAuthorizationForbidden(authHeader, req))
    {
      return true;
    }

    wifiClient.println("HTTP/1.1 200 OK");
    wifiClient.println("Content-Type: application/octet-stream");
    wifiClient.println("Content-Disposition: attachment; filename=backup.bin");
    wifiClient.println();
    WifiSensorsUtils::sendBackup(serverConfig, devices, devicesValues);
    wifiClient.println();
    return true;
  }
  else if (req.path == "/config")
  {
    if (WifiSensorsUtils::statusAuthorizationForbidden(authHeader, req))
    {
      return true;
    }
    WifiSensorsUtils::sendHeader("200 OK", "application/json");
    wifiClient.println();
    String config;
    WifiSensorsUtils::serverConfigToString(serverConfig, config);
    wifiClient.println(config);
    wifiClient.println();
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
    WifiSensorsUtils::sendDevices(devices, devicesValues, true, false);
    wifiClient.println();
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
    WifiSensorsUtils::sendDevicesTypes();
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
    WifiSensorsUtils::sendPinout(pinout);
    wifiClient.println();
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
    WifiSensorsUtils::sendPinsValues();
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
    WifiSensorsUtils::sendHeader("200 OK", "application/json");

    Hashtable<String, String> config;
    WifiSensorsUtils::parseConfigFromPayload(payload, &config);

    String deviceId;
    if (!WifiSensorsUtils::readParam(req, "id", deviceId))
    {
      if (handleServerConfig(&config))
      {
        WifiSensorsUtils::sendStatusOk();
      }
      else
      {
        WifiSensorsUtils::sendError("Server config invalid!");
      }
    }
    else
    {
      if (WifiSensorsUtils::isCallbackUrlValid(&config, devices.devices[deviceId.toInt()].pushCallback, true) && deviceConfigUpdated(&config, &devices.devices[deviceId.toInt()]))
      {
        devices_flash_store.write(devices);
        WifiSensorsUtils::sendStatusOk();
      }
      else
      {
        WifiSensorsUtils::sendError("Device config invalid!");
      }
    }

    return true;
  }
  else if (req.path == "/creds")
  {
    WifiSensorsUtils::sendHeader("200 OK", "text/html");
    wifiClient.println();
    wifiClient.println("<html><body>");

    Hashtable<String, String> config;
    WifiSensorsUtils::parseConfigFromPayload(payload, &config);

    if (handleServerConfig(&config))
    {
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
    WifiSensorsUtils::parseConfigFromPayload(payload, &config);
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
      WifiSensorsUtils::setPinMode(pinout, dpin);
      pinout_flash_store.write(pinout);

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
  else if (req.path == "/restore")
  {
    if (WifiSensorsUtils::statusAuthorizationForbidden(authHeader, req))
    {
      return true;
    }

    WifiSensorsUtils::sendHeader("200 OK", "application/json");
    for (byte i = 0; i < WS_DIGITAL_PINS + WS_ANALOG_PINS; i++)
    {
      pinout.used[i] = false;
    }
    if (WifiSensorsUtils::restoreBackup(serverConfig, pinout, devices, devicesValues, payload, authHeader))
    {
      stats.devices = devices.count;
      conf_flash_store.write(serverConfig);
      pinout_flash_store.write(pinout);
      devices_flash_store.write(devices);
      WifiSensorsUtils::sendStatusOk();
    }
    else
    {
      WifiSensorsUtils::sendError("Backup invalid!");
      // restart to re-read previous config
      restart(true, 100);
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
    WifiSensorsUtils::processWarning(serverConfig.callback, stats.lastWarning);

#if LOW_MEMORY_RESTART
    // try restart to cleanup memory
    restart(true, 10000);
#endif
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
    WifiSensorsUtils::processWarning(serverConfig.callback, stats.lastWarning);
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
        else if (currentLine.startsWith("HTTP/")) // response header
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

bool handleServerConfig(Hashtable<String, String> *config)
{
  Callback callback;
  if (WifiSensorsUtils::isCallbackUrlValid(config, callback, true) && config->containsKey("ssid"))
  {
    String ssid = *config->get("ssid");
    String pass;
    if (config->containsKey("pass"))
    {
      pass = *config->get("pass");
    }
    String serverauth;
    if (config->containsKey("serverauth"))
    {
      serverauth = *config->get("serverauth");
      serverauth.replace("+", " ");
    }

    WifiSensorsUtils::writeServerConfig(serverConfig, ssid, pass, serverauth, callback);
    conf_flash_store.write(serverConfig);
    authHeader = serverauth;

    return true;
  }
  return false;
}

void restart(bool set, long rdelay)
{
  if (restatPending)
  {
    restatPending = false;
    WiFi.end();
    Serial.println("Restart...");
    delay(3000);
    setRunStatus(RUN_STATUS_BOOT);
    setupServer();
  }

  if (set)
  {
    delay(rdelay);
    restatPending = true;
  }
}

void sendDevicesValues()
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

void setRunStatus(RunStatus status)
{
  runStatus = status;
  runStatuChanged = true;
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
    WifiSensorsUtils::setPinMode(pinout, dev->pins[i]);
  }
  pinout_flash_store.write(pinout);

  switch (dev->type)
  {
  case DEVICE_BUTTON:
    setupButton(dev);
    break;
  case DEVICE_DHT22:
    setupDHT22(dev);
    break;
  case DEVICE_GENERIC_ANALOG_INPUT:
  case DEVICE_GENERIC_DIGITAL_INPUT:
    setupGeneric(dev);
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
    setRunStatus(RUN_STATUS_ERROR);
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
  if (!runStatuChanged)
  {
    return;
  }

  runStatuChanged = false;
  switch (runStatus)
  {
  case RUN_STATUS_BOOT:
    digitalWrite(STATUS_PIN, LOW);
    break;
  case RUN_STATUS_OK:
    digitalWrite(STATUS_PIN, HIGH);
    break;
  case RUN_STATUS_AP_MODE:
    digitalWrite(STATUS_PIN, LOW);
    delay(200);
    digitalWrite(STATUS_PIN, HIGH);
    break;
  case RUN_STATUS_ERROR:
    digitalWrite(STATUS_PIN, LOW);
    delay(500);
    digitalWrite(STATUS_PIN, HIGH);
    break;
  }
}
