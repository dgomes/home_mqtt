#include <MemoryFree.h>
#include "ha_mqtt.h"

HA_Device::HA_Device(const IPAddress &server, const int server_port, const char *name, const uint8_t mac[6])
{
    mqttClient = PubSubClient(server, server_port, ethClient);
    mqttClient.setBufferSize(512);

    snprintf(this->name, 32, "%s", name);
    snprintf(this->mac_address, 16, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    snprintf(this->base_topic, 64, "%s%s/", MQTT_BASE_TOPIC, this->name);
    this->time = 2000;
}

unsigned HA_Device::get_base_topic_length() { return strlen(this->base_topic); }

void HA_Device::set_discovery_topic(char *discovery_topic, const char *device, const char *subtopic)
{
    snprintf(discovery_topic, 128, "homeassistant/%s/%s/%s/config", device, name, subtopic);
}

bool HA_Device::publish_property(const char *property, const char *value,
                                 bool retain = false)
{
    char prop[64];
    snprintf(prop, 64, "%s%s", base_topic, property);
    bool rc = mqttClient.publish(prop, value, retain);
    if (!rc)
    {
        char fail_str[36];
        snprintf(fail_str, 36, "failed publish_property at %ld", millis() / 1000);
        mqttClient.publish("debug", fail_str, retain);
    }
    return rc;
}

bool HA_Device::subscribe_property(const char *property)
{
    char prop[64];
    snprintf(prop, 64, "%s%s/set", base_topic, property);
    return mqttClient.subscribe(prop);
}

void HA_Device::setup(const char *manufacturer, const char *model, const char *fw_version, MQTT_CALLBACK_SIGNATURE)
{
    mqttClient.setCallback(callback);

    snprintf(ha_dev, 128,
             "\"dev\":{\"name\": \"%s\", \"ids\":[\"%s\"], \"mf\":\"%s\", \"mdl\":\"%s\"}",
             name, mac_address, manufacturer, model);

    while (!connect())
    {
        delay(1000);
    }

    Serial.println("HA_Device::setup DONE");
}

bool HA_Device::connect()
{
    Serial.println("HA_Device::Connect");

    char status_topic[24];
    snprintf(status_topic, 24, "%s%s", base_topic, "status");

    if (!mqttClient.connect(name, NULL, NULL, status_topic, 0, true, "offline", 1))
    {
        Serial.print("MQTT State = ");
        Serial.println(mqttClient.state());
        return false;
    }
    mqttClient.publish(status_topic, "online", true);

    return true;
}

void HA_Device::discovery_light(const char *subtopic, unsigned pushtime)
{
    bool rc = true;
    char buffer[DISC_BUFFER_SIZE];

    // Configure Home Assistant discovery
    snprintf(buffer, DISC_BUFFER_SIZE,
             "{\"~\": \"%s\", %s, \"avty_t\": \"~status\", "
             "\"cmd_t\":\"~%s/set\", \"optimistic\":\"true\", \"schema\": \"basic\", "
             "\"name\":\"%s\", \"pl_off\":\"%d\", \"pl_on\":\"%d\", "
             "\"uniq_id\":\"light_%s_%s\"}",
             base_topic, ha_dev,
             subtopic,
             subtopic, pushtime, pushtime,
             mac_address, subtopic);

    char discovery_topic[128];
    set_discovery_topic(discovery_topic, "light", subtopic);
    rc = mqttClient.publish(discovery_topic, buffer, false);

    // Subscribe subtopic control
    if(rc)
        rc = subscribe_property(subtopic);
    // Process Error
    if (!rc)
    {
        snprintf(buffer, DISC_BUFFER_SIZE, "Failed to advertise switch %s %s= %d", subtopic, buffer, rc);
        mqttClient.publish("debug", buffer);
    }
}

void HA_Device::discovery_switch(const char *subtopic, unsigned pushtime = 0)
{
    bool rc = true;
    char buffer[DISC_BUFFER_SIZE];

    char switch_type_buf[80];
    if (pushtime)
    {
        // we actually switch a contactor
        snprintf(switch_type_buf, 80,
                 "\"optimistic\":\"true\", \"pl_on\": \"%d\", \"pl_off\": \"%d\", ",
                 pushtime, pushtime);
    }
    else
    {
        // local switch with proper state
        snprintf(switch_type_buf, 80,
                 "\"stat_t\":\"~%s\", ",
                 subtopic);
    }

    // Configure Home Assistant discovery
    snprintf(buffer, DISC_BUFFER_SIZE,
             "{\"~\": \"%s\", %s, \"avty_t\": \"~status\", "
             "\"cmd_t\":\"~%s/set\", %s"
             "\"name\":\"%s\", "
             "\"uniq_id\":\"switch_%s_%s\"}",
             base_topic, ha_dev,
             subtopic, switch_type_buf,
             subtopic,
             mac_address, subtopic);

    char discovery_topic[128];
    set_discovery_topic(discovery_topic, "switch", subtopic);
    rc = mqttClient.publish(discovery_topic, buffer, false);

    // Subscribe subtopic control
    if(rc)
        rc = subscribe_property(subtopic);
    // Process Error
    if (!rc)
    {
        snprintf(buffer, DISC_BUFFER_SIZE, "Failed to advertise switch %s = %d", subtopic, rc);
        mqttClient.publish("debug", buffer);
    }
}

void HA_Device::discovery_sensor(const char *subtopic, const char *device_class, const char *value, const char *unit, int index = -1)
{
    bool rc = true;
    char buffer[DISC_BUFFER_SIZE];

    char array_index[4] = "";   //if index = -1 we don't have array_index
    if (index >= 0)
    {
        snprintf(array_index, 4, "[%]", index);
    }
    // Configure Home Assistant discovery
    char sensor[64];
    snprintf(sensor, 64, "%s_%s_%d", subtopic, value, index);
    snprintf(buffer, DISC_BUFFER_SIZE,
             "{\"~\": \"%s\", %s, \"avty_t\": \"~status\", "
             "\"dev_cla\": \"%s\", "
             "\"name\":\"%s\", \"unit_of_meas\":\"%s\", "
             "\"stat_t\":\"~%s\", \"uniq_id\":\"sensor_%s_%s_%s_%d\", "
             "\"val_tpl\":\"{{ value_json.%s%s }}\"}",
             base_topic, ha_dev,
             device_class,
             sensor, unit,
             subtopic, mac_address, subtopic, value, index,
             value, array_index);

    char discovery_topic[128];
    set_discovery_topic(discovery_topic, "sensor", sensor);
    rc = mqttClient.publish(discovery_topic, buffer);

    // Process Error
    if (!rc)
    {
        snprintf(buffer, DISC_BUFFER_SIZE, "Failed to advertise sensor %s %s %d = %d", subtopic, value, index, rc);
        mqttClient.publish("debug", buffer);
    }
}

bool HA_Device::loop()
{
    if (!mqttClient.connected())
    {
        long now = millis();
        if (now - this->lastReconnectAttempt > RECONNECT_DELAY)
        {
            this->lastReconnectAttempt = now;
            // Attempt to reconnect
            if (this->connect())
            {
                this->lastReconnectAttempt = 0;
                return false;
            }
        }
    }

    if ((millis() / UPTIME_REPORT_PERIOD) != time)
    {
        time = millis() / UPTIME_REPORT_PERIOD;
        char *ip_str = "xxx.xxx.xxx.xxx";
        snprintf(ip_str, 16, "%d.%d.%d.%d", Ethernet.localIP()[0], Ethernet.localIP()[1], Ethernet.localIP()[2], Ethernet.localIP()[3]);

        char buffer[128];
        snprintf(buffer, 128,
                 "{\"uptime\": %ld, \"freeMemory\": %d, \"ip\": \"%s\" }",
                 millis() / 1000,
                 freeMemory(),
                 ip_str);
        publish_property("info", buffer);
    }

    Ethernet.maintain();
    return mqttClient.loop();
}
