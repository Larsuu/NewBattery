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


#define STATUSCB  "statuscb"  // callback data sent when "LIGHT ON" button is pressed
#define BOOSTCB "boostcb" // callback data sent when "LIGHT OFF" button is pressed


#define VERSION_2

// old hardware version (just few old prototype boards  -- not in public use)
#ifdef VERSION_1
#define TEMP_SENSOR 33
#define VOLTAGE_PIN 39
#define HEATER_PIN 25
#define CHARGER_PIN 32
#define GREEN_LED 4
#define YELLOW_LED 18
#define RED_LED 17
#else
#define TEMP_SENSOR 33
#define VOLTAGE_PIN 39
#define HEATER_PIN 32
#define CHARGER_PIN 25
#define GREEN_LED 4
#define YELLOW_LED 18
#define RED_LED 17
#endif

#define USE_CLIENTSSL false  

#define V_REF 1121
#define MOVING_AVG_SIZE 5
#define EEPROM_OFFSET 100
#define EEPROM_SIZE 512
#define ADC_CHANNEL ADC1_CHANNEL_3
#define ADC_ATTEN ADC_ATTEN_DB_11

#define PWM_CHANNEL 4
#define PWM_FREQ 100
#define PWM_RESOLUTION 8


class Battery {
 private: 

    // hardware pins
    int tempSensor = TEMP_SENSOR;   // Pin for voltage reading
    int voltagePin = VOLTAGE_PIN;   // Pin for voltage reading
    int heaterPin = HEATER_PIN;    // Pin for controlling the heater
    int chargerPin = CHARGER_PIN;   // Pin for controlling the charger
    int greenLed = GREEN_LED;      // Pin for the green LED
    int yellowLed = YELLOW_LED;    // Pin for the yellow LED
    int redLed = RED_LED;          // Pin for the red LED

    QuickPID heaterPID;
    sTune tuner;
    esp_adc_cal_characteristics_t characteristics;

    // LED blinkers
    Blinker red;
    Blinker green;
    Blinker yellow;
    OneWire oneWire;            // Create OneWire instance
    DallasTemperature dallas;   // Create DallasTemperature instance
    Preferences preferences;

    // LAN remote control and monitoring
    WiFiClient espClient;
    PubSubClient mqtt;
    String mqttTopic;

    // WAN remote control and monitoring
    WiFiClientSecure client;  // Needed for secure connection
    AsyncTelegram2 telegram;


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

    /// void controlCharger(bool state);
    VoltageState getVoltageState(   int voltagePrecent, 
                                    int ecoPrecentVoltage, 
                                    int boostPrecentVoltage);

    // getTempState
    TempState getTempState(         float temperature, 
                                    int ecoTemp, 
                                    int boostTemp);

public:
   struct batteryState {
    // basic info
    String      myname              = "Helmi";
    float       temperature         = 0.0f;
    uint32_t    milliVoltage        = 0;
    uint8_t     voltageInPrecent    = 0;
    uint8_t     size                = 7;
    uint8_t     sizeApprx           = 7;                //autodetect

    uint8_t     wantedTemp          = 15;

    uint32_t    mV_max              = 100000;
    uint32_t    mV_min              = 10000;

    bool        voltBoost           = false;
    bool        tempBoost           = false;

    // state related
    uint8_t     previousVState      = 0;                // no need?
    uint8_t     previousTState      = 0;                // no need?
    uint8_t     vState              = 0;
    uint8_t     tState              = 0;

    uint8_t     capct               = 0;
    bool        chargerState        = false;

    uint8_t     ecoVoltPrecent      = 50;
    uint8_t     boostVoltPrecent    = 80;

    // Charger
    struct chrgr{
        bool        enable              = false;
        uint8_t     current             = 0;
    } charger;


    // Heater element
    struct heating{
        bool        enable              = false;
        uint8_t     maxPower            = 30;
        uint8_t     resistance          = 50;
        uint8_t     ecoTemp             = 15;
        uint8_t     boostTemp           = 25;
        float       pidP                = 1.00;
        float       pidI                = 0.02;
        float       pidD                = 0.02;
    } heater;

    // HTTP access
    struct httpacc {
        bool enable     = false;
        String username = "";
        String password = "";
    } http;

    struct wifi {
        String ssid = "";
        String pass = "";
    } wlan;

    // MQTT
    struct mqttconn {
        String      server          = "";
        uint16_t    port            = 1883;
        String      username        = "";
        String      password        = "";
        uint32_t    lastMessageTime = 0;
        bool        enable          = false;
    } mqtt;

    // Telegram
    struct tg {
        String token = "7875426228:AAE5HQJSmiphhDAD-CynCpamfHmk65hkF1A";
        int64_t chatId = 922951523;
        bool enable = false;
        unsigned long lastMessageTime = 0;
        bool alertsEnabled = true;
        uint32_t deltaVoltage = 0;
    } telegram;
};

    batteryState battery;

    Battery(int tempSensor, int voltagePin, int heaterPin, int chargerPin, int greenLed, 
            int yellowLed, int redLed)
        :   oneWire(tempSensor)
        ,   mqtt(espClient)
        ,   telegram(client)
        ,   dallas(&oneWire)
        ,   voltagePin(voltagePin)
        ,   heaterPin(heaterPin)
        ,   chargerPin(chargerPin)
        ,   red(redLed)
        ,   yellow(yellowLed)
        ,   green(greenLed)
        ,   heaterPID(&pidInput, &pidOutput, &pidSetpoint, kp, ki, kd, QuickPID::Action::direct)
        ,   tuner(&pidInput, &pidOutput, sTune::TuningMethod::ZN_PID, sTune::Action::directIP, sTune::SerialMode::printOFF)
    {
        mqttTopic = "battery/" + battery.myname + "/";
        battery.telegram.enable = true;
        
    }

    ~Battery();
    Battery(const Battery&) = delete;
    Battery& operator=(const Battery&) = delete;
 
    // Global variables
    const char* MYTZ = "EET-2EEST,M3.5.0/3,M10.5.0/4"; 


    enum SettingsType {
    SETUP,
    WIFI,
    HTTP,
    MQTT,
    TELEGRAM,
    PID,
    ALL
    };

    // Battery Voltage Reading variables.
    u_int32_t MOVAreadings[MOVING_AVG_SIZE] = {0};
    u_int32_t varianceReadings[MOVING_AVG_SIZE] = {0};
    int MOVAIndex = 0;
    u_int32_t MOVASum = 0;
    unsigned long currentMillis = 0;
    unsigned long voltageMillis = 0;

    bool firstRun = true;

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

    ReplyKeyboard   myMainMenuKbd; // inline keyboard object helper
    InlineKeyboard  StatusKbd;   // reply Voltage, Temp, Booststatus, Booststatus
    ReplyKeyboard   BoostKbd;    // reply Boost, Boost, Boost 
    ReplyKeyboard   ManualKbd;   // reply External URL -> github
    ReplyKeyboard   SettingsKbd; // reply eco & boost limits
    ReplyKeyboard   LogsKbd;     // reply log
    ReplyKeyboard   EndKbd;      // reply end

    bool isKeyboardActive;      // store if the reply keyboard is shown

    const char* TEMP = "C";
    const char* VOLT = "V";

    const char* Status = "status";
    const char* Boost = "boost";
    
    // PID variables
    float pidInput, pidOutput, pidSetpoint; 
    float kp = 0;
    float ki = 0;
    float kd = 0;

    float ap = 100;
    float ai = 75;
    float ad = 0;

    // sTune user settings
    uint32_t settleTimeSec = 10;
    uint32_t testTimeSec = 200;
    const uint16_t samples = 600;
    const float inputSpan = 40;
    const float outputSpan = 255;
    float outputStart = 0;
    float outputStep = 200;
    float tempLimit = 40;
    
    void loop();
    void setup();
 
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
    void  readVoltage(uint8_t intervalSeconds);

    void adjustHeaterSettings();

    uint32_t determineBatterySeries(uint32_t measuredVoltage_mV);
    float getCurrentVoltage();
    int getBatteryApprxSize();

    void addLogEntry(int voltState, int tempState);
    bool isValidHostname(const char* hostname);
    bool saveHostname(String hostname);
    bool loadHostname();

    void updateHeaterPID();
    void controlHeaterPWM(uint8_t dutycycle);

    void setupTelegram(const char* token, const char* chatId);

    void publishBatteryData();

    void mqttSetup();
    void handleMqtt();
    bool getMqttState();
    bool setMqttState(bool status);

    bool getTelegramEn();
    bool setTelegramEn(bool status);

    bool getHttpEn();
    bool setHttpEn(bool status);
};

#endif // BATTERY_H
