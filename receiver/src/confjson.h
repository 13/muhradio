#pragma once
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Flat-JSON value extractors for /config.json, shared by conf.cpp and the
// native tests. jsonGetStr decodes exactly the escapes jsonAppendEscaped
// (jsonbuilder.h) produces: \" \\ \n \r \t and \u00XX.
static bool jsonGetStr(const char* j, const char* key, char* out, size_t n) {
  char pat[80];
  snprintf(pat, sizeof(pat), "\"%s\":\"", key);
  const char* p = strstr(j, pat);
  if (!p) return false;
  p += strlen(pat);
  auto hx = [](char c) -> int {
    return c >= 'a' ? c - 'a' + 10 : c >= 'A' ? c - 'A' + 10 : c - '0';
  };
  size_t i = 0;
  for (; *p && i + 1 < n; p++) {
    if (*p == '\\' && p[1]) {
      p++;
      char c = *p;
      if      (c == 'n') out[i++] = '\n';
      else if (c == 'r') out[i++] = '\r';
      else if (c == 't') out[i++] = '\t';
      else if (c == 'u' && p[1] && p[2] && p[3] && p[4]) {
        out[i++] = (char)((hx(p[3]) << 4) | hx(p[4])); // \u00XX is all we emit
        p += 4;
      }
      else out[i++] = c; // \" \\ and unknown escapes: literal
      continue;
    }
    if (*p == '"') break;
    out[i++] = *p;
  }
  out[i] = '\0';
  return true;
}

static bool jsonGetLong(const char* j, const char* key, long& out) {
  char pat[80];
  snprintf(pat, sizeof(pat), "\"%s\":", key);
  const char* p = strstr(j, pat);
  if (!p) return false;
  out = atol(p + strlen(pat));
  return true;
}

// Value ranges shared by the settings POST, config import and load().
static inline bool cfgPortOk(long v) { return v >= 1 && v <= 65535; }
static inline bool cfgTzOk(long v)   { return v >= -720 && v <= 840; } // UTC-12..+14 in minutes
static inline bool cfgDstOk(long v)  { return v >= 0 && v <= 2; }

// Sanity check for an uploaded config.json before it replaces the live one:
// a {...} object that keeps the device reachable (SSID + broker set) and whose
// numeric fields, when present, are in range.
static inline bool cfgImportOk(const char* j) {
  size_t n = strlen(j);
  while (n && (j[n - 1] == '\n' || j[n - 1] == '\r' || j[n - 1] == ' ')) n--; // hand-edited file
  if (n < 2 || j[0] != '{' || j[n - 1] != '}') return false;
  char s[80];
  if (!jsonGetStr(j, "wifi_ssid", s, sizeof(s)) || !s[0])   return false;
  if (!jsonGetStr(j, "mqtt_server", s, sizeof(s)) || !s[0]) return false;
  long v;
  if (jsonGetLong(j, "mqtt_port", v) && !cfgPortOk(v)) return false;
  if (jsonGetLong(j, "tz_offset", v) && !cfgTzOk(v))   return false;
  if (jsonGetLong(j, "dst_mode",  v) && !cfgDstOk(v))  return false;
  return true;
}
