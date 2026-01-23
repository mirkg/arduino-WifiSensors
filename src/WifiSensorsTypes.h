#ifndef WIFISENSORS_TYPES_H
#define WIFISENSORS_TYPES_H

#define WS_VERSION "0.9.2"

#ifndef WS_ANALOG_PINS
#define WS_ANALOG_PINS 8
#endif
#ifndef WS_DIGITAL_PINS
#define WS_DIGITAL_PINS 13
#endif
#ifndef WS_MAX_REQUEST_PARAMS
#define WS_MAX_REQUEST_PARAMS 4
#endif
#ifndef WS_MAX_REQUEST_HEADERS
#define WS_MAX_REQUEST_HEADERS 1
#endif
#ifndef WS_MAX_DEVICE_PINS
#define WS_MAX_DEVICE_PINS 2
#endif
#ifndef WS_MAX_DEVICE_VALUES
#define WS_MAX_DEVICE_VALUES 2
#endif
#ifndef WS_MAX_DEVICES
#define WS_MAX_DEVICES 10
#endif
#ifndef WS_DEVICE_CONFIG_BYTES
#define WS_DEVICE_CONFIG_BYTES 1
#endif
#ifndef WS_DEVICE_CONFIG_INTS
#define WS_DEVICE_CONFIG_INTS 1
#endif
#ifndef WS_DEVICE_CONFIG_FLOATS
#define WS_DEVICE_CONFIG_FLOATS 4
#endif
#ifndef WS_DEVICE_CONFIG_LONGS
#define WS_DEVICE_CONFIG_LONGS 1
#endif

#include <Arduino.h>
#include <Array.h>

enum RunningMode
{
  RUN_MODE_AP,
  RUN_MODE_SERVER,
};

enum RunStatus
{
  RUN_STATUS_BOOT,
  RUN_STATUS_OK,
  RUN_STATUS_AP_MODE,
  RUN_STATUS_ERROR,
};

typedef struct
{
  String method;
  String path;
  String paramsNames[WS_MAX_REQUEST_PARAMS];
  String paramsValues[WS_MAX_REQUEST_PARAMS];
  String headersNames[WS_MAX_REQUEST_HEADERS];
  String headersValues[WS_MAX_REQUEST_HEADERS];
} HttpRequest;

typedef struct
{
  bool set;
  char host[16];
  int port;
  char path[128];
  char auth[64];
} Callback;

typedef struct
{
  bool set;
  bool valid;
  char ssid[32];
  char pass[64];
  char serverauth[64];
  Callback callback;
} ServerConfig;

typedef struct
{
  bool set;
  int analog[WS_ANALOG_PINS];
  int digital[WS_DIGITAL_PINS];
  bool used[WS_DIGITAL_PINS + WS_ANALOG_PINS];
} Pinout;

typedef struct
{
  int pin;
  char type;
  int mode;
} DevicePin;

typedef struct
{
  byte bytes[WS_DEVICE_CONFIG_BYTES];
  int ints[WS_DEVICE_CONFIG_INTS];
  unsigned long ulongs[WS_DEVICE_CONFIG_LONGS];
  float floats[WS_DEVICE_CONFIG_FLOATS];
} DeviceConfig;

typedef struct
{
  byte devices;
  char macStr[18];
  int freeMem;
  unsigned long devicesProcessingThresold = 0UL;
  unsigned long processingWarnings = 0UL;
  String lastWarning;
} ServerStats;

enum DeviceType
{
  DEVICE_BUTTON,
  DEVICE_DHT22,
  DEVICE_GENERIC_ANALOG_INPUT,
  DEVICE_GENERIC_DIGITAL_INPUT,
  DEVICE_MOTION,
  DEVICE_RELAY,
  DEVICE_SWITCH,
  DEVICE_TEMP_DALLAS,
  DEVICE_UNKNOWN,
};

enum DeviceConfigBytes
{
  DEVICE_CONFIG_BYTES_TRIGGER,
};

enum DeviceConfigFloats
{
  DEVICE_CONFIG_FLOAT_HUMID_ADJ,
  DEVICE_CONFIG_FLOAT_TEMP_ADJ,
  DEVICE_CONFIG_FLOAT_MIN,
  DEVICE_CONFIG_FLOAT_MAX,
};

enum DeviceConfigInts
{
  DEVICE_CONFIG_INTS_DEBOUNCE,
};

typedef struct
{
  unsigned long lastPoll;
  Array<String, WS_MAX_DEVICE_VALUES> names;
  Array<String, WS_MAX_DEVICE_VALUES> units;
  Array<String, WS_MAX_DEVICE_VALUES> values;
} DevicesValues;

typedef struct
{
  byte deviceId;
  bool active;
  DeviceType type;
  int pollInterval;
  Callback pushCallback;
  DevicePin pins[WS_MAX_DEVICE_PINS];
  byte valuesCount;
  DeviceConfig config;
} Device;

typedef struct
{
  bool set;
  byte count;
  Device devices[WS_MAX_DEVICES];
} Devices;

inline DeviceType deviceTypeFromStr(String &type)
{
  if (type == "BUTTON")
  {
    return DEVICE_BUTTON;
  }
  else if (type == "GENERIC_ANALOG")
  {
    return DEVICE_GENERIC_ANALOG_INPUT;
  }
  else if (type == "GENERIC_DIGITAL")
  {
    return DEVICE_GENERIC_DIGITAL_INPUT;
  }
  else if (type == "DHT22")
  {
    return DEVICE_DHT22;
  }
  else if (type == "MOTION")
  {
    return DEVICE_MOTION;
  }
  else if (type == "RELAY")
  {
    return DEVICE_RELAY;
  }
  else if (type == "SWITCH")
  {
    return DEVICE_SWITCH;
  }
  else if (type == "TEMP_DALLAS")
  {
    return DEVICE_TEMP_DALLAS;
  }
  return DEVICE_UNKNOWN;
}

inline String deviceTypetoStr(DeviceType type)
{
  switch (type)
  {
  case DEVICE_BUTTON:
    return "BUTTON";
  case DEVICE_DHT22:
    return "DHT22";
  case DEVICE_GENERIC_ANALOG_INPUT:
    return "GENERIC_ANALOG";
  case DEVICE_GENERIC_DIGITAL_INPUT:
    return "GENERIC_DIGITAL";
  case DEVICE_MOTION:
    return "MOTION";
  case DEVICE_RELAY:
    return "RELAY";
  case DEVICE_SWITCH:
    return "SWITCH";
  case DEVICE_TEMP_DALLAS:
    return "TEMP_DALLAS";
  default:
    return "UNKNOWN";
  }
}

inline int pinModeFromStr(String &pinMode)
{
  Serial.println(pinMode);  //TODO

  if (pinMode == "INPUT")
  {
    return 0;
  }
  else if (pinMode == "OUTPUT")
  {
    return 1;
  }
  else if (pinMode == "INPUT_PULLUP")
  {
    return 2;
  }
  return 0;
}

inline String pinModeToStr(int mode)
{
  switch (mode)
  {
  case 0:
    return "INPUT";
  case 1:
    return "OUTPUT";
  case 2:
    return "INPUT_PULLUP";
  default:
    return "INPUT";
  }
}

#endif
