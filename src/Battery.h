#ifndef BATTERY_H
#define BATTERY_H


#include <OneWire.h>
#include <DallasTemperature.h>
#include <QuickPID.h>
#include <Blinker.h>
#include <Preferences.h>
#include <ESPUI.h>
#include <esp_adc_cal.h>
#include <ArduinoJson.h>

#define V_REF 1121
#define MOVING_AVG_SIZE 5
#define EEPROM_OFFSET 100
#define EEPROM_SIZE 512
#define ADC_CHANNEL ADC1_CHANNEL_3
#define ADC_ATTEN ADC_ATTEN_DB_11

#define PWM_CHANNEL 4
#define PWM_FREQ 1000
#define PWM_RESOLUTION 8

struct batterys {

    char         myname[13];
    uint32_t     size;
    uint32_t     sizeApprx;
    uint32_t     wantedTemp;
    uint32_t     mV_max;
    uint32_t     mV_min;
    uint32_t     temperature;
    uint32_t     milliVoltage;
    uint32_t     voltageInPrecent;
    uint32_t     ecoVoltPrecent;
    uint32_t     boostVoltPrecent;
    uint32_t     ecoTemp;
    uint32_t     boostTemp;
    uint32_t     resistance;
    uint32_t     capct;
    uint32_t     chrgr;
    uint8_t      vState;
    uint8_t      tState;
    uint8_t      previousVState;
    uint8_t      previousTState;
    bool        chargerState;
    bool        voltBoostActive;
    bool        tempBoostActive;
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

    static batterys batry;
    // static statesLog batteryLog;
    // LogEntry entry;
    // static LogEntry entry;

    Preferences preferences;

    Battery(int tempSensor = 33, 
            int voltagePin = 39, 
            int heaterPin = 32, 
            int chargerPin = 26,    
            int greenLed = 4, 
            int yellowLed = 18, 
            int redLed = 17);

    void begin();
    void loop();
    void setup();

    float readTemperature();

    void handleBatteryControl();    // Main control logic for battery

    void saveSettings();
    void loadSettings();

    esp_adc_cal_characteristics_t characteristics;

    
    float getTemperature();         // Returns the current battery temperature
    
    bool isTemperatureSafe();
    int getBatteryDODprecent();

    int getVoltageInPercentage(uint32_t milliVoltage);
    float btryToVoltage(int precent); 

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
    bool tempBoostActive = false;
    bool voltageBoostActive = false;
    bool setup_done = false;
    float accurateVoltage;
    unsigned long dallasTime = 0;

    float accuVolts();

    int getNominalString();
    bool setNominalString(int size);

    bool setEcoPrecentVoltage(int value);
    int getEcoPrecentVoltage();

    bool setBoostPrecentVoltage(int boostPrecentVoltage);
    int getBoostPrecentVoltage();

    int getEcoTemp();
    bool setEcoTemp(int ecoTemp);

    int getBoostTemp();
    bool setBoostTemp(int boostTemp);

    bool activateTemperatureBoost(bool value);
    bool getActivateTemperatureBoost();
    
    bool activateVoltageBoost(bool value);
    bool getActivateVoltageBoost();

    bool setCharger(int chargeCurrent);
    int getCharger();

    bool setCapacity(int capacity);
    int getCapacity();

    bool setResistance(int resistance);
    int getResistance();

    float calculateChargeTime(int initialPercentage, int targetPercentage);
    void  readVoltage(unsigned long intervalSeconds);

    uint32_t determineBatterySeries(uint32_t measuredVoltage_mV);
    float getCurrentVoltage();
    int getBatteryApprxSize();

        // Global variables
    u_int32_t MOVAreadings[MOVING_AVG_SIZE] = {0};
    u_int32_t varianceReadings[MOVING_AVG_SIZE] = {0};
    int MOVAIndex = 0;
    u_int32_t MOVASum = 0;
    bool firstRun = true;
    unsigned long voltageMillis = 0;
    unsigned long heaterTimer = 0;

    char logBuffer[50] = {0};
    int logIndex = 0;
    uint32_t lastLogTime = 0;
    int lastVoltageState = -1;
    int lastTempState = -1;

    void addLogEntry(int voltState, int tempState);

    void charger(bool state);

    void saveHostname(const char* hostname);
    void loadHostname();
    void updateHostname(const char* newHostname);

/*
 *  Private methods
 *
 * These are the private methods that are used in the battery.cpp file
 */ 

private: 


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
        TBOOST_RESET            =  5
    };



    Settings settings;
    
    // static Battery* instance; 
    int tempSensor;   // Pin for voltage reading
    int voltagePin;   // Pin for voltage reading
    int heaterPin;    // Pin for controlling the heater
    int chargerPin = 26;   // Pin for controlling the charger
    int greenLed;     // Pin for green LED
    int yellowLed;    // Pin for yellow LED
    int redLed;       // Pin for red LED
    float currentTemperature;  // ??
    bool temperatureFailsafe;   // ??

    // LED blinkers
    Blinker red;
    Blinker green;
    Blinker yellow;
    OneWire oneWire; // Create OneWire instance
    DallasTemperature dallas; // Create DallasTemperature instance

    // PID variables
    float pidInput, pidOutput, pidSetpoint, kp, ki, kd;
    float currentTemp, wantedTemp;

    QuickPID heaterPID;
    void updateHeaterPID();

    void controlHeaterPWM(uint8_t dutycycle);
    /// void controlCharger(bool state);

    VoltageState getVoltageState(   int voltagePrecent, 
                                    int ecoPrecentVoltage, 
                                    int boostPrecentVoltage);

    // getTempState
    TempState getTempState(         float temperature, 
                                    int boostTemp, 
                                    int ecoTemp);

    // these are the final countdown voltage!! 
    uint32_t currentMilliVoltage;

    // these are the final countdown temperature!!

};

#endif // BATTERY_H
