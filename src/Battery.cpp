#include "Battery.h"
#include <esp_adc_cal.h>
#include <Preferences.h>
#include <ESPUI.h>
            
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
                    batry() 
                {
                    heaterPID.SetTunings(kp, ki, kd); //apply PID gains
                    heaterPID.SetMode(QuickPID::Control::automatic);   
                    heaterPID.SetOutputLimits(0, 255);
                    heaterPID.SetSampleTimeUs(1000 * 1000); // 1 second.
                    heaterPID.SetAntiWindupMode(QuickPID::iAwMode::iAwClamp); 

                    MOVAIndex = 0;

                    
                    instance = this;

}

Battery* Battery::getInstance() {
    if (instance == nullptr) {
        instance = new Battery();
    }
    return instance;
}

void Battery::begin()
{

        pinMode(redLed, OUTPUT);
        digitalWrite(redLed, HIGH);
        // preferences.begin("btry", false);


        loadSettings();
        // saveSettings();
        // loadSettings();

    setup();
}


void Battery::setup() {

    dallaz.begin(NonBlockingDallas::resolution_10, 1000);
    dallaz.onIntervalElapsed(handleIntervalElapsed);

    green.start();
    yellow.start();
    green.setDelay(100);
    yellow.setDelay(100);
}

void Battery::loop() {

    dallaz.update();
    heaterPID.Compute();
    handleBatteryControl();

    // redLed.blink();
    green.blink();   
    yellow.blink();  

}
void Battery::handleBatteryControl() {

    uint32_t currentMillis = millis(); 
    
    if (overrideVoltageTimer || (currentMillis - lastVoltageStateTime >= 5000)) {

        VoltageState vState = getVoltageState(batry.voltageInPrecent, batry.ecoVoltPrecent, batry.boostVoltPrecent);
        switch (vState) {
            case LOW_VOLTAGE_WARNING:

                    digitalWrite(redLed, LOW); 
                    Serial.println("V: Low voltage warning");

                    // Logic for low voltage handling (e.g., stop heating completely,  error!)
                break;
            case PROTECTED:
                    Serial.println("V: Protected mode");
                    digitalWrite(redLed, LOW);
                    green.setDelay(0, 1000);
                // Logic for protected mode, blink warning, reduce heating goal for +1 celsius degree. 
                break;
            case ECONOMY:
                    Serial.println("V: Economy mode");
                    green.setDelay(0, 1000);
                    yellow.setDelay(1000, 0);

                // Eco mode logic (Normal operation)
                break;
            case BOOST:
                    Serial.println("V: Boost mode");
                    yellow.setDelay(0, 1000);
                    green.setDelay(1000, 0);

                // Boost logic (override eco mode, heat more -> more power. 
                break;
            default:
                    Serial.println("V: Default state");
                    green.setDelay(500, 500);
                    yellow.setDelay(500, 500);
                // Waiting on initilization startup to Handle unexpected state
                break;
            }
        lastVoltageStateTime = currentMillis;
        overrideVoltageTimer = false;
        readVoltage();    
        Serial.println("Voltage Measurement loop");
        Serial.print(voltagePrecent);
        Serial.print(ecoPrecentVoltage);
        Serial.print(boostPrecentVoltage);
        Serial.println("  ");

    }

    if (overrideTempTimer || (currentMillis - lastTempStateTime >= 5000)) {

    TempState tState = getTempState(batry.temperature, batry.boostTemp, batry.ecoTemp, batry.tempBoostActive);
        switch (tState) {
            case SUBZERO:
                Serial.println("SubZero temperature detected");
                // Logic for cold temperatures (disable charging, red led on!)
                break;
            case ECO_TEMP:
                Serial.println("Eco temperature detected");
                // Normal battery operations
                break;
            case BOOST_TEMP:
                Serial.println("Boost temperature detected");
                // Boost heating logic, reset boost mode once temp is achieved
                break;
            case OVER_TEMP:
                Serial.println("Over temperature detected");
                // Disable charging and heating
                break;
            case TEMP_WARNING:
                Serial.println("Temperature warning detected");
                // Logic for temperature warning (e.g., reduce heating goal)
                break;
            case TBOOST_RESET:
                Serial.println("Boost reset temperature detected");
                batry.tempBoostActive = false;
                // Logic for resetting boost mode (e.g., turn off boost mode)
                break;
            default:
                Serial.println("Default state");
                // Waiting on initilization startup to Handle unexpected state
                break;
        }
        lastTempStateTime = currentMillis;
        overrideTempTimer = false;
        // readTemperature();
        Serial.println("Temperature Measurement loop");
        Serial.print(batry.temperature);
        Serial.print(batry.boostTemp);
        Serial.print(batry.ecoTemp);
        Serial.println("  ");
        //temperature, boostTemp, ecoTemp, boostActive

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
    preferences.begin("btry", false); // Open preferences with namespace "battery_settings" 

    preferences.putUInt("size", batry.size);
    preferences.putUInt("temperature", batry.temperature);
    preferences.putUInt("currentVolt", batry.voltageInPrecent);
    preferences.putUInt("ecoVolt", batry.ecoVoltPrecent);
    preferences.putUInt("boostVolt", batry.boostVoltPrecent);
    preferences.putUInt("ecoTemp", batry.ecoTemp);
    preferences.putUInt("boostTemp", batry.boostTemp);
    preferences.putUInt("resistance", batry.resistance);




/*
    if(!setup_done) {
        preferences.clear();

        preferences.putUInt("size",             (u_int32_t)10);
        preferences.putUInt("temperature",      (int)10);
        preferences.putUInt("voltageInPrecent", (int)10);
        preferences.putUInt("ecoVolt",          (int)10);
        preferences.putUInt("boostVolt",        (int)10);
        preferences.putUInt("ecoTemp",          (int)10);
        preferences.putUInt("boostTemp",        (int)10);
        preferences.putUInt("resistance",       (int)10);
    
        setup_done = true;
    } else {

    // Sort batry related components
    preferences.putUInt("size", batry.size);
    preferences.putUInt("temperature", batry.temperature);
    preferences.putUInt("voltageInPrecent", batry.voltageInPrecent);
    preferences.putUInt("ecoVolt", batry.ecoVoltPrecent);
    preferences.putUInt("boostVolt", batry.boostVoltPrecent);
    preferences.putUInt("ecoTemp", batry.ecoTemp);
    preferences.putUInt("boostTemp", batry.boostTemp);
    preferences.putUInt("resistance", batry.resistance);
    

    preferences.putBool("httpAccess", settings.httpAccess);
    preferences.putBool("initialSetup", settings.initialSetup);
    preferences.putUInt("clamp", settings.clamp);
    preferences.putUInt("kp", settings.kp);
    preferences.putBool("mqtt", settings.mqtt);
    preferences.putBool("accessPointMode", settings.accessPointMode);

    for (int i = 0; i < 13; i++) preferences.putUChar(("username" + String(i)).c_str(), settings.username[i]);
    for (int i = 0; i < 13; i++) preferences.putUChar(("password" + String(i)).c_str(), settings.password[i]);
*/


    preferences.end(); // Close preferences, commit changes
}

void Battery::loadSettings() {
    preferences.begin("btry", false); // Open preferences with namespace "battery_settings" 

    // Sort batry related components
    batry.size = preferences.getUInt("size", 1);
    Serial.print("Battery size loaded from memory: ");
    Serial.println(batry.size);

    batry.temperature = preferences.getUInt("temperature", 1);
    Serial.print("Battery temperature loaded from memory: ");
    Serial.println(batry.temperature);

    batry.voltageInPrecent = preferences.getUInt("currentVolt", 1);
    Serial.print("Battery voltage loaded from memory: ");
    Serial.println(batry.voltageInPrecent);

    batry.resistance = preferences.getUInt("resistance", 1);
    Serial.print("Battery resistance loaded from memory: ");
    Serial.println(batry.resistance);

    batry.ecoVoltPrecent = preferences.getUInt("ecoVolt", 1);
    Serial.print("Battery eco voltage loaded from memory: ");
    Serial.println(batry.ecoVoltPrecent);

    batry.boostVoltPrecent = preferences.getUInt("boostVolt", 1);
    Serial.print("Battery boost voltage loaded from memory: ");
    Serial.println(batry.boostVoltPrecent);

    batry.boostTemp = preferences.getUInt("boostTemp", 1);
    Serial.print("Battery boost temperature loaded from memory: ");
    Serial.println(batry.boostTemp);

    batry.ecoTemp = preferences.getUInt("ecoTemp", 1);
    Serial.print("Battery eco temperature loaded from memory: ");
    Serial.println(batry.ecoTemp);

/*
    // Sort settings related components
    settings.httpAccess = preferences.getBool("httpAccess", false);
    settings.initialSetup = preferences.getBool("initialSetup", false);
    settings.clamp = preferences.getUInt("clamp", 0);
    settings.kp = preferences.getUInt("kp", 0);
    settings.mqtt = preferences.getBool("mqtt", false);
    settings.accessPointMode = preferences.getBool("accessPointMode", false);

    for (int i = 0; i < 13; i++) settings.username[i] = preferences.getUChar(("username" + String(i)).c_str(), 0);
    for (int i = 0; i < 13; i++) settings.password[i] = preferences.getUChar(("password" + String(i)).c_str(), 0);
*/
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
        
        currentMilliVoltage = voltage;
        Serial.println("Voltage reading success:   ");
        Serial.print(voltage);

        currentPrecentVoltage = (voltage / batry.size * 4200) * 100;
        voltagePrecent = constrain(((voltage / batry.size * 4200) * 100), 10, 100);
        batry.voltageInPrecent = voltagePrecent;
        
        Serial.println("Voltage reading success ");
        Serial.print(voltagePrecent);
        Serial.print("  ");
        Serial.print(voltage);
        
    } else {
        // currentMilliVoltage = 0;
        // currentPrecentVoltage = 0;
        Serial.println("Voltage reading error");
    }
}
/*





    Battery unit conversion, from precentage to millivoltage.
        a) To avoid floats and doubles, we multiply by 1000 to get the millivoltage.
        b)to get faster state machine looping, without too much overhead.
*/
float Battery::btryToVoltage(int precent) {


    if (precent > 0 && precent < 100) {
        float milliVolts = 0.0f;
        milliVolts = ((int)batry.size * (uint32_t)3000);
        milliVolts += ((int)batry.size * (uint32_t)(1200 * (int)precent ) / 100);
        return milliVolts / 1000;
    }
/*        
        (batry.size * 1.2 * (precent / 100));
        return milliVolts;
        } else {
        return 0;
    }
    */

    Serial.print("Result:  ");
    Serial.print(batry.size);
    Serial.print("  ");
    Serial.print(precent);
    Serial.print("  ");
    Serial.print("btrysize:  ");
    float volttis = ((batry.size * 3) + (batry.size * (float)1.2 * (precent / 100)));
    Serial.print("  ");
    Serial.println(volttis);

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
        uint32_t result = (voltageDifference / voltageRange) * 100 ;  // Calculate percentage of usable area
        // Serial.print("Result:  ");
        Serial.println(result);
        return result;
    }
}

int Battery::getBatteryDODprecent() {
    Serial.print("Result:  ");
    Serial.println(batry.voltageInPrecent);
    return batry.voltageInPrecent;
}
/*





    Setters and Getters for battery economy voltage settings.
        ESPUI interface for the user to set the battery voltage settings.
    
    ECONOMY CHARGING
*/
bool Battery::setEcoPrecentVoltage(int value) {
    if(value > 50 && value < 100 && value < batry.boostVoltPrecent) {
        batry.ecoVoltPrecent = value;
        return true;
    }
    else return false;
}

int Battery::getEcoPrecentVoltage() {
    return batry.ecoVoltPrecent;
}
/*





    Setters and Getters for battery BOOST settings
 
        todo: This is for the user API through ESPUI. 
        Have some filtering, maybe some error handling. Lets worry about that later on.
*/
bool Battery::setBoostPrecentVoltage(int boostPrecentVoltage) {
        if(boostPrecentVoltage > batry.ecoVoltPrecent && boostPrecentVoltage < 100) {
            batry.boostVoltPrecent = boostPrecentVoltage;
            return true;
        }
        else return false;        
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
        if(currentTemperature < 100 && currentTemperature > -50) {
            batry.temperature = currentTemperature * 10;
        }
        
        /*
        
        This is plain Dallas, without the NonBlockingDallas. REMOVE!!! 
        
        
        */


        temperature = currentTemperature;
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
    float tempReturn = 0.0f;
    tempReturn = batry.temperature / 10;

    if(tempReturn < 80 && tempReturn > -50) {
        return tempReturn;
    }
    else return 0;
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
            instance->batry.temperature = tC * 10;
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
    return batry.ecoTemp;
}

bool Battery::setEcoTemp(int ecoTemp) {
    if(ecoTemp > 5 && ecoTemp < 30 && ecoTemp < batry.boostTemp) {
        batry.ecoTemp = ecoTemp;
        return true;
    }
    else return false;
}
/*




    SetGetter for Temperature boost settings. 
        - ESPUI interface for the user to set the battery temperature settings.
*/
int Battery::getBoostTemp() {
    return batry.boostTemp;
}

bool Battery::setBoostTemp(int boostTemp) {
    if(boostTemp > 10 && boostTemp < 35 && boostTemp > batry.ecoTemp) {
        batry.boostTemp = boostTemp;
        return true;
    }
    else return false;
}
/*




    Temp and Voltage boost settings. 
        - Activate and deactivate the boost settings. 
        - ESPUI interface for the user to set the battery temperature settings.

        Have it reset after certain timeout, just in case.

*/
bool Battery::activateTemperatureBoost(bool tempBoostActive) {
    if(tempBoostActive && !batry.tempBoostActive) {
        batry.tempBoostActive = true;
        return true;
    }
    else if(tempBoostActive && batry.tempBoostActive) {
        Serial.println("Temperature boost is already active -> toggle to deactivate");
        batry.tempBoostActive = false;
        return true;
    }
    else return false;
}

bool Battery::getActivateTemperatureBoost() {
    if (batry.tempBoostActive)
        return true;
    else
        return false;
}

bool Battery::activateVoltageBoost(bool voltBoostActive) {
    if(voltBoostActive && !batry.voltBoostActive) {
        batry.voltBoostActive = true;
        return true;
    }
    else if (voltBoostActive && batry.voltBoostActive) {
        batry.voltBoostActive = false;
        Serial.println("Voltage boost is already active -> toggle to deactivate");
        return true;
    }
    else return false;
}

bool Battery::getActivateVoltageBoost() {
    if (batry.voltBoostActive)
        return true;
    else
        return false;
}


float Battery::calculateChargeTime(float batteryCapacity, float chargerCurrent, float initialPercentage, float targetPercentage) {
    // Calculate the charge needed in Ah
    float chargeNeeded = batteryCapacity * (targetPercentage - initialPercentage) / 100.0;

    // Calculate the time required in hours
    float timeRequired = chargeNeeded / chargerCurrent;

    return timeRequired;
}