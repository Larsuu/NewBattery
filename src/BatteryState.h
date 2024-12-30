// BatteryState.h
#ifndef BATTERY_STATE_H
#define BATTERY_STATE_H

#include <Arduino.h> // Include necessary libraries

// Forward declarations for state types
enum VoltageState { 
    LAST_RESORT             =   0,
    LOW_VOLTAGE_WARNING     =   1,  
    PROTECTED               =   2,
    ECONOMY                 =   3,
    BOOST                   =   4,  
    VBOOST_RESET            =   5   
};

enum TempState {
    SUBZERO                 =  0,
    ECO_TEMP                =  1,
    BOOST_TEMP              =  2,
    OVER_TEMP               =  3,
    TEMP_WARNING            =  4,
    TBOOST_RESET            =  5,
    DEFAULT_TEMP            =  6
};


struct batteryState {
private:
    int error;

public:
  
    // Member variables
    String          name;                // Name of the battery
    uint8_t         size;                // Size of the battery
    float           temperature;         // Current temperature
    uint8_t         wantedTemp;          // Desired temperature
    uint32_t        milliVoltage;        // Voltage in millivolts
    uint8_t         voltageInPrecent;    // Voltage percentage
    uint8_t         ecoVoltPrecent;      // Eco voltage percentage
    uint8_t         boostVoltPrecent;    // Boost voltage percentage
    uint8_t         capct;               // Capacity
    bool            voltBoost;           // Flag for voltage boost
    bool            tempBoost;           // Flag for temperature boost
    TempState       tState;              // Current temperature state
    VoltageState    vState;              // Current voltage state
    TempState       prevTState;          // Previous temperature state
    VoltageState    prevVState;          // Previous voltage state
    bool            firstRun;            // Flag for first run
    uint32_t        lastMessageTime;     // Last message time
    int             sizeApprx;           // Approximate size

    // Nested struct for charger
    struct charger {
        uint8_t     current; // Charger current
        bool        enable;  // Enable/disable charger
        uint32_t    time;    // Last message time
    } chrgr;

    // Nested struct for timer
    struct timer {
        uint32_t    voltMs;       // Voltage measurement time
        uint32_t    voltFet;      // Voltage FET time
        uint32_t    tempMillis;   // Temperature measurement time
        uint32_t    heaterMillis; // Heater control time
    } timer;

    // Nested struct for heater
    struct resistor {
        uint8_t     resistance; // Heater resistance
        uint8_t     ecoTemp;    // Eco temperature
        uint8_t     boostTemp;  // Boost temperature
        uint8_t     maxPower;   // Maximum power
        float       pidP;       // PID proportional
        float       pidI;       // PID integral
        float       pidD;       // PID derivative
        uint32_t    time;       // Last message time
    } heater;

    // Nested struct for WiFi
    struct wifi {
        bool        enable; // Enable/disable WiFi
        String      ssid;   // WiFi SSID
        String      pass;   // WiFi password
        uint32_t    time;   // Last message time
    } wlan;

    // Nested struct for HTTP
    struct lan {
        bool        enable;   // Enable/disable HTTP
        String      username; // HTTP username
        String      password; // HTTP password
    } http;

    // Nested struct for MQTT
    struct mosquitto {
        bool        enable;   // Enable/disable MQTT
        String      username; // MQTT username
        String      password; // MQTT password
        String      server;   // MQTT server
        uint16_t    port;     // MQTT port
        uint32_t    lastMessageTime; // Last message time
    } mqtt;

    // Nested struct for Telegram
    struct tg {
        bool        enable;   // Enable/disable Telegram
        String      token;    // Telegram token
        int         chatId;   // Telegram chat ID
        uint32_t    lastMessageTime; // Last message time
    } telegram;

    // Nested struct for sTune
    struct stune {
        float       inputSpan;    // Input span
        float       outputSpan;   // Output span
        float       outputStart;  // Output start
        float       outputStep;   // Output step
        uint32_t    testTimeSec;  // Test time
        uint32_t    settleTimeSec; // Settle time
        uint32_t    samples;      // Samples
        float       tempLimit;    // Temperature limit
        uint32_t    time;         // Last message time
    } stune;

    struct adc {
        uint32_t    raw;        // Raw ADC value
        uint32_t    cal;        // Calibrated ADC value
        uint32_t    time;       // Last message time
        uint32_t    index;      // Index
        uint32_t    sum;        // Sum
        uint32_t    avg;        // Average
        uint8_t     mAvg;
        uint32_t    readings[5];

    } adc;

  // Public constructor to initialize batteryState with default values
    batteryState(uint8_t initialSize = 7, float initialTemperature = 25.0, String initialName = "Onni")
        : size(initialSize), 
          temperature(initialTemperature),
          name(initialName),
          ecoVoltPrecent(50), 
          boostVoltPrecent(80),
          voltBoost(false),
          tempBoost(false),
          firstRun(true),
          tState(SUBZERO),
          vState(LAST_RESORT),   
          capct(10),     
          chrgr{1, false, 0}, // Initialize charger struct
          timer{0, 0, 0, 0}, // Initialize timer struct
          heater{0, 0, 0, 0, 0.0, 0.0, 0.0, 0}, // Initialize heater struct
          wlan{false, "", "", 0}, // Initialize WiFi struct
          http{false, "", ""}, // Initialize HTTP struct
          mqtt{false, "", "", "", 1883, 0}, // Initialize MQTT struct
          telegram{false, "", 0, 0}, // Initialize Telegram struct
          stune{0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0.0, 0}, // Initialize sTune struct
          adc{0,0,0,0,0,0,5}
    {}

};

#endif // BATTERY_STATE_H