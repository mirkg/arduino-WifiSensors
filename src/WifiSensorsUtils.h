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

  static bool isCallbackUrlValid(Hashtable<String, String> *config, Callback &callback);

  static int memoryFree();

  static void parseConfigData(String &payload, Hashtable<String, String> *config);

  static void parseParam(String &s, byte cnt, HttpRequest &req);

  static void parseRequestString(String &s, HttpRequest &req);

  static bool pinUsedByDevice(Pinout &pinout, String &pinId);

  static void prepareCallbackValues(char *raw, String &value1, String &path, String &value0Name);

  static void prepareCallbackValues(char *raw, String &value1, String &value2, String &path, String &value0Name, String &value1Name);

  static void processWarning(Callback &callback, String &msg);

  static void pushCallbackToString(Callback &callback, String &str);

  static void printWifiStatus(ServerStats *stats);

  static bool readParam(HttpRequest &req, const char *name, String &value);

  static void readPayloadData(String &payload);

  static void sendChallenge();

  static void sendConfigHtml();

  static void sendError(const char *msg);

  static void sendError(String &msg)
  {
    sendError(msg.c_str());
  }

  static void sendHeader(const char *code, const char *contentType);

  static byte sendHttpRequest(Callback &callback, String path);

  static void sendStatusOk();

  static void sendStatusForbidden();

  static bool sendLoginChallange(String &serverauth, HttpRequest &req);

  static void serverConfigToString(ServerConfig &serverConfig, String &str);

  static void setAnalogPinMode(int pin, int mode);

  static void setPinValue(char pinType, int pin, byte value);

  static bool statusAuthorizationForbidden(String &serverauth, HttpRequest &req);
};

#endif
