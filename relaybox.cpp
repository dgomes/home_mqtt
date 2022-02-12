#include "relaybox.h"

RelayBox::RelayBox(Model m) {
    this->callback = NULL;
    switch(m) {
        case UNDEF: 
            relay = NULL;
            relays_len = 0;
            break;
        case _34R:
            relay = plc_m_duino_34R;
            relays_len = sizeof(plc_m_duino_34R)/sizeof(int); 
            break;
        case _20IO:
            relay = plc_ardbox_20IO_rele;
            relays_len = sizeof(plc_ardbox_20IO_rele)/sizeof(int); 
    }
    //Configuration Relay OUT 
    for(uint8_t i=1; i<=relays_len; i++) {
        pinMode(relay[i], OUTPUT);
    }
}

void RelayBox::setup(RELAYBOX_CALLBACK_SIGNATURE) {
    this->callback = callback;
}

void RelayBox::switchRelay(int i, bool mode) {  //true = HIGH, false = LOW
    if(relay==NULL)
        return;
    if(i>=relays_len)
        return;

    if(mode) {
        DEBUG_PRINTLN(String("Relay(")+String(i)+String(") ... ON"));
    } else {
        DEBUG_PRINTLN(String("Relay(")+String(i)+String(") ... OFF"));
    }
    digitalWrite(relay[i],mode?HIGH:LOW);
    if(this->callback)
        this->callback(i, mode);
}

// Turn relay ON and program relay to turn OFF in the future
void RelayBox::switchRelay(int i, unsigned long period) {
    if(i>=relays_len)
        return;
    switchRelay(i,true);
    
    relay_sleep[i] = period;
}

//Switch AFTER after period
void RelayBox::loop() {
    unsigned long now = millis();

    //keep track of time of last loop
    unsigned long elapsed_time = now - relay_sleep[0];
   
    for(int i=1; i< relays_len; i++) {
        if(relay_sleep[i] != 0) {
            if((long) (relay_sleep[i] - elapsed_time) <= 0) {
                switchRelay(i,false);
                relay_sleep[i] = 0;
            } else {
                relay_sleep[i] -= elapsed_time;
            }
        }
    }

    relay_sleep[0] = now;
}
