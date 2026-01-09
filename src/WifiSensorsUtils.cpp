
#include "WifiSensorsUtils.h"

#define DEBUG 0

extern WiFiClient wifiClient;

#ifdef __arm__
extern "C" char *sbrk(int incr); // Wywołaj z argumentem 0, aby otrzymać początkowy adres wolnej pamięci
#else
extern int *__brkval; // Wskaźnik na ostatni zapisany adres kopca (lub 0)
#endif

float WifiSensorsUtils::adjustPercent(float original, float adjustment)
{
  if (adjustment == 0.0)
  {
    return original;
  }
  return original += adjustment * original;
}

void WifiSensorsUtils::configToString(Device &dev, String &str)
{
  str = "{";
  switch (dev.type)
  {
  case DEVICE_BUTTON:
    str += "\"bounce\":";
    str += dev.config.ints[DEVICE_CONFIG_INTS_DEBOUNCE];
    break;
  case DEVICE_DHT22:
    str += "\"humid_adj\":";
    str += dev.config.floats[DEVICE_CONFIG_FLOAT_HUMID_ADJ];
    str += ",\"temp_adj\":";
    str += dev.config.floats[DEVICE_CONFIG_FLOAT_TEMP_ADJ];
    break;
  case DEVICE_GENERIC_ANALOG_INPUT:
    str += "\"min\":";
    str += dev.config.floats[DEVICE_CONFIG_FLOAT_MIN];
    str += ",\"max\":";
    str += dev.config.floats[DEVICE_CONFIG_FLOAT_MAX];
    break;
  case DEVICE_MOTION:
    str += "\"bounce\":";
    str += dev.config.ints[DEVICE_CONFIG_INTS_DEBOUNCE];
    break;
  case DEVICE_RELAY:
    str += "\"trigger\":";
    str += dev.config.bytes[DEVICE_CONFIG_BYTES_TRIGGER] == 0x0 ? "\"LOW\"" : "\"HIGH\"";
    break;
  case DEVICE_SWITCH:
    str += "\"bounce\":";
    str += dev.config.ints[DEVICE_CONFIG_INTS_DEBOUNCE];
    break;
  case DEVICE_TEMP_DALLAS:
    str += "\"temp_adj\":";
    str += dev.config.floats[DEVICE_CONFIG_FLOAT_TEMP_ADJ];
    break;
  }
  str += "}";
}

byte WifiSensorsUtils::deviceRequirePins(DeviceType type)
{
  switch (type)
  {
  case DEVICE_BUTTON:
  case DEVICE_DHT22:
  case DEVICE_GENERIC_ANALOG_INPUT:
  case DEVICE_GENERIC_DIGITAL_INPUT:
  case DEVICE_MOTION:
  case DEVICE_RELAY:
  case DEVICE_SWITCH:
  case DEVICE_TEMP_DALLAS:
    return 1;
  }
  return 0;
}

void WifiSensorsUtils::digitalWriteAnalogPin(int pin, byte value)
{
  switch (pin)
  {
  case 0:
    digitalWrite(A0, value);
    break;
  case 1:
    digitalWrite(A1, value);
    break;
  case 2:
    digitalWrite(A2, value);
    break;
  case 3:
    digitalWrite(A3, value);
    break;
  case 4:
    digitalWrite(A4, value);
    break;
  case 5:
    digitalWrite(A5, value);
    break;
  case 6:
    digitalWrite(A6, value);
    break;
  case 7:
    digitalWrite(A7, value);
    break;
  }
}

void WifiSensorsUtils::getStatusStr(String &str, ServerStats *stats)
{
  str += "{";
  str += "\"version\":\"";
  str += WS_VERSION;
  str += "\",\"mac\":\"";
  str += stats->macStr;
  str += "\",\"ssid\":\"";
  str += WiFi.SSID();
  str += "\",\"ip\":\"";
  str += WiFi.localIP().toString();
  str += "\",\"rssi\":\"";
  str += WiFi.RSSI();
  str += "\",\"memory\":";
  str += stats->freeMem;
  str += ",\"devices\":";
  str += stats->devices;
  str += ",\"devices_slow_process\":";
  str += stats->devicesProcessingThresold;
  str += ",\"warnings\":";
  str += stats->processingWarnings;
  str += ",\"last_warn\":\"";
  str += stats->lastWarning;
  str += "\",\"now\":";
  str += millis();
  str += "}";
}

void WifiSensorsUtils::parseConfigData(String &payload, Hashtable<String, String> *config)
{
#if DEBUG
      Serial.println(F("PAYLOAD"));
#endif

  int pos1, pos2;
  String tmp;
  do
  {
    pos1 = payload.indexOf("&");
    if (pos1 > -1)
    {
      tmp = payload.substring(0, pos1);
      payload = payload.substring(pos1 + 1);
    }
    else
    {
      tmp = payload;
    }

    pos2 = tmp.indexOf('=');
    if (pos2 > -1)
    {
      config->put(tmp.substring(0, pos2), tmp.substring(pos2 + 1));
#if DEBUG
      Serial.print(tmp.substring(0, pos2));
      Serial.print("=");
      Serial.println(tmp.substring(pos2 + 1));
#endif
    }
  } while (pos1 > -1);
}

int WifiSensorsUtils::memoryFree()
{
  int freeValue; // Ostatni umieszczony na stosie obiekt
#ifdef __arm__
  freeValue = &freeValue - reinterpret_cast<int *>(sbrk(0));
#else
  if ((int)__brkval == 0) // Kopiec jest pusty, więc użyj początku kopca
  {
    freeValue = ((int)&freeValue) - ((int)__malloc_heap_start);
  }
  else // Kopiec nie jest pusty, więc użyj ostatniego adresu kopca
  {
    freeValue = ((int)&freeValue) - ((int)__brkval);
  }
#endif
  return freeValue;
}

void WifiSensorsUtils::parseParam(String &s, byte cnt, HttpRequest &req)
{
  int pos = s.indexOf('=');
  if (pos > -1)
  {
    req.paramsNames[cnt] = s.substring(0, pos);
    req.paramsValues[cnt] = s.substring(pos + 1);
  }
}

void WifiSensorsUtils::parseRequestString(String &s, HttpRequest &req)
{
  int pos = s.indexOf(' ');
  if (pos > -1)
  {
    req.method = s.substring(0, pos);
    s = s.substring(pos + 1);
    pos = s.indexOf(' ');
    if (pos > -1)
    {
      s = s.substring(0, pos);
      pos = s.indexOf('?');
      if (pos > -1)
      {
        req.path = s.substring(0, pos);
        s = s.substring(pos + 1);
        s = decode(s);
        byte j = 0;
        do
        {
          pos = s.indexOf('&');
          if (pos > -1)
          {
            String tmp = s.substring(0, pos);
            parseParam(tmp, j, req);
            s = s.substring(pos + 1);
          }
          else
          {
            parseParam(s, j, req);
          }

          j++;
        } while (pos > -1 && j < 8);
      }
      else
      {
        req.path = s;
      }
    }
  }
}

bool WifiSensorsUtils::pinUsedByDevice(Pinout &pinout, String &pinId)
{
  DevicePin dpin;
  dpin.pin = pinId.substring(1).toInt();
  dpin.type = pinId.substring(0, 1).charAt(0);

  if ((dpin.type == 'D' && pinout.used[dpin.pin]) || (dpin.type == 'A' && pinout.used[WS_DIGITAL_PINS + dpin.pin]))
  {
    return true;
  }
  return false;
}

void WifiSensorsUtils::printWifiStatus(ServerStats *stats)
{
  Serial.print(F("MAC: "));
  Serial.println(stats->macStr);

  Serial.print(F("SSID: "));
  Serial.println(WiFi.SSID());

  Serial.print(F("IP Address: "));
  Serial.println(WiFi.localIP());

  Serial.print(F("signal strength (RSSI):"));
  Serial.print(WiFi.RSSI());
  Serial.println(F(" dBm"));
}

void WifiSensorsUtils::prepareCallbackValues(char *raw, String &value1, String &path, String &value0Name)
{
  path = String(raw);
  String replString = String("<") + value0Name + ">";
  String strValue1 = String(value1);
  strValue1 = encode(strValue1);
  path.replace(replString, strValue1);
}

void WifiSensorsUtils::prepareCallbackValues(char *raw, String &value1, String &value2, String &path, String &value0Name, String &value1Name)
{
  path = String(raw);
  String strValue1 = String(value1);
  String replString = String("<") + value0Name + ">";
  strValue1 = encode(strValue1);
  path.replace(replString, strValue1);
  String strValue2 = String(value2);
  replString = String("<") + value1Name + ">";
  strValue2 = encode(strValue2);
  path.replace(replString, strValue2);
}

void WifiSensorsUtils::processWarning(Callback &callback, String &msg)
{
  if (callback.set)
  {
    String path;
    static String msgTemplate = "msg";
    prepareCallbackValues(callback.path, msg, path, msgTemplate);
    if (!sendHttpRequest(callback, path))
    {
      Serial.println(F("Process warning failed!"));
    }
  }
}

void WifiSensorsUtils::pushCallbackToString(Callback &callback, String &str)
{
  if (callback.set)
  {
    str += String(callback.host);
    str += ":";
    str += callback.port;
    str += callback.path;
  }
}

bool WifiSensorsUtils::readParam(HttpRequest &req, const char *name, String &value)
{
  for (byte i = 0; i < 8; i++)
  {
    if (req.paramsNames[i] == name)
    {
      value = req.paramsValues[i];
      return true;
    }
  }
  return false;
}

void WifiSensorsUtils::readPayloadData(String &payload)
{
  while (wifiClient.available())
  {
    payload += char(wifiClient.read());
  }
  payload = decode(payload);
#if DEBUG
  Serial.println(F("PAYLOAD"));
  Serial.println(payload);
#endif
}

void WifiSensorsUtils::sendChallenge()
{
  sendHeader("401 UNAUTHORIZED", "text/plain");
  wifiClient.println("WWW-Authenticate: Basic realm=\"server\", charset=\"UTF-8\"");
  wifiClient.println();
  wifiClient.println("Zaloguj sie!");
  wifiClient.println();
}

void WifiSensorsUtils::sendConfigHtml()
{
  wifiClient.println();
  wifiClient.println("<html><body>");
  wifiClient.println("<h2>Ustawienia wifi:</h2>");
  wifiClient.println("<form action='/creds' method='POST'>");
  wifiClient.println("<p>SSID<input name='ssid' value='' required></p>");
  wifiClient.println("<p>PASSWORD<input name='pass' type='password' value='' required/></p>");
  wifiClient.println("<h2>Ustawienia serwera http:</h2>");
  wifiClient.println("<p>AUTH HEADER<input name='serverauth' value=''/></p>");
  wifiClient.println("<p>WARNING CALLBACK<input name='callback' value=''/></p>");
  wifiClient.println("<p>WARNING CALLBACK AUTH HEADER<input name='auth_header' value=''/></p>");
  wifiClient.println("<input type='submit' value='Ustaw'/>");
  wifiClient.println("</form>");
  wifiClient.println("</body></html>");
  wifiClient.println();
}

void WifiSensorsUtils::sendError(const char *msg)
{
  wifiClient.println();
  wifiClient.print("{\"error\":\"");
  wifiClient.print(msg);
  wifiClient.println("\"}");
  wifiClient.println();
}

void WifiSensorsUtils::sendHeader(const char *code, const char *contentType)
{
  wifiClient.print("HTTP/1.1 ");
  wifiClient.println(code);
  wifiClient.println("Connection: close");
  if (contentType != "")
  {
    wifiClient.print("Content-Type: ");
    wifiClient.println(contentType);
  }
}

byte WifiSensorsUtils::sendHttpRequest(Callback &callback, String path)
{
  // wait for client to finish processing
  for (byte i = 0; i < 20; i++)
  {
    if (wifiClient.connected())
    {
#if DEBUG
      Serial.print(F("Wait for client to close "));
      Serial.println(i);
#endif
      delay(10);
    }
    else
    {
      break;
    }
  }

  wifiClient.stop();

  if (wifiClient.connect(callback.host, callback.port))
  {
    Serial.print(millis());
    Serial.print(F(" Sending: "));
    Serial.println(path);

    wifiClient.print("GET ");
    wifiClient.print(path);
    wifiClient.println(" HTTP/1.1");
    wifiClient.print("Host: ");
    wifiClient.println(callback.host);
    wifiClient.println("User-Agent: ArduinoWiFi/1.1");
    if (callback.auth != "")
    {
      wifiClient.print("Authorization: ");
      wifiClient.println(callback.auth);
    }
    wifiClient.println("Connection: close");
    wifiClient.println();
    return 0;
  }

  Serial.print(F("connection failed for: "));
  Serial.print(callback.host);
  Serial.print(F(":"));
  Serial.println(callback.port);
  return 1;
}

void WifiSensorsUtils::sendStatusOk()
{
  wifiClient.println();
  wifiClient.println("{\"status\":\"ok\"}");
  wifiClient.println();
}

void WifiSensorsUtils::sendStatusForbidden()
{
  sendHeader("403 FORBIDDEN", "");
  wifiClient.println();
  wifiClient.println("{\"error\":\"403 FORBIDDEN\"}");
  wifiClient.println();
}

bool WifiSensorsUtils::sendLoginChallange(String &serverauth, HttpRequest &req)
{
  if (serverauth != "")
  {
    bool notFound = true;
    for (byte i = 0; i < WS_MAX_REQUEST_HEADERS; i++)
    {
      if (req.headersNames[i] == "Authorization")
      {
        if (serverauth != req.headersValues[i])
        {
          sendChallenge();
          return true;
        }
        notFound = false;
      }
    }
    if (notFound)
    {
      sendChallenge();
      return true;
    }
    return false;
  }

  return false;
}

void WifiSensorsUtils::serverConfigToString(ServerConfig &serverConfig, String &str)
{
  str = "{";
  str += "\"ssid\":\"";
  str += serverConfig.ssid;
  str += "\",\"serverauth\":\"";
  String serverauth = String(serverConfig.serverauth);
  if (serverauth.length() > 0)
  {
    str += "(redacted)";
  }
  str += "\",\"callback\":\"";
  String callbackStr;
  pushCallbackToString(serverConfig.callback, callbackStr);
  str += callbackStr;
  str += "\",\"callbackauth\":\"";
  if (serverConfig.callback.set)
  {
    str += "(redacted)";
  }
  str += "\"}";
}

void WifiSensorsUtils::setAnalogPinMode(int pin, int mode)
{
  if (mode == 0)
  {
    switch (pin)
    {
    case 0:
      pinMode(A0, mode == 0 ? INPUT : mode == 1 ? OUTPUT
                                                : 2);
      break;
    case 1:
      pinMode(A1, mode == 0 ? INPUT : mode == 1 ? OUTPUT
                                                : 2);
      break;
    case 2:
      pinMode(A2, mode == 0 ? INPUT : mode == 1 ? OUTPUT
                                                : 2);
      break;
    case 3:
      pinMode(A3, mode == 0 ? INPUT : mode == 1 ? OUTPUT
                                                : 2);
      break;
    case 4:
      pinMode(A4, mode == 0 ? INPUT : mode == 1 ? OUTPUT
                                                : 2);
      break;
    case 5:
      pinMode(A5, mode == 0 ? INPUT : mode == 1 ? OUTPUT
                                                : 2);
      break;
    case 6:
      pinMode(A6, mode == 0 ? INPUT : mode == 1 ? OUTPUT
                                                : 2);
    case 7:
      pinMode(A7, mode == 0 ? INPUT : mode == 1 ? OUTPUT
                                                : 2);
    }
  }
}

void WifiSensorsUtils::setPinValue(char pinType, int pin, byte value)
{
  if (pinType == 'D')
  {
    digitalWrite(pin, value);
  }
  else
  {
    digitalWriteAnalogPin(pin, value);
  }
}

bool WifiSensorsUtils::statusAuthorizationForbidden(String &serverauth, HttpRequest &req)
{
  if (serverauth != "")
  {
    bool notFound = true;
    for (byte i = 0; i < WS_MAX_REQUEST_HEADERS; i++)
    {
      if (req.headersNames[i] == "Authorization")
      {
        if (serverauth != req.headersValues[i])
        {
          sendStatusForbidden();
          return true;
        }
        notFound = false;
      }
    }
    if (notFound)
    {
      sendStatusForbidden();
      return true;
    }
    return false;
  }
}

bool WifiSensorsUtils::isCallbackUrlValid(Hashtable<String, String> *config, Callback &callback)
{
  callback.set = false;

  String callbackStr;
  String callbackAuth;

  if (config->containsKey("callback"))
  {
    callbackStr = *config->get("callback");
    callbackStr = decode(callbackStr);
  }

  if (callbackStr.length() == 0)
  {
    return true;
  }

  if (config->containsKey("auth_header"))
  {
    callbackAuth = *config->get("auth_header");
    callbackAuth.replace("+", " ");
  }

  if (callbackStr.substring(0, 4) == "https")
  {
    String msg = String("https not supported");
    sendError(msg);
    return false;
  }

  int pos1, pos2;

  // remove prefix
  pos1 = callbackStr.indexOf("://");
  if (pos1 > -1)
  {
    callbackStr = callbackStr.substring(pos1 + 3);
  }

  pos1 = callbackStr.indexOf(':');
  pos2 = callbackStr.indexOf('/');
  if (pos2 < 1)
  {
    String msg = String("callback invalid") + callbackStr;
    sendError(msg);
    return false;
  }

  String host;
  String path;
  if (pos1 > -1)
  {
    host = callbackStr.substring(0, pos1);
    String p = callbackStr.substring(pos1 + 1, pos2);
    callback.port = p.toInt();
  }
  else
  {
    host = callbackStr.substring(0, pos2);
    callback.port = 80;
  }
  path = callbackStr.substring(pos2);

  callback.set = true;
  memset(callback.host, 0, sizeof(callback.host));
  strncpy(callback.host, host.c_str(), strlen(host.c_str()));
  memset(callback.path, 0, sizeof(callback.path));
  strncpy(callback.path, path.c_str(), strlen(path.c_str()));
  if (callbackAuth.length() > 0)
  {
    memset(callback.auth, 0, sizeof(callback.auth));
    strncpy(callback.auth, callbackAuth.c_str(), strlen(callbackAuth.c_str()));
  }

  return true;
}
