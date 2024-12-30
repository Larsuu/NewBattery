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
#include <WiFi.h>
#include <PubSubClient.h>
#include <AsyncTelegram2.h>
#include <WiFiClientSecure.h>
#include <time.h>
// #include "TelegramBot.h"
#include <cctype>
#include <WiFiClient.h>
#include "BatteryState.h"
#include <esp_adc_cal.h>

#define VERSION_2

// old hardware version (just few old prototype boards  -- not in public use)
#ifdef VERSION_1
#define TEMP_SENSOR 21
#define VOLTAGE_PIN 39
#define HEATER_PIN 33
#define CHARGER_PIN 32
#define GREEN_LED 4
#define YELLOW_LED 18
#define RED_LED 17
#define PWM_CHANNEL 0
#define PWM_FREQ 100
#define PWM_RESOLUTION 8
#else
#define TEMP_SENSOR 33
#define VOLTAGE_PIN 39
#define HEATER_PIN 32
#define CHARGER_PIN 25
#define GREEN_LED 4
#define YELLOW_LED 18
#define RED_LED 17
#define PWM_CHANNEL 0
#define PWM_FREQ 100
#define PWM_RESOLUTION 8
#endif

#define USE_CLIENTSSL false  

#define V_REF 1121
#define MOVING_AVG_SIZE 5
#define EEPROM_OFFSET 100
#define EEPROM_SIZE 512
#define ADC_CHANNEL ADC1_CHANNEL_3
#define ADC_ATTEN ADC_ATTEN_DB_11

/*
// Add these enum definitions before the batteryState struct
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
*/

/*
struct batteryState {
    String          name;
    uint8_t         size;                       
    float           temperature;
    uint8_t         wantedTemp;
    uint32_t        milliVoltage;
    uint8_t         voltageInPrecent;
    uint8_t         ecoVoltPrecent;             // Eco voltage percentage
    uint8_t         boostVoltPrecent;           // Boost voltage percentage
    uint8_t         capct;                      // Capacity
    bool            voltBoost;
    bool            tempBoost;
    TempState       tState;      
    VoltageState    vState;
    TempState       prevTState;
    VoltageState    prevVState;
    bool            firstRun;
    uint32_t        lastMessageTime; // Last message time
    int             sizeApprx;

    struct charger {
        uint8_t     current; // Charger current
        bool        enable; // Enable/disable
        uint32_t    time; // Last message time
    } chrgr;

    struct timer {
        uint32_t    voltMs;
        uint32_t    voltFet;
        uint32_t    tempMillis;
        uint32_t    heaterMillis;
    } timer;

    struct resistor {
        uint8_t     resistance; // Heater resistance
        uint8_t     ecoTemp; // Eco temperature
        uint8_t     boostTemp; // Boost temperature
        uint8_t     maxPower; // Maximum power
        float       pidP; // PID proportional
        float       pidI; // PID integral
        float       pidD; // PID derivative
        uint32_t    time;
    } heater;

    struct wifi {
        bool        enable; // Enable/disable
        String      ssid; // WiFi SSID
        String      pass; // WiFi password
        uint32_t    time;
    } wlan;

    struct lan {
        bool        enable; // Enable/disable
        String      username; // HTTP username
        String      password; // HTTP password
    } http;

    struct mosquitto {
        bool        enable; // Enable/disable
        String      username; // MQTT username
        String      password; // MQTT password
        String      server; // MQTT server
        uint16_t    port; // Uncomment if needed
        uint32_t    lastMessageTime; // Last message time
    } mqtt;

    struct tg {
        bool        enable; // Enable/disable
        String      token; // Telegram token
        int         chatId; // Telegram chat ID
        uint32_t    lastMessageTime; // Last message time
    } telegram;

    struct stune {
        float       inputSpan; // Input span
        float       outputSpan; // Output span
        float       outputStart; // Output start
        float       outputStep; // Output step
        uint32_t    testTimeSec; // Test time
        uint32_t    settleTimeSec; // Settle time
        uint32_t    samples; // Samples
        float       tempLimit; // Temperature limit
        uint32_t    time;
    } stune;

    // Constructor to initialize batteryState with default values
    batteryState(uint8_t initialSize = 1, float initialTemperature = 25.0, String initialName = "Onni")
        :   size(initialSize), 
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
            heater{20, 25, 30, 30, 2.0, 0.01, 0.1},     
            wlan{true, "Olohuone", "10209997"}, // Example WiFi credentials
            http{true, "admin", "password"}, // Example HTTP credentials
            mqtt{true, "mqttUser", "mqttPass", "mqtt.server.com", 1883, 0}, // Example MQTT credentials
            telegram{true, "myTelegramToken", 123456789, 0}, // Example Telegram credentials
            stune{1.0, 2.0, 0.0, 0.1, 10, 5, 100, 50.0, 0} // Example sTune values
    {};
};
*/

//class TelegramBot;  // forward declaration lazy load.

// Add this enum definition before the Battery class
enum SettingsType {
    SETUP,
    WIFI,
    HTTP,
    MQTT,
    TELEGRAM,
    PID,
    ALL
};

class Battery {
public:   
    // Static method to get the instance of the class
    static Battery& getInstance() {
        static Battery instance; // Guaranteed to be destroyed
        return instance; // Return the instance
    }
    void initBatteryState() {
        battery = batteryState();
    }

    batteryState getBatteryState() {
        return battery;
    }

    // Add batteryState as a public member
    batteryState battery;

    /*
        esp_adc_cal_characteristics_t characteristics;
        adc1_config_width(ADC_WIDTH_12Bit);
        adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH_BIT_12, V_REF, &characteristics);
    */
    // Global variables for telegram
    const char* MYTZ = "EET-2EEST,M3.5.0/3,M10.5.0/4"; 

    // Battery Voltage Reading variables.
    u_int32_t MOVAreadings[MOVING_AVG_SIZE] = {};
    u_int32_t varianceReadings[MOVING_AVG_SIZE] = {};
    int MOVAIndex = 0;
    u_int32_t MOVASum = 1;
    // uint32_t voltVal = 0;
    // uint32_t calVoltVal = 0;

    

    unsigned long currentMillis = 0;
    unsigned long voltageMillis = 0;
    unsigned long heaterMillis = 0;

    unsigned long heaterTimer = 0;
    char logBuffer[50] = {0};
    int logIndex = 0;
    uint32_t lastLogTime = 0;
    int lastVoltageState = -1;
    int lastTempState = -1;

    // unsigned long currentMillis = 0;
    unsigned long lastTempStateTime = 0;
    unsigned long lastVoltageStateTime = 0;

    bool setup_done = false;
    unsigned long dallasTime = 0;

    // telegram
    uint32_t tgLoop = 0;

    // PID variables
    float pidInput, pidOutput, pidSetpoint;
    float kp = 1.0;
    float ki = 0.02;
    float kd = 0.02;

    float ap = 100;
    float ai = 75;
    float ad = 0;

/*
    // sTune user settings
    uint32_t settleTimeSec = 10;
    uint32_t testTimeSec = 200;
    const uint16_t samples = 600;
    const float inputSpan = 40;
    const float outputSpan = 255;
    float outputStart = 0;
    float outputStep = 200;
    float tempLimit = 40;
*/

    // sTune tuner = sTune(&pidInput, &pidOutput, tuner.ZN_PID, tuner.directIP, tuner.printALL);
    
    void loop();
    void setup();

     VoltageState getVoltageState(int voltagePrecent);
     TempState getTempState(float temperature);
 
     void initBot(WiFiClient& client);
    //bool telegramEnabled = true; // Flag to enable Telegram bot

    void readTemperature();
    void handleBatteryControl();    // Main control logic for battery
    void saveSettings(SettingsType type);
    void loadSettings(SettingsType type);

    float getTemperature();         // Returns the current battery temperature
    int getBatteryDODprecent();

    int getVoltageInPercentage(uint32_t milliVoltage);
    float btryToVoltage(int precent); 
    
    bool setPidP(float pidP);
    int getPidP();

    bool getChargerStatus();
    void charger(bool state);

    uint8_t getNominalString();
    bool setNominalString(uint8_t size);

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
    void  readVoltage(uint32_t intervalSeconds);

    void adjustHeaterSettings();

    uint32_t determineBatterySeries(uint32_t measuredVoltage_mV);
    float getCurrentVoltage();
    int getBatteryApprxSize();

    bool setHostname(String hostname);
    bool getHostname();

    void updateHeaterPID();
    void controlHeaterPWM(uint8_t dutycycle);

    void publishBatteryData();

    void mqttSetup();
    void handleMqtt();
    bool getMqttState();
    bool setMqttState(bool status);

    bool getTelegramEn();
    bool setTelegramEn(bool status);

    bool getHttpEn();
    bool setHttpEn(bool status);

    bool isValidHostname(const String& hostname);

    Preferences preferences;

        // LED blinkers


 private:

    const int tempSensor = TEMP_SENSOR;
    const int voltagePin = VOLTAGE_PIN;
    const int heaterPin = HEATER_PIN;
    const int chargerPin = CHARGER_PIN;
    const int greenLed = GREEN_LED;
    const int yellowLed = YELLOW_LED;
    const int redLed = RED_LED;

    Battery() 
        : battery(),
          oneWire(tempSensor),
          dallas(&oneWire),
          mqtt(espClient),
          heaterPID(&pidInput, &pidOutput, &pidSetpoint, kp, ki, kd, QuickPID::Action::direct),
          tuner(&pidInput, &pidOutput, tuner.ZN_PID, tuner.directIP, tuner.printALL),
          red(redLed),
          green(greenLed),
          yellow(yellowLed)
    {
        adc1_config_width(ADC_WIDTH_12Bit);
        adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH_BIT_12, V_REF, &characteristics);

        green.start();
        green.setDelay(1000);

        yellow.start();
        yellow.setDelay(1000);

        red.start();
        red.setDelay(1000, 0);
    }
    
    Blinker red;
    Blinker green;
    Blinker yellow;


    ~Battery();
    Battery(const Battery&) = delete;
    Battery& operator=(const Battery&) = delete;

    //TelegramBot* botManager = nullptr;
    QuickPID heaterPID;
    sTune tuner;

    OneWire oneWire;            // Create OneWire instance
    DallasTemperature dallas;   // Create DallasTemperature instance


    // LAN remote control and monitoring
    WiFiClient espClient;
    PubSubClient mqtt;
    String mqttTopic;

    //TelegramBot* telegramBot = nullptr;

    esp_adc_cal_characteristics_t characteristics;
};

#endif // BATTERY_H
