#include "config.h"
#include "conf.h"
#include <version.h>
#include "status.h"
#ifdef USE_BRESSER
#  include "bresser.h"
#else
#  include "radio.h"
#endif
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
#ifdef USE_BRESSER
  Bresser::init();
#else
  Radio::init();
#endif

  Serial.println(F("> [INIT] Ready..."));
}

void loop() {
  Web::loop();

  if (Net::loop(myData))
    Web::notify(myData, Net::nowUtc());

#ifdef USE_BRESSER
  if (Bresser::pending()) {
    BresserPacket dp = Bresser::decode(Net::nowUtc(), Net::nodeId);
    if (dp.valid) {
      Net::publish(dp.topic, dp.json);
      myData.addPacket(dp.json);
      Web::notify(myData, Net::nowUtc());
    }
  }
#else
  if (Radio::pending()) {
    DecodedPacket dp = Radio::decode(Radio::take(), Net::nowUtc(), Net::nodeId);
    if (dp.valid) {
      Net::publish(dp.topic, dp.json, dp.retained);
      myData.addPacket(dp.json);
      Web::notify(myData, Net::nowUtc());
    }
  }
#endif
}
