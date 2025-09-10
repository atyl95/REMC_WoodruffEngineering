#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <RPC.h>

class Logger {
public:
    // Initialize the logger with debugging preference
    static void init(uint core_idx = 0) {
        CORE_IDX = core_idx;
        if (CORE_IDX == 0)
        {
            Serial.begin(115200);
            while (!Serial) { }    // wait (native-USB boards)
        }
        else
        {
            RPC.begin();
        }
    }
    
    static void log(const String& msg) {
        if (CORE_IDX == 0) {
            Serial.println(msg);
        } else {
            RPC.println(msg);
        }
    }
    
private:
    static bool CORE_IDX;
};

#endif // LOGGER_H
