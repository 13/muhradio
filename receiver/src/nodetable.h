#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Per-node health tracking: last-seen, packet count, RSSI, battery.
// Pure logic — unit-tested in test/test_nodes. Fed by the radio decode path,
// served via GET /nodes and published per-uid on the minute tick.

// VCC*10 below this = low battery (2.7 V default; ATmega328 @ 8 MHz browns
// out around 2.4 V, so this leaves headroom to swap cells).
#ifndef LOW_BAT_DV
#define LOW_BAT_DV 27
#endif

struct NodeEntry {
  uint16_t uid;
  uint32_t lastSeen;   // epoch seconds
  uint32_t count;      // packets since boot
  int16_t  rssi;       // last RSSI dBm
  uint8_t  vcc10;      // last VCC in V*10, 0 = never reported
  bool     dirty;      // changed since last publish
};

class NodeTable {
public:
  static constexpr uint8_t MAX = 32;

  // Record one received packet. vcc10 = 0 keeps the previous battery value.
  void update(uint16_t uid, uint32_t nowS, int16_t rssi, uint8_t vcc10) {
    NodeEntry* e = _find(uid);
    if (!e) e = _evict();
    if (e->uid != uid) { *e = NodeEntry{}; e->uid = uid; }
    e->lastSeen = nowS;
    e->count++;
    e->rssi = rssi;
    if (vcc10) e->vcc10 = vcc10;
    e->dirty = true;
  }

  uint8_t size() const {
    uint8_t n = 0;
    for (const NodeEntry& e : _e) if (e.count) n++;
    return n;
  }

  const NodeEntry* get(uint16_t uid) const {
    return const_cast<NodeTable*>(this)->_find(uid);
  }

  static bool lowBat(const NodeEntry& e) {
    return e.vcc10 > 0 && e.vcc10 < LOW_BAT_DV;
  }

  // Next entry changed since last publish; returns nullptr when clean.
  NodeEntry* nextDirty() {
    for (NodeEntry& e : _e) if (e.count && e.dirty) return &e;
    return nullptr;
  }

  // One node as a small JSON object (fits a 512-byte MQTT buffer easily).
  // rxNode identifies the receiver — parallel receivers each publish their
  // own health record, so the field disambiguates retained messages.
  static void entryJson(const NodeEntry& e, uint32_t nowS, const char* rxNode,
                        char* buf, size_t cap) {
    snprintf(buf, cap,
      "{\"uid\":%u,\"last\":%lu,\"age\":%lu,\"count\":%lu,"
      "\"rssi\":%d,\"vcc\":%u.%u,\"low\":%d,\"node\":\"%s\"}",
      e.uid, (unsigned long)e.lastSeen,
      (unsigned long)(nowS >= e.lastSeen ? nowS - e.lastSeen : 0),
      (unsigned long)e.count, e.rssi, e.vcc10 / 10, e.vcc10 % 10,
      lowBat(e) ? 1 : 0, rxNode);
  }

  // Full table as a JSON array (for GET /nodes).
  size_t toJson(char* buf, size_t cap, uint32_t nowS, const char* rxNode) const {
    size_t n = snprintf(buf, cap, "{\"ts\":%lu,\"nodes\":[", (unsigned long)nowS);
    bool first = true;
    for (const NodeEntry& e : _e) {
      if (!e.count) continue;
      char one[160];
      entryJson(e, nowS, rxNode, one, sizeof(one));
      size_t need = strlen(one) + (first ? 0 : 1);
      if (n + need + 3 > cap) break; // keep room for "]}\0"
      if (!first) buf[n++] = ',';
      memcpy(buf + n, one, strlen(one));
      n += strlen(one);
      first = false;
    }
    buf[n++] = ']'; buf[n++] = '}'; buf[n] = '\0';
    return n;
  }

private:
  NodeEntry _e[MAX] = {};

  NodeEntry* _find(uint16_t uid) {
    for (NodeEntry& e : _e) if (e.count && e.uid == uid) return &e;
    return nullptr;
  }

  // Free slot if any, else the longest-silent node.
  NodeEntry* _evict() {
    NodeEntry* best = &_e[0];
    for (NodeEntry& e : _e) {
      if (!e.count) return &e;
      if (e.lastSeen < best->lastSeen) best = &e;
    }
    *best = NodeEntry{};
    return best;
  }
};
