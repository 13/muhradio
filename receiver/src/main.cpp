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
#ifdef ESP32
#include <esp_task_wdt.h>
#endif

static Status myData;
NodeTable g_nodeTable; // fed by the radio path, served via GET /nodes

// Watchdog: recover from a hung driver/network stack without a power cycle.
// 60 s covers the worst legitimate loop iteration (blocking MQTT TCP connect
// ~15 s + NTP retries); enabled after Net::begin so boot WiFi wait is exempt.
static constexpr uint32_t WDT_TIMEOUT_S = 60;

static void wdtBegin() {
#if defined(ESP32)
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms    = WDT_TIMEOUT_S * 1000;
  cfg.trigger_panic = true;
  if (esp_task_wdt_init(&cfg) != ESP_OK) esp_task_wdt_reconfigure(&cfg);
#else
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
#endif
  esp_task_wdt_add(NULL);
#elif defined(ESP8266)
  ESP.wdtEnable(WDTO_8S); // software WDT; hardware WDT stays armed behind it
#endif
}

static inline void wdtFeed() {
#if defined(ESP32)
  esp_task_wdt_reset();
#elif defined(ESP8266)
  ESP.wdtFeed();
#endif
}

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

  wdtBegin();
  Serial.println(F("> [INIT] Ready..."));
}

void loop() {
  wdtFeed();
  Web::loop();

  if (Net::loop(myData)) {
    Web::notify(myData, Net::nowUtc());
    // Publish health for nodes heard since the last minute tick (retained,
    // one small message per uid so the 512-byte MQTT buffer is never an issue).
    // Topic lives under this receiver's hostname — parallel receivers must
    // not overwrite each other's retained health records.
    // Hold off until NTP has synced: pre-sync epoch (~1970) would persist
    // nonsense last-seen values in the retained messages.
    NodeEntry* e;
    while (Cfg::g.node_stats && Net::nowUtc() > 1577836800 &&
           (e = g_nodeTable.nextDirty()) != nullptr) {
      char topic[96], json[160];
      snprintf(topic, sizeof(topic), "%s/%s/nodes/%u",
               MQTT_TOPIC_LWT, Net::hostname, e->uid);
      NodeTable::entryJson(*e, (uint32_t)Net::nowUtc(), Net::nodeId,
                           json, sizeof(json));
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
      if (Cfg::g.node_stats)
        g_nodeTable.update(dp.uid, (uint32_t)Net::nowUtc(), dp.rssi, dp.vcc10);
      myData.addPacket(dp.json);
      Web::notify(myData, Net::nowUtc());
    }
  }
#endif
}
