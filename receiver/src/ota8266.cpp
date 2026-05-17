#ifdef ESP8266
#include "ota8266.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

static ESP8266WebServer        _otaServer(8080);
static ESP8266HTTPUpdateServer _otaHandler;

void ota8266Begin(const char* hostname) {
  _otaHandler.setup(&_otaServer);
  _otaServer.begin();
  Serial.print(F("> [OTA]  http://"));
  Serial.print(WiFi.localIP());
  Serial.println(F(":8080/update"));
}

void ota8266Handle() {
  _otaServer.handleClient();
}
#endif
