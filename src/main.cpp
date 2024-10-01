#include "Battery.h"
#include <Arduino.h>


Battery battery;

void setup() {
    // Initialize the battery library
    battery.setup();
}   

void loop() {
    
        battery.loop(); // Add battery.loop() here

}