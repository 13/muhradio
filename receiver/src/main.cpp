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
#include "nodetable.h"

static Status myData;
NodeTable g_nodeTable; // fed by the radio path, served via GET /nodes

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

  if (Net::loop(myData)) {
    Web::notify(myData, Net::nowUtc());
    // Publish health for nodes heard since the last minute tick (retained,
    // one small message per uid so the 512-byte MQTT buffer is never an issue)
    NodeEntry* e;
    while ((e = g_nodeTable.nextDirty()) != nullptr) {
      char topic[64], json[128];
      snprintf(topic, sizeof(topic), "%s/%u/health", MQTT_TOPIC, e->uid);
      NodeTable::entryJson(*e, (uint32_t)Net::nowUtc(), json, sizeof(json));
      if (!Net::publish(topic, json, true)) break; // broker down — retry next tick
      e->dirty = false;
    }
  }

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
      g_nodeTable.update(dp.uid, (uint32_t)Net::nowUtc(), dp.rssi, dp.vcc10);
      myData.addPacket(dp.json);
      Web::notify(myData, Net::nowUtc());
    }
  }
#endif
}
