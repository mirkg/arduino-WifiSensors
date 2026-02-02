#ifndef WIFISENSORS_UTILS_H
#define WIFISENSORS_UTILS_H

#include "WifiSensorsTypes.h"
#include "parsers.h"

#include <Hashtable.h>
#include <WiFiNINA.h>

class WifiSensorsUtils
{
public:
  static float adjustPercent(float original, float adjustment);

  static void configToString(Device &dev, String &str);

  static byte deviceRequirePins(DeviceType type);

  static void digitalWriteAnalogPin(int pin, byte value);

  static void getStatusStr(String &str, ServerStats *stats);

  static bool isCallbackUrlValid(Hashtable<String, String> *config, Callback &callback, bool needDecode);

  static int memoryFree();

  static void parseConfigFromJson(String &json, Hashtable<String, String> *config);

  static void parseConfigFromPayload(String &payload, Hashtable<String, String> *config);

  static void parseParam(String &s, byte cnt, HttpRequest &req);

  static void parseRequestString(String &s, HttpRequest &req);

  static bool pinUsedByDevice(Pinout &pinout, String &pinId);

  static void prepareCallbackValues(char *raw, String &value1, String &path, String &value0Name);

  static void prepareCallbackValues(char *raw, String &value1, String &value2, String &path, String &value0Name, String &value1Name);

  static void processWarning(Callback &callback, ServerStats &stats);

  static void pushCallbackToString(Callback &callback, String &str);

  static void printWifiStatus(ServerStats *stats);

  static bool readParam(HttpRequest &req, const char *name, String &value);

  static void readPayloadData(String &payload);

  static bool restoreBackup(ServerConfig &serverConfig, Pinout &pinout, Devices &devices, Array<DevicesValues, WS_MAX_DEVICES> &devicesValues, String &str, String &authHeader);

  static void sendBackup(ServerConfig &serverConfig, Devices &devices, Array<DevicesValues, WS_MAX_DEVICES> &devicesValues);

  static bool restoreDevice(Devices &devices, Array<DevicesValues, WS_MAX_DEVICES> &devicesValues, Pinout &pinout, String str);

  static bool restoreDevicesConf(Devices &devices, Array<DevicesValues, WS_MAX_DEVICES> &devicesValues, Pinout &pinout, String str);

  static bool restoreServerConf(ServerConfig &serverConfig, String str, String &authHeader);

  static void sendChallenge();

  static void sendConfigHtml();

  static void sendDevice(Device &dev, DevicesValues &values, bool callbackAuth);

  static void sendDevices(Devices &devices, Array<DevicesValues, WS_MAX_DEVICES> &devicesValues, bool jsonPrefix, bool callbackAuth);

  static void sendDevicesTypes();

  static void sendError(const char *msg);

  static void sendError(String &msg)
  {
    sendError(msg.c_str());
  }

  static void sendHeader(const char *code, const char *contentType);

  static byte sendHttpRequest(Callback &callback, String path);

  static void sendPinout(Pinout &pinout);

  static void sendPinsValues();

  static void sendStatusOk();

  static void sendStatusForbidden();

  static bool sendLoginChallange(String &serverauth, HttpRequest &req);

  static void serverConfigToString(ServerConfig &serverConfig, String &str);

  static void setAnalogPinMode(int pin, int mode);

  static void setPinMode(Pinout &pinout, DevicePin &pin);

  static void setPinValue(char pinType, int pin, byte value);

  static bool statusAuthorizationForbidden(String &serverauth, HttpRequest &req);

  static void unsetPinMode(Pinout &pinout, DevicePin &pin);

  static int waitForWiFiStatus();

  static void writeServerConfig(ServerConfig &serverConfig, String &ssid, String &pass, String &serverauth, Callback &callback);
};

#endif
