#include <avr/wdt.h>

#include "relaybox.h"
#include "emontx.h"
#include "ha_mqtt.h"

const IPAddress server(192, 168, 1, 100);
const int server_port = 1883;

const uint8_t mac[6] = {0x72, 0x00, 0x05, 0xbd, 0x84, 0xe4};
const byte ip[] = {192, 168, 1, 53};

const char *device_name = "m-duinoA";

#define MAX_MDUINO_RELAY 16
#define MAX_ARDBOX_RELAY 8

/*
1 - 16 : part of m-duino
101-108 : part of remote ardbox
*/

typedef struct
{
  char subtopic[12];
  uint8_t type;     //  see ha_mqtt.h
  uint32_t payload; // 0 = ON/OFF ; number = milliseconds of operation
} relay;

relay relay_conf[] = {
    {"relay_1", COVER, 0},
    {"relay_2", COVER, 0},
    {"relay_3", COVER, 0},
    {"relay_4", COVER, 0},
    {"relay_5", COVER, 0},
    {"relay_6", LIGHT, 950},
    {"relay_7", LIGHT, 850},
    {"relay_8", COVER, 0},
    {"relay_9", LIGHT, 950},
    {"relay_10", LIGHT, 850},
    {"relay_11", LIGHT, 850},
    {"relay_12", DISABLED, 0},
    {"relay_13", DISABLED, 0},
    {"relay_14", DISABLED, 0},
    {"relay_15", DISABLED, 0},
    {"relay_16", SWITCH, 1050},
    {"relay_101", COVER, 0},
    {"relay_102", COVER, 0},
    {"relay_103", COVER, 0},
    {"relay_104", COVER, 0},
    {"relay_105", SWITCH, 1050},
    {"relay_106", COVER, 0},
    {"relay_107", COVER, 0},
    {"relay_108", LIGHT, 850},
};

typedef struct
{
  char subtopic[12];
  uint8_t up;          // relay number
  uint8_t down;        // relay number
  uint32_t traveltime; // milliseconds of operation
  char state;
} cover;

cover cover_conf[]{
    {"cover_1", 8, 3, 32000, 'S'}, // (S)topped is the initial state of the cover
    {"cover_2", 5, 4, 32000, 'S'},
    {"cover_3", 2, 1, 25000, 'S'},
    {"cover_4", 102, 103, 30000, 'S'},
    {"cover_5", 101, 106, 13000, 'S'},
    {"cover_6", 104, 107, 32000, 'S'},
};

HA_Device ha_device(server, server_port, device_name, mac);
RelayBox mduino(_34R);

// RelayBox callback whenever there is a state change
void relay_callback(uint8_t relay, bool mode)
{ // true = HIGH, false = LOW
  char relay_buf[10];
  snprintf(relay_buf, 10, "relay_%d", relay);
  ha_device.publish_property(relay_buf, mode ? "ON" : "OFF");

  for (int i = 0; i < sizeof(cover_conf) / sizeof(cover); i++)
  {
    cover *curr = &cover_conf[i];

    if (relay == curr->up and curr->state == 'O' && !mode)
    {
      ha_device.publish_property(curr->subtopic, "open");
      break;
    }
    else if (relay == curr->down and curr->state == 'C' && !mode)
    {
      ha_device.publish_property(curr->subtopic, "closed");
      break;
    }
  }
}

void activate_relay(int relay_number, bool mode, unsigned long push_time = 0)
{
  char *ardbox_cmd = "#,12345678"; // relay_number,pushtime/true/false
  bool is_ardbox = (relay_number > 100 and relay_number <= (100 + MAX_ARDBOX_RELAY));
  bool is_mduino = (relay_number > 0 and relay_number <= MAX_MDUINO_RELAY);

  if (!is_ardbox and !is_mduino)
  {
    ha_device.publish_property("debug", "unknown relay_number activation");
    return;
  }

  if (push_time > 0)
  {
    if (is_ardbox)
    {
      snprintf(ardbox_cmd, 16, "%d,%ld", relay_number - 100, push_time);
      Serial2.println(ardbox_cmd);
    }
    else
    {
      mduino.switchRelay(relay_number, push_time);
    }
  }
  else
  {
    if (is_ardbox)
    {
      snprintf(ardbox_cmd, 16, "%d,%s", relay_number - 100, mode ? "true" : "false");
      Serial2.println(ardbox_cmd);
    }
    else
    {
      mduino.switchRelay(relay_number, mode);
    }
  }
}

// HA_Device will call this callback for all subscriptions
void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
  char *pEnd;
  int relay_number = strtol((const char *)topic + ha_device.get_base_topic_length() + strlen("relay_"), &pEnd, 10); // luckly cover_ is the same size as relay_

  char sanatized_payload[length + 1];
  strncpy(sanatized_payload, (const char *)payload, length);
  sanatized_payload[length] = NULL;
  unsigned long push_time = strtol(sanatized_payload, &pEnd, 10);

  if (strncmp(HA_STATUS, (const char *)topic, 11) == 0 and strncmp((const char *)payload, HA_STATUS_PAYLOAD_ONLINE, 6) == 0)
  {
    discovery();
    ha_device.publish_property("status", HA_STATUS_PAYLOAD_ONLINE);
    return;
  }

  if (strncmp("cover_", (const char *)topic + ha_device.get_base_topic_length(), 6) == 0)
  {
    cover *curr = &cover_conf[relay_number - 1];

    if (strncmp(sanatized_payload, HA_PAYLOAD_OPEN, 4) == 0)
    {
      curr->state = 'O'; // Open
      activate_relay(curr->down, false);
      delay(500);
      activate_relay(curr->up, true, (unsigned long)curr->traveltime);
      ha_device.publish_property(curr->subtopic, HA_STATE_OPENING);
    }
    else if (strncmp(sanatized_payload, HA_PAYLOAD_CLOSE, 5) == 0)
    {
      curr->state = 'C'; // Close
      activate_relay(curr->up, false);
      delay(500);
      activate_relay(curr->down, true, (unsigned long)curr->traveltime);
      ha_device.publish_property(curr->subtopic, HA_STATE_CLOSING);
    }
    else if (strncmp(sanatized_payload, HA_PAYLOAD_STOP, 4) == 0)
    {
      curr->state = 'S'; // Stop
      activate_relay(curr->up, false);
      activate_relay(curr->down, false);
      ha_device.publish_property(curr->subtopic, HA_STATE_STOPPED);
    }

    return;
  }

  if (relay_number >= 1 && relay_number <= MAX_ARDBOX_RELAY + 100)
  {
    if (push_time > 0)
    {
      activate_relay(relay_number, true, push_time);
    }
    else if (strncmp(sanatized_payload, "ON", 2) == 0)
    {
      activate_relay(relay_number, true);
    }
    else if (strncmp(sanatized_payload, "OFF", 3) == 0)
    {
      activate_relay(relay_number, false);
    }
    else
    {
      DEBUG_PRINTLN(String(topic) + "\t" + String(sanatized_payload));
    }
    return;
  }
}

void discovery()
{
  // Switch and Light discovery
  for (int i = 0; i < sizeof(relay_conf) / sizeof(relay); i++)
  {
    maintain();
    if (relay_conf[i].type == SWITCH)
    {
      ha_device.discovery_switch(relay_conf[i].subtopic, relay_conf[i].payload);
    }
    else if (relay_conf[i].type == LIGHT)
    {
      ha_device.discovery_light(relay_conf[i].subtopic, relay_conf[i].payload);
    }
  }

  // Cover discovery
  for (int i = 0; i < sizeof(cover_conf) / sizeof(cover); i++)
  {
    maintain();
    ha_device.discovery_cover(cover_conf[i].subtopic);
  }

  // EmonTx discovery
  for (uint8_t i = 0; i < 4; i++)
  {
    maintain();
    ha_device.discovery_sensor("emontx", "power", "ct", "W", i, NULL, "measurement");
  }
  ha_device.discovery_sensor("emontx", "voltage", "Vrms", "V", -1, NULL, "measurement");
  ha_device.discovery_sensor("info", NULL, "uptime", "s", -1, "diagnostic");
  ha_device.discovery_sensor("info", NULL, "freeMemory", "b", -1, "diagnostic");
}

void setup()
{
  // setup watchdog
  wdt_enable(WDTO_8S);

  Serial.begin(9600);
  Serial.println("M-DUINO HA_Device v3");

  Serial1.begin(9600);      // emontx
  Serial1.setTimeout(2100); // we have a reading every 2 seconds

  Serial2.begin(2400); // ardbox

  Ethernet.begin(mac); // Ethernet.begin(mac, ip);
  delay(1000);

  Serial.print("localIP: ");
  Serial.println(Ethernet.localIP());

  mduino.setup(relay_callback);
  ha_device.setup("industrial shields", "m-duino", "0.3.0", mqtt_callback);

  /* Setup done, now moving into discovery */
  discovery();

  Serial.println("setup() done");
}

void maintain()
{
  wdt_reset();
  ha_device.loop();
  mduino.loop();
  delay(50);
}

/* Test if we open and close {} and "" */
bool tentative_json(const char *json, unsigned length)
{
  bool has_brackets = false;
  int brackets = 0;
  int quotation_mark = 0;

  if (length < 2)
    return false;

  for (int i = 0; i < length; i++)
  {
    if (json[i] == '{')
    {
      has_brackets = true;
      brackets++;
    }
    else if (json[i] == '}')
      brackets--;
    else if (json[i] == '"' && (quotation_mark & 1 == quotation_mark))
      quotation_mark--;
    else if (json[i] == '"')
      quotation_mark++;
  }

  return has_brackets && (brackets == quotation_mark) && (brackets == 0);
}

void loop()
{
  // emontx
  if (Serial1.available())
  {
    char emontx_buf[64];
    int len = Serial1.readBytesUntil('\n', emontx_buf, 64);
    emontx_buf[len] = NULL;
    if (tentative_json(emontx_buf, strlen(emontx_buf)))
      ha_device.publish_property("emontx", emontx_buf);
    else
      ha_device.publish_property("debug", emontx_buf);
  }
  maintain();

  // RS232 interface with ardbox PLC
  if (Serial2.available())
  {
    char buf[9];                                           // format:  #,bool
    int len = Serial2.readBytesUntil('\n', buf, 9);        // reads a relay number and bool, e.g.:  7,true
    if (strlen(buf) > 3 && buf[0] >= '0' && buf[0] <= '9') // checks that next line is sane
    {
      relay_callback(100 + (buf[0] - '0'), buf[2] == 't'); // t = true else false
    }
  }
  maintain();
}
