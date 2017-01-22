const String readSerial1() {
    String response = "";
    bool begin = false;
    uint8_t len = 0;
    while (true) {
        char in = Serial1.read();
        len++;

        if (in == '{') {
            begin = true;
            len = 0;
        }

        if (begin) response += (in);

        if (in == '}' || len > 128) break;  //FAILSAFE: we jump off the loop after 128bytes

        delay(10);
    }
    while(Serial1.available())  //remove everything else in the buffer
        Serial1.read();

    DEBUG_PRINTLN(response);
    return response;
}

const char *emontx_loop() {
    if (Serial1.available()) {
        String reading = readSerial1();
        if(reading.length()) 
            return reading.c_str();
    }
    return NULL;
}
