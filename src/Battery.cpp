#include "Battery.h"
#include <esp_adc_cal.h>
#include <Preferences.h>

Battery* Battery::instance = nullptr;

Battery::Battery(   int tempSensor, 
                    int voltagePin, 
                    int heaterPin, 
                    int chargerPin, 
                    int greenLed, 
                    int yellowLed, 
                    int redLed) 
                :   oneWire(tempSensor),
                    dallas(&oneWire),
                    dallaz(&dallas),
                    voltagePin(voltagePin),
                    heaterPin(heaterPin),
                    chargerPin(chargerPin),
                    heaterPID(&pidInput, &pidOutput, &pidSetpoint, kp, ki, kd, QuickPID::Action::direct),
                    red(redLed),
                    yellow(yellowLed),
                    green(greenLed),
                    batry({0, 0, 0, 0, 0, 0, 0, 0}) 
                {
                    heaterPID.SetTunings(kp, ki, kd); //apply PID gains
                    heaterPID.SetMode(QuickPID::Control::automatic);   
                    heaterPID.SetOutputLimits(0, 255);
                    heaterPID.SetSampleTimeUs(1000 * 1000); // 1 second.
                    heaterPID.SetAntiWindupMode(QuickPID::iAwMode::iAwClamp); 

                    loadSettings();

                    MOVAIndex = 0;

                    instance = this;
}

Battery* Battery::getInstance() {
    if (instance == nullptr) {
        instance = new Battery();
    }
    return instance;
}

void Battery::setup() {
    dallaz.begin(NonBlockingDallas::resolution_10, 1000);
    dallaz.onIntervalElapsed(handleIntervalElapsed);
}

void Battery::loop() {

    dallaz.update();
    heaterPID.Compute();
    handleBatteryControl();
}

void Battery::handleBatteryControl() {
   
    VoltageState vState = getVoltageState(voltagePrecent, ecoPrecentVoltage, boostPrecentVoltage);
    
    TempState tState = getTempState(temperature, boostTemp, ecoTemp, boostActive);

    if (overrideVoltageTimer || (currentMillis - lastVoltageStateTime >= 60000)) {
        switch (vState) {
            case LOW_VOLTAGE_WARNING:
                // Logic for low voltage handling (e.g., stop heating completely,  error!)
                break;
            case PROTECTED:
                // Logic for protected mode, blink warning, reduce heating goal for +1 celsius degree. 
                break;
            case ECONOMY:
                // Eco mode logic (Normal operation)
                break;
            case BOOST:
                // Boost logic (override eco mode, heat more -> more power. 
                break;
            default:
                // Waiting on initilization startup to Handle unexpected state
                break;
            }
        lastVoltageStateTime = currentMillis;
        overrideVoltageTimer = false;
        readVoltage();    
    }

    if (overrideTempTimer || (currentMillis - lastTempStateTime >= 1000)) {
        switch (tState) {
            case SUBZERO:
                // Logic for cold temperatures (disable charging, red led on!)
                break;
            case ECO_TEMP:
                // Normal battery operations
                break;
            case BOOST_TEMP:
                // Boost heating logic, reset boost mode once temp is achieved
                boostActive = false;
                break;
            case OVER_TEMP:
                // Disable charging and heating
                break;
            case TEMP_WARNING:
                // Logic for temperature warning (e.g., reduce heating goal)
                break;
            case TBOOST_RESET:
                // Logic for resetting boost mode (e.g., turn off boost mode)
                break;
            default:
                // Waiting on initilization startup to Handle unexpected state
                break;
        }
        lastTempStateTime = currentMillis;
        overrideTempTimer = false;
        readTemperature();
    }
}
/*






 State Machine for battery voltage levels and temperature levels
- add Boost reset state
*/
Battery::VoltageState Battery::getVoltageState(     int voltagePrecent, 
                                                    int ecoPrecentVoltage, 
                                                    int boostPrecentVoltage) {
    if (voltagePrecent < 30) {
        return LOW_VOLTAGE_WARNING;
    } else if (voltagePrecent >= 30 && voltagePrecent < 50) {
        return PROTECTED;
    } else if (voltagePrecent >= 50 && voltagePrecent < ecoPrecentVoltage) {
        return ECONOMY;
    } else if (voltagePrecent >= ecoPrecentVoltage && voltagePrecent < boostPrecentVoltage) {
        return BOOST;
    } else if (voltagePrecent >= boostPrecentVoltage) {
        return VBOOST_RESET;  // Add boost reset state
    } else {
        return LOW_VOLTAGE_WARNING;  // Fallback state
    }
}
/*





    State machine for temperature levels, to simplify the logic 
    for the battery control.
*/
Battery::TempState Battery::getTempState(   float temperature, 
                                            int boostTemp, 
                                            int ecoTemp, 
                                            bool boostActive) {
    if (boostActive) {
        if (temperature < boostTemp && temperature > ecoTemp) {
            return BOOST_TEMP;
        } else if (temperature >= boostTemp) {
            return TBOOST_RESET;
        }
    } else {
        if (temperature < 0) {
            return SUBZERO;
        } else if (temperature < ecoTemp) {
            return ECO_TEMP;
        } else if (temperature > ecoTemp && temperature < boostTemp) {
            return BOOST_TEMP;
        } else if (temperature > boostTemp && temperature < 40) {
            return OVER_TEMP;
        } else if (temperature >= 40) {
            return TEMP_WARNING;
        }
    }
    return ECO_TEMP; // Default state
}
/*




    Battery Load and Save function calls, to store settings in 
    the NVS memory. 

        - lets ditch EEPROM
        - lets use Preferences.h library for ESP32
        - Maybe Future: Have some sort of logging in case of errors.
        - maybe Future: Black Box kind of logging for backtracking faults 
*/     
void Battery::saveSettings() {
    preferences.begin("battery_settings", false); // Open preferences with namespace "battery_settings" 

    // Sort batry related components
    preferences.putUInt("size", batry.size);
    preferences.putUInt("resistance", batry.resistance);
    preferences.putUInt("ecoTemp", batry.ecoTempPrecent);
    preferences.putUShort("ecoVolt", batry.ecoVoltPrecent);
    preferences.putUShort("boostVolt", batry.boostVoltPrecent);
    preferences.putUInt("boostTemp", batry.boostTempPrecent);

    // Sort settings related components
    preferences.putBool("httpAccess", settings.httpAccess);
    preferences.putBool("initialSetup", settings.initialSetup);
    preferences.putUInt("clamp", settings.clamp);
    preferences.putUInt("kp", settings.kp);
    preferences.putBool("mqtt", settings.mqtt);
    preferences.putBool("accessPointMode", settings.accessPointMode);

    for (int i = 0; i < 13; i++) preferences.putUChar(("username" + String(i)).c_str(), settings.username[i]);
    for (int i = 0; i < 13; i++) preferences.putUChar(("password" + String(i)).c_str(), settings.password[i]);

    preferences.end(); // Close preferences, commit changes
}

void Battery::loadSettings() {
    preferences.begin("battery_settings", false); // Open preferences with namespace "battery_settings" 

    // Sort batry related components
    batry.size = preferences.getUInt("size", 0);
    batry.resistance = preferences.getUInt("resistance", 0);
    batry.ecoTempPrecent = preferences.getUInt("ecoTemp", 0);
    batry.ecoVoltPrecent = preferences.getUShort("ecoVolt", 0);
    batry.boostVoltPrecent = preferences.getUShort("boostVolt", 0);
    batry.boostTempPrecent = preferences.getUInt("boostTemp", 0);

    // Sort settings related components
    settings.httpAccess = preferences.getBool("httpAccess", false);
    settings.initialSetup = preferences.getBool("initialSetup", false);
    settings.clamp = preferences.getUInt("clamp", 0);
    settings.kp = preferences.getUInt("kp", 0);
    settings.mqtt = preferences.getBool("mqtt", false);
    settings.accessPointMode = preferences.getBool("accessPointMode", false);

    for (int i = 0; i < 13; i++) settings.username[i] = preferences.getUChar(("username" + String(i)).c_str(), 0);
    for (int i = 0; i < 13; i++) settings.password[i] = preferences.getUChar(("password" + String(i)).c_str(), 0);

    preferences.end(); // Close preferences
}
/*





    Battery voltage reading function call. 
        -   In the future we could have some gaussian filter, 
            to compare reading in the previous reading. Like Moving avarage
            but in a array. And this would lead into actionable data.
*/
void Battery::readVoltage() {

    adc1_config_width(ADC_WIDTH_12Bit);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
    esp_adc_cal_characteristics_t characteristics;
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH_BIT_12, V_REF, &characteristics);

    u_int32_t voltVal = 0;
    for (int i = 0; i < 3; i++) {
        voltVal += adc1_get_raw(ADC_CHANNEL);
    }
    voltVal /= 3;

    if (MOVAIndex >= MOVING_AVG_SIZE) MOVAIndex = 0;
    MOVAreadings[MOVAIndex] = esp_adc_cal_raw_to_voltage(voltVal, &characteristics) * 30.81;
    MOVAIndex++;

    MOVASum = 0;
    for (int i = 0; i < MOVING_AVG_SIZE; i++) {
        MOVASum += MOVAreadings[i];
    }

    MOVASum /= MOVING_AVG_SIZE;
    if (MOVASum > 5000 && MOVASum < 100000) {
        voltage = MOVASum / 1000;
        
        this->currentMilliVoltage = voltage;
        this->currentPrecentVoltage = (voltage / batry.size * 4200) * 100;

    } else {
        currentMilliVoltage = 0;
        currentPrecentVoltage = 0;
    }
}
/*





    Battery unit conversion, from precentage to millivoltage.
        a) To avoid floats and doubles, we multiply by 1000 to get the millivoltage.
        b)to get faster state machine looping, without too much overhead.
*/
int Battery::btryToMilliVoltage(int precent) {
    return ((batry.size * 3) + (batry.size * 1.2 * (precent / 1))) * 1000;
}

int Battery::getVoltageInPercentage(uint32_t milliVoltage) {
    uint32_t minVoltage = 3000;  // Minimum voltage in millivolts (3V)
    uint32_t maxVoltage = 4200;  // Maximum voltage in millivolts (4.2V)

    if (milliVoltage < minVoltage * batry.size) {
        return 0;  // Below minimum voltage, 0% usable area
    } else if (milliVoltage > maxVoltage * batry.size) {
        return 100;  // Above maximum voltage, 100% usable area
    } else {
        uint32_t voltageRange = (maxVoltage - minVoltage) * batry.size;
        uint32_t voltageDifference = milliVoltage - minVoltage * batry.size;
        return (voltageDifference / voltageRange) * 100 ;  // Calculate percentage of usable area
    }
}
/*





    Setters and Getters for battery economy voltage settings.
        ESPUI interface for the user to set the battery voltage settings.
    
*/
bool Battery::setEcoPrecentVoltage( int value) {
    if(value > 50 && value < 100) {
        batry.ecoVoltPrecent = value;
        return true;
    }
    return false;
}

int Battery::getEcoPrecentVoltage() {
    return batry.ecoVoltPrecent;
}
/*





    Setters and Getters for battery BOOST settings
 
        todo: This is for the user API through ESPUI. 
        Have some filtering, maybe some error handling. Lets worry about that later on.
*/
void Battery::setBoostPrecentVoltage(int boostPrecentVoltage) {
    batry.boostVoltPrecent = boostPrecentVoltage;
}

int Battery::getBoostPrecentVoltage() {
    return batry.boostVoltPrecent;
}
/*





    Battery DS18B20 temp sensor handling. 

    Maybe have some moving avarage sum, where we can get the average temperature over a period of time.
    This is to compare the reading, if it differs from the previous reading, we can have a failsafe.

*/
void Battery::readTemperature() {
    if (dallas.requestTemperaturesByIndex(0)) {
        currentTemperature = dallas.getTempCByIndex(0);
        batry.temperature = currentTemperature * 10;
        if (currentTemperature == DEVICE_DISCONNECTED_C) {
            currentTemperature = 255;  // Failsafe to stop heating
            temperatureFailsafe = true;
        } else {
            temperatureFailsafe = false;
        }
    } else {
        currentTemperature = 255;  // Failsafe if reading fails
        temperatureFailsafe = true;
    }
}

bool Battery::isTemperatureSafe() {
    return currentTemperature != 255;
}

float Battery::getTemperature() {
    float tempReturn = 0;
    tempReturn = batry.temperature / 10;

    if(tempReturn < 80 && tempReturn > -50) {
        return tempReturn;
    }
    return 0;
}   

void Battery::handleIntervalElapsed(int deviceIndex, int32_t temperatureRAW) {
    // Access the instance via the static pointer
    if (instance) {
        float tC = instance->dallaz.rawToCelsius(temperatureRAW);  
        if (tC == DEVICE_DISCONNECTED_C) {
            instance->currentTemperature = 255;  // Failsafe to stop heating
            instance->temperatureFailsafe = true;
        } else {
            instance->currentTemperature = tC;
            instance->temperatureFailsafe = false;
        }
    }
}
/*







    Battery's most important function. Settings its nominal string value. 
        For charging, economy and boost mode definitions.
        In the future this cuold maybe have automated, but thats too much work for now. 
*/
bool Battery::setNominalString(int size) {
    if(size > 6 && size < 22)
        {
            batry.size = size;
            return true;
        }
    else return false;
}

int Battery::getNominalString() {
    return batry.size;
}
/*





    Setter and Getter for battery boost temperature settings. 
        - ESPUI interface for the user to set the battery temperature settings.
*/
int Battery::getEcoTemp() {
    return batry.ecoTempPrecent;
}

bool Battery::setEcoTemp(int ecoTemp) {
    if(ecoTemp > 5 && ecoTemp < 30 && ecoTemp < batry.boostTempPrecent) {
        batry.ecoTempPrecent = ecoTemp;
        return true;
    }
    return false;
}
/*




    SetGetter for Temperature boost settings. 
        - ESPUI interface for the user to set the battery temperature settings.
*/
int Battery::getBoostTemp() {
    return batry.boostTempPrecent;
}

bool Battery::setBoostTemp(int boostTemp) {
    if(boostTemp > 10 && boostTemp < 35 && boostTemp > batry.ecoTempPrecent) {
        batry.boostTempPrecent = boostTemp;
        return true;
    }
    return false;
}
/*







*/







/*

// Bolean switches
bool batt.getBoostActiveTemp();
bool batt.setBoostActiveTemp();

// boolean switches
bool batt.getBoostActiveVoltage();
bool batt.setBoostActiveVoltage();




ESPUI callbacks

public: 
batt.getTmp();  ---> Battery::getTemperature();
batt.getVoltage();   ---> Battery::getMilliVoltage();




batt.getIP();


// Charger manual override
bool batt.setChargerOff();                       // override charger to off
bool batt.getChargerState();                      // get charger state



char batt.getHostname();                         // get hostname
char batt.setHostname();                         // set hostname

bool batt.setHeaterOff();                        // override heater to off
bool batt.getHeaterOnOff();                      // get heater state


*/