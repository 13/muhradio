#include <Arduino.h>
#include <VoltageReference.h>
#include <version.h>

#include "node.h"
#include "packet.h"
#include "transport.h"
#include "power.h"

#ifdef SENSOR_TYPE_button
#include "sensors/button.h"
#endif
#ifdef SENSOR_TYPE_switch
#include "sensors/switch.h"
#endif
#ifdef SENSOR_TYPE_pir
#include "sensors/pir.h"
#endif
#ifdef SENSOR_TYPE_radar
#include "sensors/radar.h"
#endif
#ifdef SENSOR_TYPE_si7021
#include "sensors/si7021.h"
#endif
#ifdef SENSOR_TYPE_ds18b20
#include "sensors/ds18b20.h"
#endif
#ifdef SENSOR_TYPE_bmp280
#include "sensors/bmp280.h"
#endif
#ifdef SENSOR_TYPE_bme680
#include "sensors/bme680.h"
#endif

static VoltageReference vRef;
static Packet           pkt;

#ifdef VERBOSE_PC
static uint16_t msgCounter = 1;
#endif

void setup() {
  Serial.begin(9600);
  delay(30);

  Serial.print(F("> Booting "));
  Serial.println(VERSION);
#ifdef VERBOSE
  Serial.print(F("> Mode:"));
#ifdef USE_CRYPTO
  Serial.print(F(" CRYPTO"));
#endif
#ifdef DEBUG
  Serial.print(F(" DEBUG"));
#endif
  Serial.println();
#endif

  randomSeed(analogRead(0));
  Node::init();
  Transport::init();
  vRef.begin();
  digitalWrite(LED_BUILTIN, LOW);

#ifdef SENSOR_TYPE_button
  Button::setup();
#endif
#ifdef SENSOR_TYPE_switch
  Switch::setup();
#endif
#ifdef SENSOR_TYPE_pir
  PIR::setup(); // blocks until first event
#endif
#ifdef SENSOR_TYPE_radar
  Radar::setup();
#endif
#ifdef SENSOR_TYPE_si7021
  Si7021::setup();
#endif
#ifdef SENSOR_TYPE_ds18b20
  DS18B20::setup();
#endif
#ifdef SENSOR_TYPE_bmp280
  BMP280::setup();
#endif
#ifdef SENSOR_TYPE_bme680
  BME680::setup();
#endif
}

void loop() {
  // uid and pid go into the fixed header, not the bitmap
  pkt.reset(Node::uid(), (uint8_t)random(1, 256));

  // Fields must be added in ascending Field enum order (bit 0 → 14)
#ifdef VERBOSE_PC
  pkt.addU16(Field::COUNTER, msgCounter++);  // bit 0
#endif
#ifdef SENSOR_TYPE_button
  Button::read(pkt);                         // bit 1
#endif
#ifdef SENSOR_TYPE_switch
  Switch::read(pkt);                         // bit 2
#endif
#ifdef SENSOR_TYPE_pir
  PIR::read(pkt);                            // bit 3
#endif
#ifdef SENSOR_TYPE_radar
  Radar::read(pkt);                          // bit 4
#endif
#ifdef SENSOR_TYPE_si7021
  Si7021::read(pkt);                         // bits 5-6
#endif
#ifdef SENSOR_TYPE_ds18b20
  DS18B20::read(pkt);                        // bit 7
#endif
#ifdef SENSOR_TYPE_bmp280
  BMP280::read(pkt);                         // bits 8-9
#endif
#ifdef SENSOR_TYPE_bme680
  BME680::read(pkt);                         // bits 10-13
#endif

  float vcc = vRef.readVcc() / 1000.0f;
  pkt.addU8(Field::VCC, (uint8_t)round(vcc * 10)); // bit 14
#ifdef VERBOSE
  Serial.print(F("VCC: ")); Serial.print(vcc, 1); Serial.println(F("V"));
#endif

#ifdef MQTT_RETAINED
  pkt.setRetained();
#endif
#ifdef DEBUG
  pkt.print();
#endif

  Transport::send(pkt);
  Power::sleepSensor();
}
