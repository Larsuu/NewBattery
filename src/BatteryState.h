// BatteryState.h
#ifndef BATTERY_STATE_H
#define BATTERY_STATE_H

#include <Arduino.h> // Include necessary libraries
#include <cstring>

// Forward declarations for state types
enum VoltageState { 
    ALERT,                  // =   0,              // red
    WARNING,                 // =   1,              // yellow+red
    LOVV,                     // =   2,              // yellow_solid
    ECO,                    // =   3,              // Green_solid    
    BOOST,                   // =   4,              // Green_blink
    FULL                    // =   5   
};

enum TempState {
    SUBZERO        ,
    COLD           ,
    ECO_TEMP       ,
    ECO_READY      ,
    BOOST_TEMP     ,
    BOOST_READY    ,
    TEMP_WARNING   ,
    UNKNOWN_TEMP     
};


struct batteryState {
private:
    int error;
public:
    // Member variables
    String          name;                // Name of the battery
    uint8_t         size;                // Size of the battery
    bool            init;                // Init of the battery
    uint8_t         initError;           // Init error of the battery
    uint8_t         initWarning;         // Error of the battery
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
    uint8_t         sizeApprx;           // Approximate size

    // Nested struct for charger
    struct charger {
        uint8_t     current;        // Charger current
        bool        enable;         // Enable/disable charger
        uint32_t    time;           // Last message time
    } chrgr;

    // Nested struct for timer
    struct timer {
        uint32_t    voltMs;         // Voltage measurement time
        uint32_t    voltFet;        // Voltage FET time
        uint32_t    tempMillis;     // Temperature measurement time
        uint32_t    heaterMillis;   // Heater control time
    } timer;

    // Nested struct for heater
    struct resistor {
        uint32_t    time;           // Last message time
        uint8_t     runTimes;       // lets avarage the runs of the PID
        uint8_t     resistance;     // Heater resistance
        uint8_t     ecoTemp;        // Eco temperature
        uint8_t     boostTemp;      // Boost temperature
        uint8_t     maxPower;       // Maximum power
        uint8_t     powerLimit;     // Power limit

        float       pidP;           // PID proportional
        float       pidI;           // PID integral
        float       pidD;           // PID derivative
        float       pidInput;       // PID input
        float       pidSetpoint;    // PID setpoint
        float       pidOutput;      // PID output

        bool        pidRun;         // PID stune run
        bool        pidDone;        // PID stune done
        bool        pidEnable;      // PID sTune enable
        bool        enable;         // Enable/disable heater
        bool        init;           // init for heater
    } heater;

    // Nested struct for WiFi
    struct wifi {
        bool        enable;         // Enable/disable WiFi
        String      ssid;           // WiFi SSID
        String      pass;           // WiFi password
        uint32_t    time;           // Last message time
    } wlan;

    // Nested struct for HTTP
    struct lan {
        bool        enable;         // Enable/disable HTTP
        String      username;       // HTTP username
        String      password;       // HTTP password
    } http;

    // Nested struct for MQTT
    struct mosquitto {
        bool        enable;         // Enable/disable MQTT
        bool        setup;          // MQTT connected
        String      username;       // MQTT username
        String      password;       // MQTT password
        String      server;         // MQTT server
        uint16_t    port;           // MQTT port
        uint32_t    lastMessageTime; // Last message time
    } mqtt;

    // Nested struct for Telegram
    struct tg {
        bool        enable;         // Enable/disable Telegram
        bool        setup;          // Telegram connected
        String      token;          // Telegram token
        uint32_t    chatId;         // Telegram chat ID
        uint32_t    lastMessageTime; // Last message time
        String      chatIdStr;      // Telegram chat ID as string
        
    } telegram;

    // Nested struct for sTune
    struct stune {
        uint32_t    timeNow;         // Last message time
        uint32_t    lenTime;        // Measurement time
        uint32_t    startTime;      // Measurement time in seconds
        uint32_t    endTime;        // Measurement time in seconds

        float       inputSpan;      // Input span
        float       outputSpan;     // Output span
        float       outputStart;    // Output start
        float       outputStep;     // Output step
        float       testTimeSec;    // Test time
        float       settleTimeSec;   // Settle time
        float       samples;        // Samples
        float       tempLimit;      // Temperature limit
    
        bool        enable;         // Enable/disable sTune
        bool        firstRun;       // First run PID
        bool        run;            // Run/stop sTune
        bool        done;           // Done/not done sTune
        bool        error;          // Error/no error sTune
        
        float       pidInput;       // PID input
        float       pidOutput;      // PID output
        float       pidSetpoint;    // PID setpoint
    
        float       pidP;          // PID P
        float       pidI;          // PID I
        float       pidD;           // PID D
        uint8_t     runTimes;      // lets avarage the runs of the PID
    } stune;

    struct adc {
        uint32_t    raw;        // Raw ADC value
        uint32_t    cal;        // Calibrated ADC value
        uint32_t    time;       // Last message time
        uint32_t    index;      // Index
        uint32_t    sum;        // Sum
        uint32_t    avg;        // Average
        uint8_t     mAvg;                           // integer div by zero!
        uint32_t    readings[5];
    } adc;

  // Public constructor to initialize batteryState with default values
    batteryState()
        : error(0),
          size(0), 
          init(false),
          initError(0),
          initWarning(0),
          temperature(25.0),
          name("Onni"),
          ecoVoltPrecent(50), 
          boostVoltPrecent(80),
          voltBoost(false),
          tempBoost(false),
          prevTState(SUBZERO),
          prevVState(ALERT),
          firstRun(true),
          tState(COLD),
          vState(WARNING),   
          capct(10),     
          lastMessageTime(0),
          sizeApprx(0),
          chrgr{1, false, 0}, // Initialize charger struct
          timer{0, 0, 0, 0}, // Initialize timer struct
          heater{
              0,    // time
              0,    // runTimes 
              0,    // resistance
              30,   // ecoTemp - setting reasonable default
              40,   // boostTemp - setting reasonable default
              30,   // maxPower - setting to 100% as default
              0,    // powerLimit
              1.0,  // pidP
              0.01, // pidI
              0.2,  // pidD
              25.0, // pidInput - starting at room temperature
              40.0, // pidSetpoint - default target temperature
              0.0,  // pidOutput
              false, // pidRun
              false, // pidDone
              false, // pidEnable
              false,  // enable
              false  // init
          },
          wlan{false, "", "", 0}, // Initialize WiFi struct
          http{false, "", ""}, // Initialize HTTP struct
          mqtt{false, false, "", "", "", 1883, 0}, // Initialize MQTT struct
          telegram{false,false, "", 922951523, 0, "922951523"}, // Initialize Telegram struct 
          stune{
                0,          // timeNow
                250,        // lenTime
                0,          // startTime
                1000,       // endTime 

                40.0,       // inputSpan: Input span for tuning
                255.0,      // outputSpan: Output span for tuning
                0.0,        // outputStart: Initial output value
                50.0,       // outputStep: Step size for output adjustments
                60,         // testTimeSec: Duration of the test in seconds
                10,         // settleTimeSec: Settle time for the tuning process
                60,         // samples: Number of samples to collect
                40.0,       // tempLimit: Temperature limit for tuning

                false,      // enable: Flag to enable/disable sTune
                false,      // firstRunPID: Flag for the first run of PID tuning
                false,      // run: Flag indicating if tuning is currently running
                false,      // done: Flag indicating if tuning is complete
                false,      // error: Flag indicating if there was an error during tuning
                
                0.0,        // pidInput: Current PID input value
                0.0,        // pidOutput: Current PID output value
                0.0,        // pidSetpoint: Desired setpoint for PID control
                0.0,        // pidP: Proportional gain for PID
                0.0,        // pidI: Integral gain for PID
                0.0,        // pidD: Derivative gain for PID
                1           // runtime: lets avarage the runs of the PID
              
          },
          adc{0, 0, 0, 0, 0, 0, 5, {0, 0, 0, 0, 0}} // Initialize adc struct with correct types
    {}

};

#endif // BATTERY_STATE_H