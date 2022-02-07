#include <relaybox.h>
#include <avr/wdt.h>

#include "ha_mqtt.h"
#include "emontx.h"

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
  char type;        //  see ha_mqtt.h
  uint32_t payload; // 0 = ON/OFF ; number = milliseconds of operation
} relay;

relay relay_conf[] = {
    {"relay_1", SWITCH, 0},
    {"relay_2", LIGHT, 2000},
    {"relay_3", SWITCH, 0},
    {"relay_4", SWITCH, 4000},
    {"relay_5", SWITCH, 0},
    {"relay_6", SWITCH, 0},
    {"relay_7", SWITCH, 0},
    {"relay_8", SWITCH, 0},
    {"relay_9", SWITCH, 0},
    {"relay_10", SWITCH, 0},
    {"relay_11", SWITCH, 0},
    {"relay_12", SWITCH, 0},
    {"relay_13", SWITCH, 0},
    {"relay_14", SWITCH, 0},
    {"relay_15", SWITCH, 0},
    {"relay_16", SWITCH, 0},
    {"relay_101", SWITCH, 0},
    {"relay_102", SWITCH, 0},
    {"relay_103", SWITCH, 0},
    {"relay_104", SWITCH, 0},
    {"relay_105", SWITCH, 0},
    {"relay_106", SWITCH, 0},
    {"relay_107", SWITCH, 0},
    {"relay_108", SWITCH, 0},
};

HA_Device ha_device(server, server_port, device_name, mac);
RelayBox mduino(_34R);

// RelayBox callback whenever there is a state change
void relay_callback(uint8_t i, bool mode)
{ // true = HIGH, false = LOW
  char relay_buf[10];
  snprintf(relay_buf, 10, "relay_%d", i);
  ha_device.publish_property(relay_buf, mode ? "ON" : "OFF");
}

// HA_Device will call this callback for all subscriptions
void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
  char *pEnd;
  int relay_number = strtol((const char *)topic + ha_device.get_base_topic_length() + strlen("relay_"), &pEnd, 10);

  char sanatized_payload[length + 1];
  strncpy(sanatized_payload, (const char *)payload, length);
  sanatized_payload[length] = NULL;
  unsigned long push_time = strtol(sanatized_payload, &pEnd, 10);

  if (relay_number >= 1 && relay_number <= MAX_MDUINO_RELAY)
  {
    if (push_time > 0)
    {
      mduino.switchRelay(relay_number, push_time);
    }
    else if (strncmp(sanatized_payload, "ON", 2) == 0)
    {
      mduino.switchRelay(relay_number, true);
    }
    else if (strncmp(sanatized_payload, "OFF", 3) == 0)
    {
      mduino.switchRelay(relay_number, false);
    }
    else
    {
      DEBUG_PRINTLN(String(topic) + "\t" + String(sanatized_payload));
    }
    return;
  }

  if (relay_number >= 101 && relay_number <= (100 + MAX_ARDBOX_RELAY))
  {
    relay_number -= 100;
    DEBUG_PRINTLN(String("> ") + String(relay_number) + "," + String(push_time));

    if (push_time > 0)
    {
      Serial2.println(String(relay_number) + "," + String(push_time));
    }
    else if (strncmp(sanatized_payload, "ON", 2) == 0)
    {
      Serial2.println(String(relay_number) + ",true");
    }
    else if (strncmp(sanatized_payload, "OFF", 3) == 0)
    {
      Serial2.println(String(relay_number) + ",false");
    }
    else
    {
      DEBUG_PRINTLN(String(topic) + "\t" + String(sanatized_payload));
    }
    return;
  }
}

void setup()
{
  // setup watchdog
  wdt_enable(WDTO_8S);

  Serial.begin(9600);
  Serial.println("M-DUINO HA_Device v3");

  Serial1.begin(9600);      // emontx
  Serial1.setTimeout(2000); // we have a reading every 2 seconds

  Serial2.begin(2400); // ardbox

  Ethernet.begin(mac, ip);
  delay(1000);

  Serial.print("localIP: ");
  Serial.println(Ethernet.localIP());

  mduino.setup(relay_callback);
  ha_device.setup("industrial shields", "m-duino", "0.3.0", mqtt_callback);

  for (int i = 0; i < MAX_MDUINO_RELAY + MAX_ARDBOX_RELAY; i++) // 16 relays m-duino + 8 relays from Ardbox
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

  // EmonTx discovery
  for (uint8_t i = 0; i < 4; i++)
  {
    maintain();
    ha_device.discovery_sensor("emontx", "power", "ct", "W", i);
  }
  ha_device.discovery_sensor("emontx", "voltage", "Vrms", "V");

  Serial.println("setup() done");

  Serial.println(sizeof(relay_conf));
}

void maintain()
{
  wdt_reset();
  ha_device.loop();
  mduino.loop();
  delay(50);
}

void loop()
{
  // emontx
  if (Serial1.available())
  {
    char emontx_buf[64];
    int len = Serial1.readBytesUntil('\n', emontx_buf, 64);
    emontx_buf[len] = NULL;
    if (emontx_buf[0] == '{' && strlen(emontx_buf) > 0)
      ha_device.publish_property("emontx", emontx_buf);
  }
  maintain();

  // RS232 interface with ardbox PLC
  if (Serial2.available())
  {
    char buf[64];
    int len = Serial2.readBytesUntil('\n', buf, 64); // # true/false - relay number and bool
    buf[len] = NULL;
    if (strlen(buf) > 0)
    { // check we don't fail next line even if it is just garbage
      char *relay_buf = "relay_10X";
      relay_buf[8] = buf[0];                                               // fill in the relay number (suffix)
      ha_device.publish_property(relay_buf, buf[2] == 't' ? "ON" : "OFF"); // t = true else false
    }
  }
  maintain();
}
