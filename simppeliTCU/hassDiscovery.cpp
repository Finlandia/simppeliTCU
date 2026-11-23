#include "hassDiscovery.h"
#include "configuration.h"
#include "stringBuffer.h"
#include "version.h"
#include <PubSubClient.h>
#include <WiFi.h>

// Define the global instance of the metric topics
const OvmsMetrics ovmsMetrics;

// Appends the fields shared by every entity: availability (bound to the MQTT
// Last Will topic, so entities go unavailable when the TCU drops off the
// network) and device info. Closes the JSON object.
static void appendCommonFields(StringBuffer<1024>& payload, const StringBuffer<128>& mqttPrefix) {
  const char* vid = getVehicleId();
  // Sized for the device block with a max-length (64 char) vehicle ID, which appears twice.
  StringBuffer<384> extra;
  extra.format(",\"availability_topic\":\"%s%s\",\"payload_available\":\"yes\",\"payload_not_available\":\"no\"",
               mqttPrefix.c_str(), ovmsMetrics.connected);
  payload.append(extra.c_str());

  // configuration_url adds a "Visit" link to the web UI on the HA device page.
  extra.format(",\"device\":{\"identifiers\":[\"simppeliTCU_%s\"],\"name\":\"simppeliTCU %s\",\"manufacturer\":\"simppeliTCU\",\"model\":\"TCU\",\"sw_version\":\"%s\",\"configuration_url\":\"http://%s/\"}}",
               vid, vid, FW_FULL_VERSION, WiFi.localIP().toString().c_str());
  payload.append(extra.c_str());
}

// Helper function to publish a single HASS config message
void publishHassConfigForMetric(PubSubClient& mqttClient, const StringBuffer<128>& mqttPrefix, const HassMqttMetric& metric) {
  const char* vid = getVehicleId();
  StringBuffer<128> topic;
  topic.format("homeassistant/%s/simppeliTCU_%s/%s/config", metric.hassComponent, vid, metric.entityId);

  StringBuffer<1024> payload;
  payload.copyFrom("{");

  payload.append("\"unique_id\":\"simppeliTCU_");
  payload.append(vid);
  payload.append("_");
  payload.append(metric.entityId);
  payload.append("\",");

  for (size_t i = 0; i < metric.propertyCount; i++) {
    const auto& prop = metric.properties[i];
    payload.append("\"");
    payload.append(prop.key);
    payload.append("\":\"");

    // Prepend the base OVMS topic path for state and command topics
    if (strcmp(prop.key, "state_topic") == 0 || strcmp(prop.key, "command_topic") == 0) {
      payload.append(mqttPrefix.c_str());
    }
    payload.append(prop.value);
    payload.append("\"");
    if (i < metric.propertyCount - 1) {
      payload.append(",");
    }
  }

  appendCommonFields(payload, mqttPrefix);

  mqttClient.publish(topic.c_str(), (const uint8_t*)payload.c_str(), payload.length(), true);
}

// --- Metric Definitions ---

// Sensor: State of Charge
static const HassMqttProperty socProps[] = {
  {"name", "SOC"},
  {"device_class", "battery"},
  {"unit_of_measurement", "%"},
  {"state_topic", ovmsMetrics.soc}
};

// Sensor: Cabin Temperature
static const HassMqttProperty tempProps[] = {
  {"name", "Cabin Temperature"},
  {"device_class", "temperature"},
  {"unit_of_measurement", "°C"},
  {"state_topic", ovmsMetrics.cabinTemp}
};

// Binary Sensor: Charging Status
static const HassMqttProperty chargeProps[] = {
  {"name", "Charging Active"},
  {"device_class", "battery_charging"},
  {"state_topic", ovmsMetrics.chargingActive},
  {"payload_on", "yes"},
  {"payload_off", "no"}
};

// Binary Sensor: HVAC Status
static const HassMqttProperty hvacProps[] = {
  {"name", "HVAC Active"},
  {"device_class", "running"},
  {"state_topic", ovmsMetrics.hvacActive},
  {"payload_on", "yes"},
  {"payload_off", "no"}
};

// Sensor: Charging State
static const HassMqttProperty chargeStateProps[] = {
  {"name", "Charging State"},
  {"state_topic", ovmsMetrics.chargingState}
};

// Sensor: HVAC Setpoint
static const HassMqttProperty hvacSetpointProps[] = {
  {"name", "HVAC Setpoint"},
  {"device_class", "temperature"},
  {"unit_of_measurement", "°C"},
  {"state_topic", ovmsMetrics.hvacSetpoint}
};

// Sensor: Fan Speed
static const HassMqttProperty fanSpeedProps[] = {
  {"name", "Fan Speed"},
  {"unit_of_measurement", "%"},
  {"state_topic", ovmsMetrics.fanSpeed}
};

// Sensor: Ventilation Mode
static const HassMqttProperty ventilationModeProps[] = {
  {"name", "Ventilation Mode"},
  {"state_topic", ovmsMetrics.ventilationMode}
};

// Sensor: Latitude
static const HassMqttProperty latitudeProps[] = {
  {"name", "Latitude"},
  {"unit_of_measurement", "°"},
  {"state_topic", ovmsMetrics.latitude}
};

// Sensor: Longitude
static const HassMqttProperty longitudeProps[] = {
  {"name", "Longitude"},
  {"unit_of_measurement", "°"},
  {"state_topic", ovmsMetrics.longitude}
};

// Binary Sensor: Lock Status
static const HassMqttProperty lockProps[] = {
  {"name", "Locked"},
  {"device_class", "lock"},
  {"state_topic", ovmsMetrics.locked},
  {"payload_on", "yes"},
  {"payload_off", "no"}
};

// Binary Sensor: Heating Status
static const HassMqttProperty heatingProps[] = {
  {"name", "Heating Active"},
  {"state_topic", ovmsMetrics.heating},
  {"payload_on", "yes"},
  {"payload_off", "no"}
};

// Binary Sensor: Cooling Status
static const HassMqttProperty coolingProps[] = {
  {"name", "Cooling Active"},
  {"state_topic", ovmsMetrics.cooling},
  {"payload_on", "yes"},
  {"payload_off", "no"}
};

// Binary Sensor: Door Status (template for all doors)
static const HassMqttProperty doorProps[] = {
  {"device_class", "door"},
  {"payload_on", "yes"},
  {"payload_off", "no"}
};

// Switch: Lock Control
static const HassMqttProperty lockSwProps[] = {
  {"name", "Door Lock"},
  {"state_topic", ovmsMetrics.locked},
  {"command_topic", "client/hass/command/lock"},
  {"payload_on", "lock"},
  {"payload_off", "unlock"},
  {"state_on", "yes"},
  {"state_off", "no"},
  {"icon", "mdi:car-door-lock"}
};

// Switch: HVAC Control
static const HassMqttProperty hvacSwProps[] = {
  {"name", "HVAC Control"},
  {"state_topic", ovmsMetrics.hvacActive},
  {"command_topic", "client/hass/command/hvac"},
  {"payload_on", "climatecontrol on"},
  {"payload_off", "climatecontrol off"},
  {"state_on", "yes"},
  {"state_off", "no"}
};

// Button: Charge Control
static const HassMqttProperty chargeBtnProps[] = {
  {"name", "Start Charging"},
  {"command_topic", "client/hass/command/charge"},
  {"payload_press", "charge start"}
};

// Button: Refresh Data
static const HassMqttProperty refreshProps[] = {
  {"name", "Refresh Data"},
  {"command_topic", "client/hass/command/refresh"},
  {"payload_press", "server v3 update modified"}
};

// --- Diagnostics (shown under "Diagnostic" on the HA device page) ---

// Sensor: WiFi signal strength
static const HassMqttProperty wifiRssiProps[] = {
  {"name", "WiFi Signal"},
  {"device_class", "signal_strength"},
  {"unit_of_measurement", "dBm"},
  {"state_class", "measurement"},
  {"entity_category", "diagnostic"},
  {"expire_after", DIAGNOSTICS_EXPIRE_AFTER_S},
  {"state_topic", ovmsMetrics.wifiRssi}
};

// Sensor: Free heap memory
static const HassMqttProperty freeHeapProps[] = {
  {"name", "Free Heap"},
  {"device_class", "data_size"},
  {"unit_of_measurement", "B"},
  {"suggested_unit_of_measurement", "kB"},
  {"state_class", "measurement"},
  {"icon", "mdi:memory"},
  {"entity_category", "diagnostic"},
  {"expire_after", DIAGNOSTICS_EXPIRE_AFTER_S},
  {"state_topic", ovmsMetrics.freeHeap}
};

// Sensor: Reason for the last reset (power-on, brownout, watchdog, panic, ...)
static const HassMqttProperty resetReasonProps[] = {
  {"name", "Last Reset Reason"},
  {"icon", "mdi:restart-alert"},
  {"entity_category", "diagnostic"},
  {"expire_after", DIAGNOSTICS_EXPIRE_AFTER_S},
  {"state_topic", ovmsMetrics.resetReason}
};

// Sensor: Primary CAN controller state
static const HassMqttProperty canStateProps[] = {
  {"name", "CAN Bus State"},
  {"icon", "mdi:car-connected"},
  {"entity_category", "diagnostic"},
  {"expire_after", DIAGNOSTICS_EXPIRE_AFTER_S},
  {"state_topic", ovmsMetrics.canState}
};

// Sensor: CAN transmit error counter (TEC). Non-zero values point at wiring,
// termination or bitrate problems; 256 means the controller went bus-off.
static const HassMqttProperty canTxErrorsProps[] = {
  {"name", "CAN TX Error Counter"},
  {"state_class", "measurement"},
  {"icon", "mdi:alert-circle-outline"},
  {"entity_category", "diagnostic"},
  {"expire_after", DIAGNOSTICS_EXPIRE_AFTER_S},
  {"state_topic", ovmsMetrics.canTxErrors}
};

// Sensor: CAN receive error counter (REC)
static const HassMqttProperty canRxErrorsProps[] = {
  {"name", "CAN RX Error Counter"},
  {"state_class", "measurement"},
  {"icon", "mdi:alert-circle-outline"},
  {"entity_category", "diagnostic"},
  {"expire_after", DIAGNOSTICS_EXPIRE_AFTER_S},
  {"state_topic", ovmsMetrics.canRxErrors}
};

// Sensor: CAN frames dropped because the RX queue was full (cumulative since boot)
static const HassMqttProperty canRxMissedProps[] = {
  {"name", "CAN Missed RX Frames"},
  {"state_class", "total_increasing"},
  {"icon", "mdi:message-alert-outline"},
  {"entity_category", "diagnostic"},
  {"expire_after", DIAGNOSTICS_EXPIRE_AFTER_S},
  {"state_topic", ovmsMetrics.canRxMissed}
};

// The Central Metric Registry
static const HassMqttMetric hassMetrics[] = {
  // Sensors
  {"sensor", "soc", socProps, sizeof(socProps)/sizeof(HassMqttProperty), false},
  {"sensor", "cabin_temp", tempProps, sizeof(tempProps)/sizeof(HassMqttProperty), false},
  {"sensor", "charge_state", chargeStateProps, sizeof(chargeStateProps)/sizeof(HassMqttProperty), false},
  {"sensor", "hvac_setpoint", hvacSetpointProps, sizeof(hvacSetpointProps)/sizeof(HassMqttProperty), false},
  {"sensor", "fan_speed", fanSpeedProps, sizeof(fanSpeedProps)/sizeof(HassMqttProperty), false},
  {"sensor", "ventilation_mode", ventilationModeProps, sizeof(ventilationModeProps)/sizeof(HassMqttProperty), false},
  {"sensor", "latitude", latitudeProps, sizeof(latitudeProps)/sizeof(HassMqttProperty), true},
  {"sensor", "longitude", longitudeProps, sizeof(longitudeProps)/sizeof(HassMqttProperty), true},

  // Binary Sensors
  {"binary_sensor", "charging", chargeProps, sizeof(chargeProps)/sizeof(HassMqttProperty), false},
  {"binary_sensor", "hvac", hvacProps, sizeof(hvacProps)/sizeof(HassMqttProperty), false},
  {"binary_sensor", "locked", lockProps, sizeof(lockProps)/sizeof(HassMqttProperty), false},
  {"binary_sensor", "heating", heatingProps, sizeof(heatingProps)/sizeof(HassMqttProperty), false},
  {"binary_sensor", "cooling", coolingProps, sizeof(coolingProps)/sizeof(HassMqttProperty), false},

  // Switches
  {"switch", "hvac_switch", hvacSwProps, sizeof(hvacSwProps)/sizeof(HassMqttProperty), false},
  {"switch", "lock_switch", lockSwProps, sizeof(lockSwProps)/sizeof(HassMqttProperty), false},

  // Diagnostics
  {"sensor", "wifi_rssi", wifiRssiProps, sizeof(wifiRssiProps)/sizeof(HassMqttProperty), false},
  {"sensor", "free_heap", freeHeapProps, sizeof(freeHeapProps)/sizeof(HassMqttProperty), false},
  {"sensor", "reset_reason", resetReasonProps, sizeof(resetReasonProps)/sizeof(HassMqttProperty), false},
  {"sensor", "can_state", canStateProps, sizeof(canStateProps)/sizeof(HassMqttProperty), false},
  {"sensor", "can_tx_errors", canTxErrorsProps, sizeof(canTxErrorsProps)/sizeof(HassMqttProperty), false},
  {"sensor", "can_rx_errors", canRxErrorsProps, sizeof(canRxErrorsProps)/sizeof(HassMqttProperty), false},
  {"sensor", "can_rx_missed", canRxMissedProps, sizeof(canRxMissedProps)/sizeof(HassMqttProperty), false},

  // Buttons
  {"button", "charge", chargeBtnProps, sizeof(chargeBtnProps)/sizeof(HassMqttProperty), false},
  {"button", "refresh", refreshProps, sizeof(refreshProps)/sizeof(HassMqttProperty), false},
};

// Door definitions
struct HassDoorDefinition {
    const char* entityId;
    const char* name;
    const char* stateTopic;
};

static const HassDoorDefinition doorDefinitions[] = {
    {"door_fl", "Front Left Door", ovmsMetrics.doorFL},
    {"door_fr", "Front Right Door", ovmsMetrics.doorFR},
    {"door_rl", "Rear Left Door", ovmsMetrics.doorRL},
    {"door_rr", "Rear Right Door", ovmsMetrics.doorRR},
    {"door_trunk", "Trunk", ovmsMetrics.doorTrunk},
};

/**
 * @brief Publishes all Home Assistant discovery configuration messages.
 * 
 * Iterates through the central metric registry and sends a retained MQTT message
 * for each, allowing Home Assistant to automatically discover and configure the entities.
 * 
 * @param mqttClient The active PubSubClient instance.
 * @param mqttPrefix The base topic prefix for the device (e.g., "ovms/user/vehicleid/").
 */
void mqttPublishHassDiscovery(PubSubClient& mqttClient, const StringBuffer<128>& mqttPrefix) {
  if (!mqttClient.connected()) return;

  for (const auto& metric : hassMetrics) {
    if (metric.requiresGpsTracking && !getTrackingEnabled()) continue;
    publishHassConfigForMetric(mqttClient, mqttPrefix, metric);
  }

  // Publish door sensors separately as they share properties
  for (const auto& door : doorDefinitions) {
    const char* vid = getVehicleId();
    StringBuffer<128> topic;
    topic.format("homeassistant/binary_sensor/simppeliTCU_%s/%s/config", vid, door.entityId);

    StringBuffer<1024> payload;
    payload.format("{\"unique_id\":\"simppeliTCU_%s_%s\",\"name\":\"%s\",\"state_topic\":\"%s%s\",\"device_class\":\"door\",\"payload_on\":\"yes\",\"payload_off\":\"no\"",
                   vid, door.entityId, door.name, mqttPrefix.c_str(), door.stateTopic);

    appendCommonFields(payload, mqttPrefix);

    mqttClient.publish(topic.c_str(), (const uint8_t*)payload.c_str(), payload.length(), true);
  }
}