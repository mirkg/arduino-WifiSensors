#ifndef WIFISENSORS_UTILS_PARSERS_H
#define WIFISENSORS_UTILS_PARSERS_H

inline char dec2hex(short int c)
{
  if (0 <= c && c <= 9)
  {
    return c + '0';
  }
  else if (10 <= c && c <= 15)
  {
    return c + 'A' - 10;
  }
  else
  {
    return -1;
  }
}

inline int hex2dec(char c)
{
  if ('0' <= c && c <= '9')
  {
    return c - '0';
  }
  else if ('a' <= c && c <= 'f')
  {
    return c - 'a' + 10;
  }
  else if ('A' <= c && c <= 'F')
  {
    return c - 'A' + 10;
  }
  else
  {
    return -1;
  }
}

inline String decode(String &urlcode)
{
  String strcode = "";
  int i = 0;
  int len = urlcode.length();
  for (i = 0; i < len; ++i)
  {
    char c = urlcode[i];
    if (c != '%')
    {
      strcode += String(c);
    }
    else
    {
      char c1 = urlcode[++i];
      char c0 = urlcode[++i];
      int num = 0;
      num = hex2dec(c1) * 16 + hex2dec(c0);
      strcode += String((char)num);
    }
  }
  return strcode;
}

inline String encode(String &strcode)
{
  String urlcode = "";
  int i = 0;
  int len = strcode.length();
  for (i = 0; i < len; ++i)
  {
    char c = strcode[i];
    if (('0' <= c && c <= '9') ||
        ('a' <= c && c <= 'z') ||
        ('A' <= c && c <= 'Z') ||
        c == '/' || c == '.')
    {
      urlcode += String(c);
    }
    else
    {
      int j = (short int)c;
      if (j < 0)
        j += 256;
      int i1, i0;
      i1 = j / 16;
      i0 = j - i1 * 16;
      urlcode += String('%');
      urlcode += String((char)dec2hex(i1));
      urlcode += String((char)dec2hex(i0));
    }
  }
  return urlcode;
}

template <typename T>
inline String serialize(const T &v)
{
  char buffer[sizeof(T)];
  memcpy(buffer, &v, sizeof(T));
  return String(buffer, sizeof(T));
}

inline String crypt(String strin)
{
  String strout;
  for (int i = 0; i < strin.length(); i++)
  {
    char c = strin[i];
    strout += String((char)(c + 3));
  }
  return strout;
}

inline String decrypt(String strin)
{
  String strout;
  for (int i = 0; i < strin.length(); i++)
  {
    char c = strin[i];
    strout += String((char)(c - 3));
  }
  return strout;
}

inline bool findBoolInJson(String &str, String key, bool &value)
{
  int pos = str.indexOf("\"" + key + "\":");
  if (pos < 0)
  {
    return false;
  }
  String tmp = str.substring(pos + 3 + key.length());
  pos = tmp.indexOf(",");
  if (pos < 0)
  {
    pos = tmp.indexOf("}");
  }
  value = tmp.substring(0, pos) == "true";
  return true;
}

inline bool findIntInJson(String &str, String key, int &value)
{
  int pos = str.indexOf("\"" + key + "\":");
  if (pos < 0)
  {
    return false;
  }
  String tmp = str.substring(pos + 3 + key.length());
  pos = tmp.indexOf(",");
  if (pos < 0)
  {
    pos = tmp.indexOf("}");
  }
  value = tmp.substring(0, pos).toInt();
  return true;
}

inline bool findJsonStrInJson(String &str, String key, byte level, String &value)
{
  int pos = str.indexOf("\"" + key + "\":");
  if (pos < 0)
  {
    return false;
  }
  String tmp = str.substring(pos + 3 + key.length());
  String lvl = "";
  for (byte i = 0; i < level; i++)
  {
    lvl += "}";
  }
  pos = tmp.indexOf(lvl);
  if (pos < 0)
  {
    return false;
  }
  value = tmp.substring(0, pos + level);
  return true;
}

inline bool findStrInJson(String &str, String key, String &value)
{
  int pos = str.indexOf("\"" + key + "\":");
  if (pos < 0)
  {
    return false;
  }
  String tmp = str.substring(pos + 4 + key.length());
  pos = tmp.indexOf("\",");
  if (pos < 0)
  {
    pos = tmp.indexOf("\"}");
  }
  value = tmp.substring(0, pos);
  return true;
}

#endif
