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
#include <sTune.h>

#define V_REF 1121
#define MOVING_AVG_SIZE 5
#define EEPROM_OFFSET 100
#define EEPROM_SIZE 512
#define ADC_CHANNEL ADC1_CHANNEL_3
#define ADC_ATTEN ADC_ATTEN_DB_11

#define PWM_CHANNEL 4
#define PWM_FREQ 100
#define PWM_RESOLUTION 8


struct batterys {

    String      myname;
    uint8_t     size;
    uint8_t     sizeApprx;
    uint8_t     wantedTemp;
    uint32_t    mV_max;
    uint32_t    mV_min;
    float       temperature;
    uint32_t    milliVoltage;
    uint8_t     voltageInPrecent;
    uint8_t     ecoVoltPrecent;
    uint8_t     boostVoltPrecent;
    uint8_t     ecoTemp;
    uint8_t     boostTemp;
    uint8_t     resistance;
    uint8_t     capct;
    uint8_t     chrgr;
    uint8_t     vState;
    uint8_t     tState;
    uint8_t     previousVState;
    uint8_t     previousTState;
    uint8_t     maxPower;
    uint8_t     pidP;
    bool        chargerState;
    bool        voltBoostActive;
    bool        tempBoostActive;
    };

class Battery {
public:
    static batterys batry;
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

    
    bool readTemperature();
    void handleBatteryControl();    // Main control logic for battery
    void saveSettings();
    void loadSettings();

    esp_adc_cal_characteristics_t characteristics;

    float getTemperature();         // Returns the current battery temperature
    int getBatteryDODprecent();

    int getVoltageInPercentage(uint32_t milliVoltage);
    float btryToVoltage(int precent); 
    
    unsigned long currentMillis = 0;
    unsigned long lastTempStateTime = 0;
    unsigned long lastVoltageStateTime = 0;

    bool setup_done = false;
    unsigned long dallasTime = 0;

    bool setPidP(int pidP);
    int getPidP();

    bool getChargerStatus();
    void charger(bool state);

    uint8_t getNominalString();
    bool setNominalString(int size);

    bool setEcoPrecentVoltage(int value);
    uint8_t getEcoPrecentVoltage();

    bool setBoostPrecentVoltage(int boostPrecentVoltage);
    uint8_t getBoostPrecentVoltage();

    uint8_t getEcoTemp();
    bool setEcoTemp(int ecoTemp);

    uint8_t getBoostTemp();
    bool setBoostTemp(int boostTemp);

    bool activateTemperatureBoost(bool value);
    bool getActivateTemperatureBoost();
    
    bool activateVoltageBoost(bool value);
    bool getActivateVoltageBoost();

    bool setCharger(int chargeCurrent);
    int getCharger();

    bool setCapacity(int capacity);
    int getCapacity();

    bool setResistance(uint8_t resistance);
    int getResistance();

    float calculateChargeTime(int initialPercentage, int targetPercentage);
    void  readVoltage(unsigned long intervalSeconds);

    void adjustHeaterSettings();

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

    bool isValidHostname(const char* hostname);

    bool saveHostname(String hostname);
    bool loadHostname();

        // PID variables
    float pidInput, pidOutput, pidSetpoint; 
    float kp = 0;
    float ki = 0;
    float kd = 0;

    float ap = 100;
    float ai = 75;
    float ad = 0;


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
        TBOOST_RESET            =  5,
        DEFAULT_TEMP            =  6
    };
    
    /**/
    // static Battery* instance; 
    // int tempSensor;   // Pin for voltage reading
    int voltagePin  = 39;   // Pin for voltage reading
    int heaterPin   = 32;    // Pin for controlling the heater
    int chargerPin  = 25;   // Pin for controlling the charger
    // int greenLed;     // Pin for green LED
    // int yellowLed;    // Pin for yellow LED
    // int redLed;       // Pin for red LED
    // float currentTemperature;  // ??
    // bool temperatureFailsafe;   // ??

    // LED blinkers
    Blinker red;
    Blinker green;
    Blinker yellow;
    OneWire oneWire; // Create OneWire instance
    DallasTemperature dallas; // Create DallasTemperature instance

/*
    // PID variables
    float pidInput, pidOutput, pidSetpoint; 
    float kp = 50;
    float ki = 50;
    float kd = 0;

    float ap = 100;
    float ai = 75;
    float ad = 0;
*/

    // user settings
    
    uint32_t settleTimeSec = 20;
    uint32_t testTimeSec = 360;
    const uint16_t samples = 500;
    const float inputSpan = 40;
    const float outputSpan = 255;
    float outputStart = 0;
    float outputStep = 50;
    float tempLimit = 40;
        
    QuickPID heaterPID;
    sTune tuner;

    void updateHeaterPID();

    void controlHeaterPWM(uint8_t dutycycle);
    /// void controlCharger(bool state);

    VoltageState getVoltageState(   int voltagePrecent, 
                                    int ecoPrecentVoltage, 
                                    int boostPrecentVoltage);

    // getTempState
    TempState getTempState(         float temperature, 
                                    int ecoTemp, 
                                    int boostTemp);

    // these are the final countdown voltage!! 
    uint32_t currentMilliVoltage;

    // these are the final countdown temperature!!

};

#endif // BATTERY_H
