#pragma once
#include <Arduino.h>
#include "status.h"

namespace Web {
  // Initialise LittleFS, HTTP routes, WebSocket, and start the server.
  void begin(Status& s);

  // Call every loop() to clean up stale WebSocket clients.
  void loop();

  // Serialise s and push to all connected WebSocket clients.
  // ts should be Net::now() — passed in to keep Web free of Net dependency.
  void notify(Status& s, time_t ts);
}
