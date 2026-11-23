#ifndef HASS_DISCOVERY_H
#define HASS_DISCOVERY_H

#include <stddef.h>

// Forward declarations to avoid including large headers
class PubSubClient;
template <size_t N> class StringBuffer;

// Defines the topic paths for the metrics, compatible with OVMS
struct OvmsMetrics {
  const char* soc = "metric/v/b/soc";
  const char* cabinTemp = "metric/v/e/cabintemp";
  const char* chargingState = "metric/v/c/state";
  const char* chargingActive = "metric/v/c/charging";
  const char* hvacActive = "metric/v/e/hvac";
  const char* doorFL = "metric/v/d/fl";
  const char* doorFR = "metric/v/d/fr";
  const char* doorRL = "metric/v/d/rl";
  const char* doorRR = "metric/v/d/rr";
  const char* doorTrunk = "metric/v/d/trunk";
  const char* locked = "metric/v/e/locked";
  const char* hvacSetpoint = "metric/v/e/cabinsetpoint";
  const char* fanSpeed = "metric/v/e/cabinfan";
  const char* heating = "metric/v/e/heating";
  const char* cooling = "metric/v/e/cooling";
  const char* ventilationMode = "metric/v/e/cabinvent";
  const char* latitude = "metric/v/p/latitude";
  const char* longitude = "metric/v/p/longitude";
  // Server connection status ("yes"/"no"), also used as MQTT Last Will and as
  // the Home Assistant availability topic.
  const char* connected = "metric/s/v3/connected";
  // Module diagnostics. wifiRssi and freeHeap use the OVMS metric names
  // (m.net.wifi.sq, m.freeram); the "tcu" ones are simppeliTCU specific.
  const char* wifiRssi = "metric/m/net/wifi/sq";
  const char* freeHeap = "metric/m/freeram";
  const char* resetReason = "metric/m/tcu/resetreason";
  const char* canState = "metric/m/tcu/canstate";
  const char* canTxErrors = "metric/m/tcu/can/txerrors";
  const char* canRxErrors = "metric/m/tcu/can/rxerrors";
  const char* canRxMissed = "metric/m/tcu/can/rxmissed";
};
extern const OvmsMetrics ovmsMetrics;

// Diagnostics are published on connect and then every DIAGNOSTICS_INTERVAL_MS.
// Home Assistant marks them unavailable after DIAGNOSTICS_EXPIRE_AFTER_S without an
// update (3 intervals, so one or two lost publishes do not cause false alarms).
#define DIAGNOSTICS_INTERVAL_MS (5UL * 60UL * 1000UL)
#define DIAGNOSTICS_EXPIRE_AFTER_S "900"

// Represents a single key-value pair for HASS discovery JSON
struct HassMqttProperty {
  const char* key;
  const char* value;
};

// Represents a full metric to be published to HASS
struct HassMqttMetric {
  const char* hassComponent; // "sensor", "binary_sensor", "switch", etc.
  const char* entityId;
  const HassMqttProperty* properties;
  size_t propertyCount;
  // Skip this entity when location tracking is disabled. No default member
  // initializer here: that would make the struct a non-aggregate under C++11
  // (ESP32 toolchain). Omitted members in aggregate init are zero-initialized.
  bool requiresGpsTracking;
};

void mqttPublishHassDiscovery(PubSubClient& mqttClient, const StringBuffer<128>& mqttPrefix);

#endif // HASS_DISCOVERY_H
