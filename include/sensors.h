#pragma once

#include "globals.h"

class SensorManager
{
public:
    static void begin();
    static void Update();

private:
#ifdef DHT11_PIN
    static bool ReadDHT11(int pin, float& tempF, float& humidity);
#endif
};
