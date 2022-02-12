/*
 HA_device.h - A simple client for MQTT using HA_device convention.
  Diogo Gomes
  http://diogogomes.com
*/

#ifndef HA_device_h
#define HA_device_h

#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

#include <PubSubClient.h>
#include <EthernetENC.h>

#define HA_PAYLOAD_OPEN "OPEN"
#define HA_PAYLOAD_CLOSE "CLOSE"
#define HA_PAYLOAD_STOP "STOP"
#define HA_STATE_OPENING  "opening"
#define HA_STATE_CLOSING  "closing"
#define HA_STATE_STOPPED  "stopped"
#define HA_STATUS "hass/status"
#define HA_STATUS_PAYLOAD_ONLINE  "online"
#define HA_STATUS_PAYLOAD_OFFLINE  "offline"

#define MQTT_BASE_TOPIC "devices/"
#define RECONNECT_DELAY 5000
#define UPTIME_REPORT_PERIOD 60000

#define DISC_BUFFER_SIZE  448

#define LIGHT 0
#define SWITCH 1
#define COVER 2
#define SENSOR 3
#define DISABLED 4

class HA_Device
{
private:
  char name[32];
  char mac_address[16];
  char base_topic[64];
  char ha_dev[128];   // JSON substring with device information
  unsigned long time; // var created to show uptime more close to zero
                      // milliseconds as possible
  unsigned long lastReconnectAttempt = 0;

  bool connect();
  void set_discovery_topic(char *discovery_topic, const char *device, const char *subtopic);

public:
  EthernetClient ethClient;
  PubSubClient mqttClient;

  HA_Device(const IPAddress &server, const int server_port, const char *name, const uint8_t *mac);

  void setup(const char *manufacturer, const char *model, const char *fw_version, MQTT_CALLBACK_SIGNATURE);
  bool loop();

  void discovery_cover(const char *subtopic);
  void discovery_switch(const char *subtopic, unsigned pushtime = 0);
  void discovery_light(const char *subtopic, unsigned pushtime = 1000);
  void discovery_sensor(const char *subtopic, const char *device_class, const char *value, const char *unit, int index = -1, const char *entity_category=NULL);

  bool publish_property(const char *property, const char *value, bool retain = false);
  bool subscribe_property(const char *property);
  unsigned get_base_topic_length();
};

#endif
