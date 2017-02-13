#include <relaybox.h>
#include <UIPEthernet.h>
#include <homie.h>

#include "emontx.h"

IPAddress server(192,168,1,10);
int server_port = 1883;
uint8_t mac[6] = {0x72,0x00,0x05,0xbd,0x83,0xe1};

EthernetClient ethClient;
PubSubClient mqttClient(ethClient);

#define MAX_MDUINO_RELAY 16
#define MAX_ARDBOX_RELAY 8
/* 
1 - 16 : part of m-duino
101-108 : part of remote ardbox 
*/
const char *nodes[] = {"relay/1", 
                       "relay/2", 
                       "relay/3", 
                       "relay/4",                     
                       "relay/5", 
                       "relay/6", 
                       "relay/7",                      
                       "relay/8", 
                       "relay/9", 
                       "relay/10",                      
                       "relay/11", 
                       "relay/12", 
                       "relay/13",                     
                       "relay/14", 
                       "relay/15", 
                       "relay/16",
                       "relay/101",
                       "relay/102",
                       "relay/103",
                       "relay/104",
                       "relay/105",
                       "relay/106",
                       "relay/107",
                       "relay/108",
                       };  

Homie homie(mqttClient, String("m-duino"), nodes, MAX_MDUINO_RELAY+MAX_ARDBOX_RELAY); //16 relays + 8 relays
RelayBox mduino(_34R);

// RelayBox callback whenever there is a state change
void relay_callback(uint8_t i, bool mode) {  //true = HIGH, false = LOW
  homie.publish_property(String("relay/")+String(i), mode?"true":"false");
}

// Homie will call this callback for all "<node>/set" subscriptions
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  char msg[length+1];
  memset(msg, 0, length+1);
  strncpy(msg, (const char *) payload, length);

  int relay_number = atoi((const char *)topic + homie.base_topic().length()+strlen("relay/"));

  if(relay_number >= 1 && relay_number <= MAX_MDUINO_RELAY) {
    if(atoi(msg)>0) {
      mduino.switchRelay(relay_number, atoi(msg));
    } else if(strncmp(msg, "true", 4)==0) {
      mduino.switchRelay(relay_number,true);
    } else if(strncmp(msg, "false", 5)==0) {
      mduino.switchRelay(relay_number,false);
    } else {
      DEBUG_PRINTLN(String(topic) + "\t" + String(msg));
    }
  }

  if(relay_number >= 101 && relay_number <= (100+MAX_ARDBOX_RELAY)) {
    relay_number-=100;
    if(atoi(msg)>0) {
      Serial2.println(String(relay_number)+","+String(atoi(msg)));
      DEBUG_PRINTLN(String("> ")+String(relay_number)+","+String(atoi(msg)));
    } else if(strncmp(msg, "true", 4)==0) {
      Serial2.println(String(relay_number)+",true");
    } else if(strncmp(msg, "false", 5)==0) {
      Serial2.println(String(relay_number)+",false");
    } else {
      DEBUG_PRINTLN(String(topic) + "\t" + String(msg));
    }
  }
  
}


void setup() {
  Serial.begin(9600);
  Serial.println("M-DUINO HOMIE v2");

  Serial1.begin(9600); //emontx
  Serial2.begin(2400); //ardbox

  Ethernet.begin(mac);
  
  Serial.print("localIP: ");
  Serial.println(Ethernet.localIP());
  byte myip[4] = {Ethernet.localIP()[0], Ethernet.localIP()[1], Ethernet.localIP()[2], Ethernet.localIP()[3]};

  mqttClient.setServer(server, server_port);
  mduino.setup(relay_callback);
  homie.setBrand("industrial shields");
  homie.setFirmware("mduino", "0.1.0");
  homie.setup(myip , mqtt_callback);
}

void loop() {
  Ethernet.maintain();
  homie.loop();
  mduino.loop();

  //RS232 interface with ardbox PLC
  if (Serial2.available()) {
    String cp = Serial2.readStringUntil('\n');
    cp.trim();
    DEBUG_PRINTLN("Serial2: "+cp);
    if(cp.length()>2) //check we don't fail next line even if it is just garbage
      homie.publish_property(String("relay/10")+cp.substring(0,1), cp.substring(2));
  }

  //emontx
  if (Serial1.available()) {
    String reading = readSerial1();
    if(reading.length()) 
      homie.publish_property("emontx", reading);
  }
  delay(100);
}
