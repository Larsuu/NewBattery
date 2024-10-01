#ifndef BATTERY_H
#define BATTERY_H


#include <OneWire.h>
#include <DallasTemperature.h>
#include "NonBlockingDallas.h"
#include <QuickPID.h>
#include <Blinker.h>
#include <Preferences.h>

#define V_REF 1121
#define MOVING_AVG_SIZE 5
#define EEPROM_OFFSET 100
#define EEPROM_SIZE 512
#define ADC_CHANNEL ADC1_CHANNEL_3
#define ADC_ATTEN ADC_ATTEN_DB_11

struct batterys {

    uint16_t    size;
    uint16_t    temperature;
    uint16_t    voltagePrecent;
    uint16_t    ecoVoltPrecent;
    uint16_t    boostVoltPrecent;
    uint16_t    ecoTempPrecent;
    uint16_t    boostTempPrecent;
    uint16_t    resistance;
};

struct Settings {
        bool    httpAccess;
        bool    initialSetup;
        int     clamp;
        int     kp;
        bool    mqtt;
        bool    accessPointMode;
        char    username[13];
        char    password[13];
};


class Battery {
public:

    Battery(int tempSensor = 33, 
            int voltagePin = 39, 
            int heaterPin = 32, 
            int chargerPin = 25,    
            int greenLed = 4, 
            int yellowLed = 18, 
            int redLed = 17);

    void loop();
    void setup();

    void handleBatteryControl();    // Main control logic for battery

    void saveSettings();
    void loadSettings();
    
    void readVoltage();

    float getTemperature();         // Returns the current battery temperature
    void readTemperature();
    bool isTemperatureSafe();
    // void onIntervalElapsed();

    int getVoltageInPercentage(uint32_t milliVoltage);
    int btryToMilliVoltage(int precent); 

    static void handleIntervalElapsed(int deviceIndex, int32_t temperatureRAW); // Static callback

    int currentPrecentVoltage;
    int voltagePrecent, ecoPrecentVoltage, boostPrecentVoltage;
    bool boostVoltageModeOn;
    bool boostTemperatureModeOn;
    bool boostActive;
    int boostTemp, ecoTemp;
    float temperature;
    bool boostVoltageMode;
    unsigned long currentMillis = 0;
    unsigned long lastTempStateTime = 0;
    unsigned long lastVoltageStateTime = 0;
    bool overrideTempTimer = false;
    bool overrideVoltageTimer = false;

    Battery* getInstance();

    int getNominalString();
    bool setNominalString(int size);

    bool setEcoPrecentVoltage(int value);
    int getEcoPrecentVoltage();

    void setBoostPrecentVoltage(int boostPrecentVoltage);
    int getBoostPrecentVoltage();

    int getEcoTemp();
    bool setEcoTemp(int ecoTemp);

    int getBoostTemp();
    bool setBoostTemp(int boostTemp);

/*
 *  Private methods
 *
 * These are the private methods that are used in the battery.cpp file
 */ 

private:
    batterys batry; 
    static Battery* instance; 

    int tempSensor;   // Pin for voltage reading
    int voltagePin;   // Pin for voltage reading
    int heaterPin;    // Pin for controlling the heater
    int chargerPin;   // Pin for controlling the charger
    int greenLed;     // Pin for green LED
    int yellowLed;    // Pin for yellow LED
    int redLed;       // Pin for red LED

    OneWire oneWire; // Create OneWire instance
    DallasTemperature dallas; // Create DallasTemperature instance
    NonBlockingDallas dallaz; // Create NonBlockingDallas instance
    Preferences preferences; // Create Preferences instance

    enum VoltageState {
        LOW_VOLTAGE_WARNING,
        PROTECTED,
        ECONOMY,
        BOOST,
        VBOOST_RESET
    };

    enum TempState {
        SUBZERO,
        ECO_TEMP,
        BOOST_TEMP,
        OVER_TEMP,
        TEMP_WARNING,
        TBOOST_RESET
    };


    // LED blinkers
    Blinker red;
    Blinker green;
    Blinker yellow;

    // PID variables
    float pidInput, pidOutput, pidSetpoint, kp, ki, kd;

    QuickPID heaterPID;

    // my own User Interface struct
    Settings settings;


    VoltageState getVoltageState(   int voltagePrecent, 
                                    int ecoPrecentVoltage, 
                                    int boostPrecentVoltage);

    // getTempState
    TempState getTempState(         float temperature, 
                                    int boostTemp, 
                                    int ecoTemp,
                                    bool boostActive);

    // these are the final countdown voltage!! 
    uint32_t currentMilliVoltage;

    // these are the final countdown temperature!!
    float currentTemperature;
    bool temperatureFailsafe;

    // ADC for voltage reading
    u_int32_t MOVAreadings[MOVING_AVG_SIZE];
    int MOVAIndex = 0;
    u_int32_t MOVASum = 0;
    u_int32_t voltage = 0;

};

#endif // BATTERY_H
