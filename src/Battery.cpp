#include "Battery.h"
#include <esp_adc_cal.h>
#include <Preferences.h>
#include <OneWire.h>
#include <ESPUI.h>
#include <PubSubClient.h>

batterys Battery::batry;


Battery::Battery(   int tempSensor, 
                    int voltagePin, 
                    int heaterPin, 
                    int chargerPin, 
                    int greenLed, 
                    int yellowLed, 
                    int redLed) 
                :   oneWire(tempSensor),
                    dallas(&oneWire),
                    voltagePin(voltagePin),
                    heaterPin(heaterPin),
                    chargerPin(chargerPin),
                    heaterPID(&pidInput, &pidOutput, &pidSetpoint, kp, ki, kd, QuickPID::Action::direct),
                    red(redLed),
                    yellow(yellowLed),
                    green(greenLed)
                {
                    heaterPID.SetTunings(kp, ki, kd); //apply PID gains
                    heaterPID.SetMode(QuickPID::Control::automatic);   
                    heaterPID.SetOutputLimits(0, 25);
                    heaterPID.SetSampleTimeUs(1000 * 1000); // 1 second.
                    heaterPID.SetAntiWindupMode(QuickPID::iAwMode::iAwClamp); 
                }


void Battery::begin()
{
    pinMode(redLed, OUTPUT);
    digitalWrite(redLed, HIGH);
    setup();
}


void Battery::setup() {

    dallas.begin();
    green.start();
    yellow.start();
    green.setDelay(100);
    yellow.setDelay(100);

    // lets have the battery not start heating during the startup.
    heaterPID.SetMode(QuickPID::Control::manual);
    heaterPID.SetOutputLimits(0, 25);  // 10% power to start with
    heaterPID.SetSampleTimeUs(1000 * 1000); // 1 second.

    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(heaterPin, PWM_CHANNEL);

    adc1_config_width(ADC_WIDTH_12Bit);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH_BIT_12, V_REF, &characteristics);
}

void Battery::loop() {

    handleBatteryControl();

    readVoltage(5);                // read voltage every X seconds

    heaterPID.Compute();

    // redLed.blink();
    green.blink();   
    yellow.blink();  

    if (millis() - dallasTime  >= 2500) {
        dallasTime = millis();
        float temperature = readTemperature();
        if (temperature == DEVICE_DISCONNECTED_C) {
            Serial.println("Error: Could not read temperature data");
        } else {
            batry.temperature = temperature;
            // Serial.print("Temp: ");
            // Serial.println(batry.temperature);
        }
    }
}
/*




    Control the PWM
*/
void Battery::controlHeaterPWM(uint8_t dutyCycle) {
    ledcWrite(PWM_CHANNEL, dutyCycle);
}
/*





*/
void Battery::updateHeaterPID() {
    if (heaterPID.GetMode() == static_cast<uint8_t>(QuickPID::Control::automatic)) {
        this->pidInput = batry.temperature; // Update the input with the current temperature
        this->pidSetpoint = batry.wantedTemp; // Update the setpoint with the wanted temperature
        controlHeaterPWM(static_cast<uint8_t>(pidOutput)); // Control the heater with the PID output
    }
}



/*


        Dallas temp sensor function

        @bried: Reads the temperature from the Dallas sensor
        @return: float
*/
float Battery::readTemperature() {
    dallas.requestTemperatures();
    return dallas.getTempCByIndex(0);
}


void Battery::handleBatteryControl() {
    uint32_t currentMillis = millis(); 
    
    if (currentMillis - lastVoltageStateTime >= 5000) {

        VoltageState vState = getVoltageState(batry.voltageInPrecent, batry.ecoVoltPrecent, batry.boostVoltPrecent);
        TempState tState;

        switch (vState) {
            case LAST_RESORT:

                    Serial.println("V: Last resort");
                    digitalWrite(redLed, LOW);  // inverted.
                    tState = getTempState(batry.temperature, 3, 2);                             // lets lower the target for temperature        
                    if(tState != SUBZERO && tState != TEMP_WARNING) charger(true);               // lets enable the charger unless subzero temp
              
                break;
            case LOW_VOLTAGE_WARNING:

                    Serial.println("V: Low voltage warning");
                    tState = getTempState(batry.temperature, batry.ecoTemp, batry.ecoTemp);
                    if(tState != SUBZERO && tState != TEMP_WARNING) charger(true);
                    digitalWrite(redLed, LOW); 
                    yellow.setDelay(0, 1000);

                // Logic for low voltage handling (e.g., stop heating completely,  error!)
                break;
            case PROTECTED:

                    Serial.println("V: Protected mode");
                    tState = getTempState(batry.temperature, batry.boostTemp, batry.ecoTemp);
                    digitalWrite(redLed, HIGH);
                    green.setDelay(500, 500);
                    yellow.setDelay(1000, 1000);
                    if(tState != SUBZERO && tState != TEMP_WARNING) charger(true);
                // Logic for protected mode, blink warning, reduce heating goal for +1 celsius degree. 
                break;
            case ECONOMY:

                    tState = getTempState(batry.temperature, batry.ecoTemp, batry.ecoTemp);
                    Serial.println("V: Economy mode");
                    green.setDelay(0, 1000);
                    yellow.setDelay(1000, 0);
                    if(tState != SUBZERO && tState != TEMP_WARNING ) charger(true);

                // Eco mode logic (Normal operation)

                break;
            case BOOST:
                    Serial.println("V: Boost mode");
                    tState = getTempState(batry.temperature, batry.boostTemp, batry.ecoTemp);
                    if ((tState != SUBZERO && tState != TEMP_WARNING ) && batry.voltBoostActive) charger(true);
                    else charger(false);
                    yellow.setDelay(200, 1000);
                    green.setDelay(1000, 200);

                // Boost logic (override eco mode, heat more -> more power. 
                break;
            default:
                    tState = getTempState(batry.temperature, batry.boostTemp, batry.ecoTemp);
                    Serial.println("V: Default state");
                    green.setDelay(500, 2500);
                    yellow.setDelay(2500, 500);
                    if(tState != SUBZERO && tState != TEMP_WARNING ) charger(true);
                    else charger(false);

                // Waiting on initilization startup to Handle unexpected state
                break;
            }

    // TempState tState = getTempState(batry.temperature, batry.boostTemp, batry.ecoTemp, batry.tempBoostActive);

        switch (tState) {
            case SUBZERO:

                Serial.println("SubZero temperature detected");
                charger(false);
                // Logic for cold temperatures (disable charging, red led on!) - SLOW HEATING

                break;
            case ECO_TEMP:
                Serial.println("Eco temperature detected");

                if(batry.tempBoostActive) {
                    batry.wantedTemp = batry.boostTemp;
                }
                else {
                batry.wantedTemp = batry.ecoTemp;
                }


                // Normal battery operations
                break;
            case BOOST_TEMP:
                Serial.println("Boost temperature detected");

                if(batry.tempBoostActive) {
                    batry.wantedTemp = batry.boostTemp;
                }
                else {
                    batry.wantedTemp = batry.ecoTemp;
                }

                // Boost heating logic, reset boost mode once temp is achieved
                break;
            case OVER_TEMP:

                batry.wantedTemp = batry.ecoTemp;
                Serial.println("Over temperature detected");

                break;
            case TEMP_WARNING:

                charger(false);
                batry.wantedTemp = 0;

                // Logic for temperature warning (e.g., reduce heating goal)
                Serial.println("Temperature warning detected");
                heaterPID.SetMode(QuickPID::Control::manual);
                batry.wantedTemp = 0;
                controlHeaterPWM(0);
            

                break;
            case TBOOST_RESET:

                Serial.println("Boost reset temperature detected");
                batry.wantedTemp = batry.ecoTemp;
                batry.tempBoostActive = false;
                // Logic for resetting boost mode (e.g., turn off boost mode)

                break;
            default:

                Serial.println("Default state");
                // Waiting on initilization startup to Handle unexpected state
                Serial.println("Default state");

                break;
        }

        lastVoltageStateTime = currentMillis;

    }
}
/*






    State Machine for battery voltage levels and temperature levels
        Todo: add Boost reset state?
        @brief: Simplifies the logic for the battery control
        @return: VoltageState enum
*/
Battery::VoltageState Battery::getVoltageState(     int voltagePrecent, 
                                                    int ecoPrecentVoltage, 
                                                    int boostPrecentVoltage) {
    if (voltagePrecent >= 10 && voltagePrecent < 30) {
        return LAST_RESORT;  
    } else if (voltagePrecent < 30) {
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
                                            int ecoTemp) {
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
        } else {
            return TEMP_WARNING;
        }
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
    preferences.putUInt("size",         constrain(batry.size, 10, 20));
    preferences.putUInt("temperature",  constrain(batry.temperature, 1, 2550));
    preferences.putUInt("currentVolt",  constrain(batry.voltageInPrecent, 1, 100));
    preferences.putUInt("ecoVolt",      constrain(batry.ecoVoltPrecent, 1, 100));
    preferences.putUInt("boostVolt",    constrain(batry.boostVoltPrecent, 1, 100));
    preferences.putUInt("ecoTemp",      constrain(batry.ecoTemp, 1, 2550));
    preferences.putUInt("boostTemp",    constrain(batry.boostTemp, 1, 2550));
    preferences.putUInt("resistance",   constrain(batry.resistance, 1, 255));
    preferences.putUInt("capct",        constrain(batry.capct, 2, 255));
    preferences.putUInt("chrgr",        constrain(batry.chrgr, 1, 5));




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
    batry.size = constrain(preferences.getUInt("size", 1), 7, 20);
    Serial.print("Battery size loaded from memory: ");
    Serial.println(batry.size);

    batry.temperature = constrain(preferences.getUInt("temperature", 1), 1, 255);
    Serial.print("Battery temperature loaded from memory: ");
    Serial.println(batry.temperature);

    batry.voltageInPrecent = constrain(preferences.getUInt("currentVolt", 1), 5, 100);
    Serial.print("Battery voltage loaded from memory: ");
    Serial.println(batry.voltageInPrecent);

    batry.resistance = constrain(preferences.getUInt("resistance", 1), 5, 255);
    Serial.print("Battery resistance loaded from memory: ");
    Serial.println(batry.resistance);

    batry.ecoVoltPrecent = constrain(preferences.getUInt("ecoVolt", 1), 5, 255);
    Serial.print("Battery eco voltage loaded from memory: ");
    Serial.println(batry.ecoVoltPrecent);

    batry.boostVoltPrecent = constrain(preferences.getUInt("boostVolt", 1), 5, 255);
    Serial.print("Battery boost voltage loaded from memory: ");
    Serial.println(batry.boostVoltPrecent);

    batry.boostTemp = constrain(preferences.getUInt("boostTemp", 1), 5, 39);
    Serial.print("Battery boost temperature loaded from memory: ");
    Serial.println(batry.boostTemp);

    batry.ecoTemp = constrain(preferences.getUInt("ecoTemp", 1), 5, 30);
    Serial.print("Battery eco temperature loaded from memory: ");
    Serial.println(batry.ecoTemp);

    batry.capct = constrain(preferences.getUInt("capct", 1), 2, 255);
    Serial.print("Battery capacity loaded from memory: ");
    Serial.println(batry.capct);

    batry.chrgr = constrain(preferences.getUInt("chrgr", 0), 1, 10);
    Serial.print("Battery charger loaded from memory: ");
    Serial.println(batry.chrgr);

    preferences.end(); // Close preferences
}
/*





    Battery voltage reading function call. 
        -   In the future we could have some gaussian filter, 
            to compare reading in the previous reading. Like Moving avarage
            but in a array. And this would lead into actionable data.
*/

void Battery::readVoltage(unsigned long intervalSeconds) {
    unsigned long currentMillis = millis();
    unsigned long intervalMillis = intervalSeconds * 1000;

    if (currentMillis - voltageMillis >= intervalMillis) {
        voltageMillis = currentMillis;

        u_int32_t voltVal = adc1_get_raw(ADC_CHANNEL);
        u_int32_t calibratedVoltage = esp_adc_cal_raw_to_voltage(voltVal, &characteristics) * 30.81;

        if (firstRun) {
            // Initialize all readings to the first calibrated voltage for a smooth start
            for (int i = 0; i < MOVING_AVG_SIZE; i++) {
                MOVAreadings[i] = calibratedVoltage;
            }
            firstRun = false;
        }

        // Update the moving average array
        MOVAreadings[MOVAIndex] = calibratedVoltage;
        MOVAIndex = (MOVAIndex + 1) % MOVING_AVG_SIZE;

        // Calculate the moving average
        MOVASum = 0;
        for (int i = 0; i < MOVING_AVG_SIZE; i++) {
            MOVASum += MOVAreadings[i];
        }
        u_int32_t movingAverage = MOVASum / MOVING_AVG_SIZE;

        // Calculate variance percentage
        u_int32_t varianceSum = 0;
        for (int i = 0; i < MOVING_AVG_SIZE; i++) {
            varianceSum += abs((int32_t)MOVAreadings[i] - (int32_t)movingAverage);
        }
        Serial.print("VarianceSum: ");
        Serial.println(varianceSum);

        u_int32_t variance = (varianceSum * 100) / (MOVING_AVG_SIZE * movingAverage);

        // Store variance in the secondary array
        varianceReadings[MOVAIndex] = variance;

        // Print debug information
        // Serial.print("MOVASum: ");
        // Serial.println(MOVASum);
        Serial.print("Variance: ");
        Serial.println(variance);

        // Check if the moving average is within the valid range
        if (movingAverage > 5000 && movingAverage < 100000) {
            // Update battery size approximation if the new series is larger
            int newSeries = Battery::determineBatterySeries(movingAverage);
            if (batry.sizeApprx < newSeries) {
                batry.sizeApprx = newSeries;
                Serial.print("Battery size approximated to: ");
                Serial.println(batry.sizeApprx);
            }

            // Update voltage and accurate voltage
            batry.milliVoltage = movingAverage;

            // Update battery voltage in percentage
            batry.voltageInPrecent = getVoltageInPercentage(movingAverage);
        } else {
            accurateVoltage = 0; // Set the accurate voltage to 0 in case of reading error
            Serial.println("Voltage reading error");
        }
    }
}

/*





    Battery unit conversion, from precentage to millivoltage.
        a) To avoid floats and doubles, we multiply by 1000 to get the millivoltage.
        b)to get faster state machine looping, without too much overhead.
        c) due to a bug, (negative return) it was left as float
*/
float Battery::btryToVoltage(int precent) {


    if (precent > 0 && precent < 100) {
        float milliVolts = 0.0f;
        milliVolts = (batry.size * 3000);
        milliVolts += (batry.size * (1200 * (int)precent ) / 100);
        return milliVolts / 1000;
    }
    else {

    float volttis = ((batry.size * 3) + (batry.size * (float)1.2 * (precent / 100)));
    Serial.print("btryTolVoltage-Fail:  ");
    Serial.println(volttis);
    return -1;
    }
}

/*





Lets convert the given milliVoltage to precentage at given series 
*/
int Battery::getVoltageInPercentage(uint32_t milliVoltage) {
    uint32_t minVoltage = 3000;  // Minimum voltage in millivolts (3V)
    uint32_t maxVoltage = 4200;  // Maximum voltage in millivolts (4.2V)
    // Serial.print("MilliVoltage:  ");
    // Serial.println(milliVoltage);
    milliVoltage = constrain(milliVoltage, 25000, 100000);
    if (milliVoltage < (minVoltage * batry.size)) {
        return 0;  // Below minimum voltage, 0% usable area
        Serial.println("Below minimum voltage");
    } else if (milliVoltage > (maxVoltage * batry.size)) {
        return 100;  // Above maximum voltage, 100% usable area
        Serial.println("Above maximum voltage");
    } else {
        uint32_t voltageRange = (maxVoltage - minVoltage) * batry.size;
        uint32_t voltageDifference = milliVoltage - ( minVoltage * batry.size) ;
        float result = ((float)voltageDifference / (float)voltageRange) * 100 ;  // Calculate percentage of usable area
        // Serial.print("Voltage left:  ");
        // Serial.println(voltageDifference);
        // Serial.print("Result:  ");
        // Serial.println(result);
        return result;
    }
}

/*



    Batterys Depth of Discharge percentage return.
*/
int Battery::getBatteryDODprecent() {
    // Serial.print("Result:  ");
    // Serial.println(batry.voltageInPrecent);
    return batry.voltageInPrecent;
}
/*





    Setters and Getters for battery economy voltage settings.
        ESPUI interface for the user to set the battery voltage settings.
    
    ECONOMY CHARGING
*/
bool Battery::setEcoPrecentVoltage(int value) {
    if(value > 49 && value < 101 && value < batry.boostVoltPrecent) {
        batry.ecoVoltPrecent = value;
        return true;
    }
    else return false;
}

// @brief Get the battery economy voltage percentage endpoint
int Battery::getEcoPrecentVoltage() {
    return batry.ecoVoltPrecent;
}
/*





    Setters and Getters for battery BOOST settings
 
        todo: This is for the user API through ESPUI. 
        Have some filtering, maybe some error handling. Lets worry about that later on.
*/
bool Battery::setBoostPrecentVoltage(int boostPrecentVoltage) {
        if(boostPrecentVoltage > batry.ecoVoltPrecent && boostPrecentVoltage < 101) {
            batry.boostVoltPrecent = boostPrecentVoltage;
            return true;
        }
        else return false;        
}

// @brief Get the battery boost voltage percentage endpoint
int Battery::getBoostPrecentVoltage() {
    return batry.boostVoltPrecent;
}
/*





    Battery DS18B20 temp sensor handling. 

    Maybe have some moving avarage sum, where we can get the average temperature over a period of time.
    This is to compare the reading, if it differs from the previous reading, we can have a failsafe.

*/




bool Battery::isTemperatureSafe() {
    return currentTemperature != 255;
}

float Battery::getTemperature() {
    float tempReturn = 0.0f;
    tempReturn = batry.temperature;

    if(tempReturn < 80 && tempReturn > -50) {
        return tempReturn;
    }
    else return 1;
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
    if(ecoTemp > 4 && ecoTemp < 30 && ecoTemp < batry.boostTemp) {
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
    if(boostTemp > 10 && boostTemp < 36 && boostTemp > batry.ecoTemp) {
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
        // Serial.println("Temperature boost is already active -> toggle to deactivate");
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
        // Serial.println("Voltage boost is already active -> toggle to deactivate");
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


float Battery::calculateChargeTime(int initialPercentage, int targetPercentage) {
    // Calculate the charge needed in Ah
    float chargeNeeded = batry.capct * ((targetPercentage - initialPercentage) / (float)100.0);
    // Serial.println(chargeNeeded);
    // Calculate the time required in minutes
    float timeRequired = (chargeNeeded / getCharger());
    // Serial.print("timeRequired: ");
    // Serial.println(timeRequired);
    return timeRequired;
}


    // Setter function to set charger current and battery capacity
bool Battery::setCharger(int chargerCurrent) {
    if(chargerCurrent > 0 && chargerCurrent < 6) {
        batry.chrgr = chargerCurrent;
        return true;
    }
    else return false;
}

// Getter for charger current
int Battery::getCharger() {
    return batry.chrgr;
}

// Setter for battery capacity
bool Battery::setCapacity(int capacity) {
    if (capacity > 0 && capacity < 250) {
        batry.capct = capacity;
        return true;
    } else {
        return false;
    }
}

// Getter for battery capacity
int Battery::getCapacity()  {
    return batry.capct;
}


/*
    // Getter function to get charger current and battery capacity
    uint32_t Battery::getCharger(uint32_t chargerCurrent, uint32_t batteryCapacity)  {
        return chargerCurrent = batry.chrgr, batteryCapacity = batry.capct;
    }

*/
/**/
float Battery::accuVolts() {
    return accurateVoltage;
}

uint32_t Battery::determineBatterySeries(uint32_t measuredVoltage_mV) {
    const uint32_t voltagePerCell_mV = 4200; // 4.2V on 4200 millivolttia
    const uint32_t toleranceMultiplier = 5;  // 5% marginaali
    const uint32_t toleranceDivisor = 100;   // Jaetaan sadalla 5% marginaalin saamiseksi

    // Lasketaan alustava sarja
    uint32_t series = measuredVoltage_mV / voltagePerCell_mV;
    uint32_t measuredSeries = series;

    // Lasketaan jäännös
    uint32_t remainder_mV = measuredVoltage_mV % voltagePerCell_mV; // Tämä on "ylijäämä" millivolteissa

    // Tarkistetaan, ylittääkö jäännös 5% kennojännitteestä
    if (remainder_mV * toleranceDivisor > voltagePerCell_mV * toleranceMultiplier) {
        return measuredSeries + 1; // Siirrytään seuraavaan sarjaan
    } else {
        return measuredSeries; // Pysytään samassa sarjassa
    }
}

float Battery::getCurrentVoltage() {
    return batry.milliVoltage / 1000;
}

int Battery::getBatteryApprxSize() {

    return batry.sizeApprx;
}

void Battery::charger(bool state) {
    if (state) {
        digitalWrite(chargerPin, HIGH);
    } else {
        digitalWrite(chargerPin, LOW);
    }
}

void Battery::saveHostname(const char* hostname) {
    preferences.begin("batry", false);
    preferences.putString("myname", hostname);
    preferences.end();
    strncpy(batry.myname, hostname, sizeof(batry.myname) - 1);
    batry.myname[sizeof(batry.myname) - 1] = '\0'; // Ensure null-termination
}

void Battery::loadHostname() {
    preferences.begin("batry", true);
    String hostname = preferences.getString("myname", "helper");
    preferences.end();
    strncpy(batry.myname, hostname.c_str(), sizeof(batry.myname) - 1);
    batry.myname[sizeof(batry.myname) - 1] = '\0'; // Ensure null-termination
}

void Battery::updateHostname(const char* newHostname) {
    saveHostname(newHostname);
    // Additional code to apply the new hostname if needed
}