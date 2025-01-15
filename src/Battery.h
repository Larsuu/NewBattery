#ifndef BATTERY_H
#define BATTERY_H


#include <PubSubClient.h>
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
#include <time.h>
#include <cctype>
#include <cmath>
#include <WiFiClient.h>
#include "BatteryState.h"
#include <esp_adc_cal.h>
// #include <WiFiClientSecure.h>
// #include <HTTPClient.h>



//              PROPERTIES!!
/* ______________________________________ */
#define MQTT_ENABLED
#define VERSION_1
// #define TELEGRAM_ENABLED



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
#define PWM_FREQ 254
#define PWM_RESOLUTION 8
#endif

#define USE_CLIENTSSL false  

#define V_REF 1121
#define MOVING_AVG_SIZE 5
#define EEPROM_OFFSET 100
#define EEPROM_SIZE 512
#define ADC_CHANNEL ADC1_CHANNEL_3
#define ADC_ATTEN ADC_ATTEN_DB_11

#define MYTZ "EET-2EEST-3,M3.5.0/03:00:00,M10.5.0/04:00:00"

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

enum TuneState {
    STARTUP,
    TUNING,
    INITIALIZING,
    IDLE,
    ERROR,
    RETUNE
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

    unsigned long currentMillis = 0;
    unsigned long voltageMillis = 0;
    unsigned long heaterMillis = 0;
    unsigned long batteryInitTime = 0;

    unsigned long heaterTimer = 0;
    char logBuffer[50] = {0};
    int logIndex = 0;
    uint32_t lastLogTime = 0;
    int lastVoltageState = 0;
    int lastTempState = 0;

    // unsigned long currentMillis = 0;
    unsigned long lastTempStateTime = 0;
    unsigned long lastVoltageStateTime = 0;

    bool setup_done = false;
    unsigned long dallasTime = 0;


    void loop();
    void setup();
    bool init();
    bool batteryInit();
    void resetSettings(bool reset);


     VoltageState getVoltageState(int voltagePrecent);
     TempState getTempState(float temperature);

    void readTemperature();
    void handleBatteryControl();    // Main control logic for battery
    void saveSettings(SettingsType type);
    void loadSettings(SettingsType type);

    float getTemperature();         // Returns the current battery temperature
    int getBatteryDODprecent();

    int getVoltageInPercentage(uint32_t milliVoltage);
    float btryToVoltage(int precent); 
    
    void runTune();

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

    bool setResistance(int resistance);
    int getResistance();

    float calculateChargeTime(int initialPercentage, int targetPercentage);
    void  readVoltage(uint32_t intervalSeconds);

    void adjustHeaterSettings();

    uint32_t determineBatterySeries(uint32_t measuredVoltage_mV);
    float getCurrentVoltage();
    int getBatteryApprxSize();

    //void setHostname(const String& hostname);

    void getHostname();

    // void setHostname(const char* hostname);
    // const char* getHostname() const;

    void updateHeaterPID();
    void controlHeaterPWM();

    void manageTuning();

    void publishBatteryData();

    void mqttSetup();
    void handleMqtt();
    bool getMqttState();
    bool setMqttState(bool status);

    bool getTelegramEn();
    bool setTelegramEn(bool status);

    bool getHttpEn();
    bool setHttpEn(bool status);

    // bool isValidHostname(const String& hostname);

    // bool telegramSendMessage(const char *message);

    Preferences preferences;

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
          heaterPID(&battery.heater.pidInput, &battery.heater.pidOutput, &battery.heater.pidSetpoint, battery.heater.pidP, battery.heater.pidI, battery.heater.pidD, QuickPID::Action::direct),
          tuner(&battery.stune.pidInput, &battery.stune.pidOutput, tuner.ZN_PID, tuner.directIP, tuner.printALL),
          green(greenLed),
          yellow(yellowLed),
          mqtt(espClient)
    {
        
        adc1_config_width(ADC_WIDTH_12Bit);
        adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH_BIT_12, V_REF, &characteristics);

        heaterPID.SetMode(QuickPID::Control::manual);
        heaterPID.SetOutputLimits(0, 0);

        // client.setInsecure()
        #ifdef TELEGRAM_ENABLED
        client.setInsecure();
        #endif

    }
    
    Blinker green;
    Blinker yellow;

    ~Battery();
    Battery(const Battery&) = delete;
    Battery& operator=(const Battery&) = delete;

    QuickPID heaterPID;
    sTune tuner;

    OneWire oneWire;            // Create OneWire instance
    DallasTemperature dallas;   // Create DallasTemperature instance

    // LAN remote control and monitoring
    WiFiClient espClient;
    PubSubClient mqtt;

    float lastVoltage = 0.0;
    float lastTemperature = 0.0;

    esp_adc_cal_characteristics_t characteristics;

    TuneState tuningState = STARTUP;
    unsigned long tuningMillis = 0;
};

#endif // BATTERY_H
