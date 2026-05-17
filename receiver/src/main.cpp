#include "config.h"
#include "conf.h"
#include <version.h>
#include "status.h"
#include "radio.h"
#include "net.h"
#include "web.h"

static Status myData;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("> "));
  Serial.print(F("> Booting "));
  Serial.println(VERSION);

  Cfg::load();
  Net::begin(myData);
  Web::begin(myData);
  Radio::init();

  Serial.println(F("> [INIT] Ready..."));
}

void loop() {
  Web::loop();

  if (Net::loop(myData))
    Web::notify(myData, Net::now());

  if (Radio::pending()) {
    DecodedPacket dp = Radio::decode(Radio::take(), Net::now(), Net::nodeId);
    if (dp.valid) {
      Net::publish(dp.topic, dp.json);
      myData.addPacket(dp.json);
      Web::notify(myData, Net::now());
    }
  }
}
