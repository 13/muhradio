#include <Arduino.h>
#include <avr/power.h>
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
#if defined(SENSOR_TYPE_button) + defined(SENSOR_TYPE_switch) + \
    defined(SENSOR_TYPE_pir) + defined(SENSOR_TYPE_radar) > 1
#error "One wake sensor (button/switch/pir/radar) per node: they share the PCINT flag"
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
#ifdef USE_WDT
  MCUSR = 0;         // clear WDRF, or the WDT stays on at its 15 ms reset timeout
  Power::wdtArm();
#endif
  // Timer0 stays on for millis()/delay(); SPI stays on for the radio.
  power_timer1_disable();
  power_timer2_disable();
#if !defined(SENSOR_TYPE_si7021) && !defined(SENSOR_TYPE_bmp280) && !defined(SENSOR_TYPE_bme680)
  power_twi_disable();
#endif

#if defined(VERBOSE) || defined(DEBUG)
  Serial.begin(9600);
  delay(30);

  Serial.print(F("> Booting "));
  Serial.println(VERSION);
  Serial.print(F("> Mode:"));
#ifdef USE_CRYPTO
  Serial.print(F(" CRYPTO"));
#endif
#ifdef DEBUG
  Serial.print(F(" DEBUG"));
#endif
  Serial.println();
#else
  power_usart0_disable();
#endif

  randomSeed(analogRead(0));
  Node::init();
  Power::init();
  for (uint8_t attempt = 1; !Transport::init(); attempt++) {
    if (attempt >= 3) {
#ifdef VERBOSE
      Serial.println(F("> Radio init failed — sleeping forever"));
#endif
      Power::sleepDeep(); // dead radio: don't burn the battery on wake/send cycles
    }
    delay(250);
  }
  vRef.begin();
  digitalWrite(LED_BUILTIN, LOW);

#ifdef SENSOR_TYPE_button
  Button::setup();
#endif
#ifdef SENSOR_TYPE_switch
  Switch::setup();
#endif
#ifdef SENSOR_TYPE_pir
  PIR::setup();
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
  // Wake sensors: debounce before anything else, and go straight back to sleep
  // on a spurious wake, on chatter that settled where it already was, or on a
  // button release / motion end. VCC is added unconditionally below, so
  // without this the node would transmit anyway.
#if defined(SENSOR_TYPE_switch)
  if (!Switch::pending()) { Power::sleepSensor(); return; }
#elif defined(SENSOR_TYPE_button)
  if (!Button::pending()) { Power::sleepSensor(); return; }
#elif defined(SENSOR_TYPE_pir)
  if (!PIR::pending())    { Power::sleepSensor(); return; }
#elif defined(SENSOR_TYPE_radar)
  if (!Radar::pending())  { Power::sleepSensor(); return; }
#endif

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
