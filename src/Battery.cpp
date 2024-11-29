#include "Battery.h"
#include <esp_adc_cal.h>
#include <Preferences.h>
#include <OneWire.h>
#include <ESPUI.h>
#include <sTune.h>
#include <PubSubClient.h>
#define VERSION_1



/*

This is the first version of the battery control system. Save this for later debugging, and testing. 
So one can define which was the cause if something goes wrong. 

-- Kallen testiversio: Huomita. Mosfetin gatelle asennettin zener diodit suojaamaan. PTC rele unohtui, mutta ei toistaiseksi vielä lämmitystä tarkoitusta
    rakentaa hänellä.

    Kallen tallennus



    TODO:

    PID: P value save
    Wifi Access Point mode on after certain timeout of no wifi availability
    save hostname to preferences and to as mdns hostname.
    save wifi credentials to preferences
    save mqtt credentials to preferences
    save ADC voltage multiplier to preferences
    random reboots -> oscillation.







*/

#define LOG_BUFFER 50

batterys Battery::batry;

#define LOGS_SIZE 10

struct logEntry {
    int voltageState;
    int tempState;
    int previousVState;
    int previousTState;
    uint32_t timestamps;
};

struct statesLog {
    logEntry entries[LOGS_SIZE];
    int index;
};

statesLog batteryLog;

Battery::Battery(   int tempSensor, 
                    int voltagePin, 
                    int heaterPin, 
                    int chargerPin, 
                    int greenLed, 
                    int yellowLed, 
                    int redLed) :
        oneWire(tempSensor),
        dallas(&oneWire),
        voltagePin(voltagePin),
        heaterPin(heaterPin),
        chargerPin(chargerPin),
        heaterPID(&pidInput, &pidOutput, &pidSetpoint, kp, ki, kd, QuickPID::Action::direct),
        red(redLed),
        yellow(yellowLed),
        green(greenLed),
        tuner(&pidInput, &pidOutput, sTune::TuningMethod::ZN_PID, sTune::Action::directIP, sTune::SerialMode::printOFF)
                {
                    heaterPID = QuickPID(&pidInput, &pidOutput, &pidSetpoint, kp, ki, kd, QuickPID::Action::direct);
                    heaterPID.SetTunings(kp, ki, kd); //apply PID gains
                    heaterPID.SetOutputLimits(0, 255);
                    heaterPID.SetSampleTimeUs(1000 * 1000); // 1 second.
                    heaterPID.SetMode(QuickPID::Control::manual);   

                    pinMode(CHARGER_PIN, OUTPUT);
                    digitalWrite(CHARGER_PIN, LOW);
                    dallas.begin();

                    adc1_config_width(ADC_WIDTH_12Bit);
                    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
                    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH_BIT_12, V_REF, &characteristics);

                    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
                    ledcAttachPin(heaterPin, PWM_CHANNEL);

                    loadSettings();
                    controlHeaterPWM(0);

                }

void Battery::begin()
{
    pinMode(CHARGER_PIN, OUTPUT);
    digitalWrite(CHARGER_PIN, LOW);
    dallas.begin();

    adc1_config_width(ADC_WIDTH_12Bit);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH_BIT_12, V_REF, &characteristics);

    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(heaterPin, PWM_CHANNEL);
    controlHeaterPWM(0);

}

void Battery::setup() {

    pinMode(heaterPin, OUTPUT);
    tuner.Configure(inputSpan, outputSpan, outputStart, outputStep, testTimeSec, settleTimeSec, samples);
    tuner.SetEmergencyStop(tempLimit);

    loadSettings();

    green.start();
    green.setDelay(1000);

    yellow.start();
    yellow.setDelay(1000);

    red.start();
    red.setDelay(1000, 2000);





}

void Battery::loop() {

    red.blink();
    green.blink();   
    yellow.blink();  

    handleBatteryControl();         // Logic for battery control

    readVoltage(5);                 // Voltage reading
    
    updateHeaterPID();              // temperature control

    readTemperature();              // Temperature reading

}
/*


    Control the PWM with updateHeaterPID function
*/
void Battery::controlHeaterPWM(uint8_t dutyCycle) {

    ledcWrite(PWM_CHANNEL, dutyCycle);
}
/*



    the QuickPID Wrapping Function, executed every second. 
*/
void Battery::updateHeaterPID() {

    if(firstRun) {

        pidSetpoint = pidSetpoint;
        pidInput = batry.temperature;
    
        // float optimusPrime = 
        tuner.softPwm(heaterPin, pidInput, pidOutput, pidSetpoint, outputSpan, 1);
        // Serial.print("Optimus Prime: ");
        // Serial.println(optimusPrime);

            switch (tuner.Run()) {
                case tuner.sample: // active once per sample during test
                  this->pidInput = batry.temperature;
                  tuner.plotter(pidInput, pidOutput, pidSetpoint, 1, 60);
                  break;

                case tuner.tunings: // active just once when sTune is done
                  tuner.GetAutoTunings(&kp, &ki, &kd); // sketch variables updated by sTune
                  heaterPID.SetOutputLimits(0, outputSpan);
                  heaterPID.SetSampleTimeUs((outputSpan - 1 ) * 1000);
                  heaterPID.SetMode(QuickPID::Control::automatic); // the PID is turned on
                  heaterPID.SetProportionalMode(QuickPID::pMode::pOnMeas);
                  heaterPID.SetAntiWindupMode(QuickPID::iAwMode::iAwClamp);
                  heaterPID.SetTunings(kp, ki, kd); // update PID with the new tunings
                  Serial.print("Tunings: ");
                    Serial.print(kp);
                    Serial.print(" ");
                    Serial.print(ki);
                    Serial.print(" ");
                    Serial.println(kd);
                  break;

                case tuner.runPid: // active once per sample after tunings
                  this->pidInput = batry.temperature;
                  heaterPID.Compute();
                  tuner.plotter(pidInput, pidOutput, pidSetpoint, 1.0f, 60);
                  tuner.GetAutoTunings(&kp, &ki, &kd); // sketch variables updated by sTune
                  break;
              }
    }

    else if (( millis() - heaterTimer >= 1000)) {
        heaterTimer = millis();

        Battery::adjustHeaterSettings();

        this->pidInput = batry.temperature; // Update the input with the current temperature
        this->pidSetpoint = batry.wantedTemp; // Update the setpoint with the wanted temperature

        Serial.print("PID: ");
        Serial.print("O: ");
        Serial.print(pidOutput);
        Serial.print(" ");
        Serial.print(batry.maxPower);  
        Serial.print(" ");
        Serial.print("mode: ");
        Serial.print(heaterPID.GetMode());
        Serial.print(" ");
        Serial.print("Kp: ");
        Serial.print(heaterPID.GetKp());
        Serial.print(" ");
        Serial.print("Ki: ");
        Serial.print(heaterPID.GetKi());
        Serial.print(" ");
        Serial.print("Kd: ");
        Serial.print(heaterPID.GetKd());
        Serial.print(" ");

        Serial.print(" PWM: ");
        Serial.print(ledcReadFreq(PWM_CHANNEL));
        Serial.print(" ");
        Serial.print(ledcRead(PWM_CHANNEL));
        

        uint8_t powerOutput = pidOutput;
        Serial.println(powerOutput);

        heaterPID.Compute();
        controlHeaterPWM(powerOutput); // Control the heater with the PID output
    }
}
/*
        Dallas temp sensor function

        @bried: Reads the temperature from the Dallas sensor
        @return: float
*/
bool Battery::readTemperature() {

    if (millis() - dallasTime  >= 2500) {
        dallasTime = millis();
        dallas.requestTemperatures();
        float temperature = dallas.getTempCByIndex(0);
        if (temperature == DEVICE_DISCONNECTED_C) {
            Serial.println(" Error: Could not read temperature data ");
            batry.temperature = 127;
            return false;
        } 
        else {
            batry.temperature = temperature;
            return true;
        }
    return false;
    }
return false;

}
/*


        Logging function for the battery control & quickpanel.
*/

void Battery::handleBatteryControl() {
    uint32_t currentMillis = millis(); 
    
    if (currentMillis - lastVoltageStateTime >= 1000) {
        batry.previousVState = batry.vState;
        batry.previousTState = batry.tState;

        TempState tState;
        VoltageState vState;

/*  enum VoltageState {
        LAST_RESORT             =   0,
        LOW_VOLTAGE_WARNING     =   1,  
        PROTECTED               =   2,
        ECONOMY                 =   3,
        BOOST                   =   4,  
        VBOOST_RESET            =   5   };

    enum TempState {
        SUBZERO                 =  0,
        ECO_TEMP                =  1,
        BOOST_TEMP              =  2,
        OVER_TEMP               =  3,
        TEMP_WARNING            =  4,
        TBOOST_RESET            =  5,
        DEFAULT_TEMP            =  6 }; */


        // First Primary control of the battery charging and heating's state selection. To simplify the logic.
        // 1. we cant charge if the battery is too hot or too cold
        // if we were to have favorable conditions for charging, then we can charge according to the voltage level.
        // if not, then we can't charge, and we might need to heat or cool the battery first.

        if(batry.temperature > float(0) && batry.temperature < float(40)) { 

            vState = getVoltageState(batry.voltageInPrecent, batry.ecoVoltPrecent, batry.boostVoltPrecent);  
        }
        else {   

            charger(false);
            tState = getTempState(batry.temperature, batry.ecoTemp, batry.boostTemp);         
        }

    switch (vState) {
            case LAST_RESORT:

                    tState = getTempState(batry.temperature, 2, 3);

                    if(tState != SUBZERO && tState != TEMP_WARNING) 
                        { 
                            charger(true); 
                            red.setDelay(1000, 2000);
                            green.setDelay(1000, 0);
                            yellow.setDelay(1000, 0);
                        }
                    else 
                        { 
                            charger(false);
                            red.setDelay(0, 5000);
                        }
                break;
            case LOW_VOLTAGE_WARNING:

                    tState = getTempState(batry.temperature, batry.ecoTemp, batry.ecoTemp);

                    if(tState != SUBZERO && tState != TEMP_WARNING) 
                        { 
                            charger(true); 
                            red.setDelay(1000);
                            green.setDelay(1000, 0);
                            yellow.setDelay(1000);
                        }
                    else 
                        { 
                            charger(false); 
                            red.setDelay(0, 2500);
                        }

                break;
            case PROTECTED:

                    red.setDelay(1000, 0);
                    yellow.setDelay(1000);
                    green.setDelay(1000, 0);

                    tState = getTempState(batry.temperature, batry.ecoTemp, batry.boostTemp);

                    if(tState != SUBZERO && tState != TEMP_WARNING) { charger(true); }

                break;
            case ECONOMY:

                    red.setDelay(1000, 0);
                    green.setDelay(0, 1000);
                    yellow.setDelay(1000);

                    tState = getTempState(batry.temperature, batry.ecoTemp, batry.boostTemp);

                    if(tState != SUBZERO && tState != TEMP_WARNING ) 
                        { charger(true); }

                break;
            case BOOST:

                    red.setDelay(1000, 0);
                    green.setDelay(0, 1000);
                    yellow.setDelay(1000);

                    tState = getTempState(batry.temperature, batry.ecoTemp, batry.boostTemp);

                    if ((tState != SUBZERO && tState != TEMP_WARNING ) && batry.voltBoostActive)   // if not in cold or warning state, and boost is active
                        { charger(true); }                                                         // then charge
                    else 
                        { charger(false); }
                break;
            case VBOOST_RESET:

                    red.setDelay(1000, 0);
                    yellow.setDelay(1000, 0);
                    green.setDelay(0, 1000);

                    tState = getTempState(batry.temperature, batry.ecoTemp, batry.boostTemp);

                    if (!batry.voltBoostActive) 
                        { charger(false); }

                    if(getActivateVoltageBoost()) 
                        { activateVoltageBoost(false); }

                break;
        }


    switch (tState) {
            case SUBZERO:

                    red.setDelay(0, 1000);

                    charger(false);

                 break;
            case ECO_TEMP:

                    if(batry.tempBoostActive) { batry.wantedTemp = batry.boostTemp; }
                    else { batry.wantedTemp = batry.ecoTemp; }

                break;
            case BOOST_TEMP:

                    if(batry.tempBoostActive) { batry.wantedTemp = batry.boostTemp; }
                    else { batry.wantedTemp = batry.ecoTemp; }

                break;
            case OVER_TEMP:

                    batry.wantedTemp = batry.ecoTemp;
                    batry.tempBoostActive = false;

                 break;
            case TEMP_WARNING:

                    charger(false);
                    batry.wantedTemp = 0;
                    // controlHeaterPWM(0);

                break;
            case TBOOST_RESET:

                    batry.wantedTemp = batry.ecoTemp;
                    batry.tempBoostActive = false;

                break;
           case DEFAULT_TEMP:

                    // controlHeaterPWM(0);
                    batry.wantedTemp = 0;

            break;
    }

/*
    Serial.print("Kp: ");
    Serial.print(Battery::kp);
    Serial.print(" ");
    Serial.print("Ki: ");
    Serial.print(Battery::ki);
    Serial.print(" ");
    Serial.print(tState);
    Serial.print(" ");
    Serial.print(vState);
    Serial.print(" in:  ");
    Serial.print(pidInput);
    Serial.print(" Out:  ");
    Serial.print(pidOutput);
    Serial.print(" Set:  ");               
    Serial.print(pidSetpoint);        
    Serial.print(" Vstate: ");
    Serial.print(static_cast<int>(vState));
    Serial.print(", Tstate: ");
    Serial.println(static_cast<int>(tState));
    */

    batry.vState = static_cast<int>(vState);
    batry.tState = static_cast<int>(tState);
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
    if (voltagePrecent >= 10 && voltagePrecent < 20) {
        return LAST_RESORT;                                                             // Lets shut all down
    } else if (voltagePrecent >= 20 && voltagePrecent < 30) {
        return LOW_VOLTAGE_WARNING;                                                     // lets warn the user with leds and even lower P value
    } else if (voltagePrecent >= 30 && voltagePrecent < 50) {
        return PROTECTED;                                                               // lets protect the battery by lowering the P term
    } else if (voltagePrecent >= 50 && voltagePrecent < ecoPrecentVoltage) {
        return ECONOMY;                                                                 // Normal operation
    } else if (voltagePrecent >= ecoPrecentVoltage && voltagePrecent < boostPrecentVoltage) {
        return BOOST;                                                                    // above economy voltage, in the wanted boost area
    } else if (voltagePrecent >= boostPrecentVoltage) {
        return VBOOST_RESET;  // Add boost reset state
    } else {                                                                             // lets reset the boost mode
        return LAST_RESORT;  // Fallback state
    }
}
/*



    State machine for temperature levels, to simplify the logic 
    for the battery control.
*/
Battery::TempState Battery::getTempState(   float temperature, 
                                            int ecoTemp, 
                                            int boostTemp) {                                                
        if (temperature < 0) {
            return SUBZERO;
        } else if (temperature <= ecoTemp) {
            return ECO_TEMP;
        } else if (temperature > ecoTemp && temperature <= boostTemp) {
            return BOOST_TEMP;
        } else if (temperature > boostTemp && temperature < 40) {
            return OVER_TEMP;
        } else if (temperature >= 40) {
            return TEMP_WARNING;
        } else {
            return DEFAULT_TEMP;
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
    preferences.putUChar("size",         constrain(batry.size, 10, 20));
    preferences.putFloat("temperature",  constrain(batry.temperature, float(-50), float(120)));
    preferences.putUChar("currentVolt",  constrain(batry.voltageInPrecent, 1, 100));
    preferences.putUChar("ecoVolt",      constrain(batry.ecoVoltPrecent, 1, 100));
    preferences.putUChar("boostVolt",    constrain(batry.boostVoltPrecent, 1, 100));
    preferences.putUChar("ecoTemp",      constrain(batry.ecoTemp, 1, 255));
    preferences.putUChar("boostTemp",    constrain(batry.boostTemp, 1, 255));
    preferences.putUChar("resistance",   constrain(batry.resistance, 1, 255));
    preferences.putUChar("capct",        constrain(batry.capct, 1, 255));
    preferences.putUChar("chrgr",        constrain(batry.chrgr, 1, 5));
    preferences.putUChar("resistance",   constrain(batry.resistance, 1, 255));
    preferences.putUChar("maxPower",     constrain(batry.maxPower, 1, 255));
    preferences.putString("myname",      String(batry.myname));
    preferences.putUChar("pidP",         constrain(batry.pidP, 1, 100));
    preferences.end(); // Close preferences, commit changes
}

void Battery::loadSettings() {
    preferences.begin("btry", false); // Open preferences with namespace "battery_settings" 
    // Sort batry related components

    batry.myname = preferences.getString("myname", "Helmi");
    Serial.print("Battery name loaded from memory: ");
    Serial.println(batry.myname);

    batry.size = constrain(preferences.getUChar("size", 1), 7, 20);
    Serial.print("Battery size loaded from memory: ");
    Serial.println(batry.size);

    batry.temperature = constrain(preferences.getFloat("temperature", float(2)), float(-50), float(120));
    Serial.print("Battery temperature loaded from memory: ");
    Serial.println(batry.temperature);

    batry.voltageInPrecent = constrain(preferences.getUChar("currentVolt", 1), 5, 100);
    Serial.print("Battery voltage loaded from memory: ");
    Serial.println(batry.voltageInPrecent);

    batry.resistance = constrain(preferences.getUChar("resistance", 1), 2, 255);
    Serial.print("Battery resistanloadSce loaded from memory: ");
    Serial.println(batry.resistance);

    batry.ecoVoltPrecent = constrain(preferences.getUChar("ecoVolt", 1), 5, 255);
    Serial.print("Battery eco voltage loaded from memory: ");
    Serial.println(batry.ecoVoltPrecent);

    batry.boostVoltPrecent = constrain(preferences.getUChar("boostVolt", 1), 5, 255);
    Serial.print("Battery boost voltage loaded from memory: ");
    Serial.println(batry.boostVoltPrecent);

    batry.boostTemp = constrain(preferences.getUChar("boostTemp", 1), 5, 40);
    Serial.print("Battery boost temperature loaded from memory: ");
    Serial.println(batry.boostTemp);

    batry.ecoTemp = constrain(preferences.getUChar("ecoTemp", 1), 5, 30);
    Serial.print("Battery eco temperature loaded from memory: ");
    Serial.println(batry.ecoTemp);

    batry.capct = constrain(preferences.getUChar("capct", 1), 1, 255);
    Serial.print("Battery capacity loaded from memory: ");
    Serial.println(batry.capct);

    batry.chrgr = constrain(preferences.getUChar("chrgr", 0), 1, 5);
    Serial.print("Battery charger loaded from memory: ");
    Serial.println(batry.chrgr);

    batry.resistance = constrain(preferences.getUChar("resistance", 1), 2, 255);
    Serial.print("Battery resistance loaded from memory: ");
    Serial.println(batry.resistance);

    batry.maxPower = constrain(preferences.getUChar("maxPower", 1), 2, 255);
    Serial.print("Battery max power loaded from memory: ");
    Serial.println(batry.maxPower);

    batry.tempBoostActive = preferences.getBool("tboost", false);
    Serial.print("Battery temp boost loaded from memory: ");
    Serial.println(batry.tempBoostActive);

    batry.voltBoostActive = preferences.getBool("vboost", false);
    Serial.print("Battery volt boost loaded from memory: ");
    Serial.println(batry.voltBoostActive);

    batry.pidP = constrain(preferences.getUChar("pidP", 1), 1, 250);
    Serial.print("Battery PID P value loaded from memory: ");
    Serial.println(batry.pidP);

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

        #ifndef VERSION_1  // different hardware setup
        if(!batry.chargerState) {
        gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_NUM_25, HIGH); 
        delay(5); }
        #else
        digitalWrite(CHARGER_PIN, HIGH);
        delay(5);
        #endif
        
        u_int32_t voltVal = adc1_get_raw(ADC_CHANNEL);
        u_int32_t calibratedVoltage = esp_adc_cal_raw_to_voltage(voltVal, &characteristics) * 30.81;

        #ifndef VERSION_1
        if(!batry.chargerState) {
        gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_NUM_25, LOW);   }
        #else
        digitalWrite(CHARGER_PIN, LOW);
        #endif
    
        if (firstRun) {
            // Initialize the moving average array -> otherwise slow voltage increase takes time.
            for (int i = 0; i < MOVING_AVG_SIZE; i++) {
                MOVAreadings[i] = calibratedVoltage;
            }
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
        // Serial.print("VarianceSum: ");
        // Serial.println(varianceSum);

        u_int32_t variance = (varianceSum * 100) / (MOVING_AVG_SIZE * movingAverage);

        // Store variance in the secondary array
        varianceReadings[MOVAIndex] = variance;


        // Check if the moving average is within the valid range
        if (movingAverage > 5000 && movingAverage < 100000) {
            // Update battery size approximation if the new series is larger
            int newSeries = Battery::determineBatterySeries(movingAverage);
            if (batry.sizeApprx < newSeries) {
                batry.sizeApprx = newSeries;
                Serial.print(" BattApproxSize: ");
                Serial.println(batry.sizeApprx);
            }

            // Update voltage and accurate voltage
            batry.milliVoltage = movingAverage;

            // Update battery voltage in percentage
            batry.voltageInPrecent = getVoltageInPercentage(movingAverage);
        } else {
            batry.milliVoltage = 0; // Set the accurate voltage to 0 in case of reading error
            batry.voltageInPrecent = 0; // Set the voltage in percentage to 0 in case of reading error
            Serial.println(" Voltage reading error ");
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


    if (precent > 0 && precent < 101) {
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

    milliVoltage = constrain(milliVoltage, 25000, 100000);
    if (milliVoltage < (minVoltage * batry.size)) {
        return 0;  // Below minimum voltage, 0% usable area
        Serial.println("Below minimum voltage");
    } else if (milliVoltage > (maxVoltage * batry.size)) {
        return 0;  // Above maximum voltage, 100% usable area
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
uint8_t Battery::getEcoPrecentVoltage() {
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
uint8_t Battery::getBoostPrecentVoltage() {
    return batry.boostVoltPrecent;
}
/*


    Battery DS18B20 temp sensor handling. 

    Maybe have some moving avarage sum, where we can get the average temperature over a period of time.
    This is to compare the reading, if it differs from the previous reading, we can have a failsafe.
*/
float Battery::getTemperature() {
    float tempReturn = 0.0f;
    tempReturn = batry.temperature;

    if(tempReturn < float(80) && tempReturn > float(-50)) {
        return tempReturn;
    }
    else return 255;
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

uint8_t Battery::getNominalString() {
    return batry.size;
}
/*


    Setter and Getter for battery boost temperature settings. 
        - ESPUI interface for the user to set the battery temperature settings.
*/
uint8_t Battery::getEcoTemp() {
    return batry.ecoTemp;
}

bool Battery::setEcoTemp(int ecoTemp) {
    if(ecoTemp > 0 && ecoTemp < 31 && ecoTemp < batry.boostTemp) {
        batry.ecoTemp = ecoTemp;
        return true;
    }
    else return false;
}
/*


    SetGetter for Temperature boost settings. 
        - ESPUI interface for the user to set the battery temperature settings.
*/
uint8_t Battery::getBoostTemp() {
    return batry.boostTemp;
}

bool Battery::setBoostTemp(int boostTemp) {
    if(boostTemp > 9 && boostTemp < 41 && boostTemp > batry.ecoTemp) {
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
    if(tempBoostActive) {
        batry.tempBoostActive = true;
        heaterPID.SetTunings(float(batry.pidP + 20), ai, ad);
        preferences.begin("btry", false); // Open preferences with namespace "battery_settings" 
        preferences.putBool("tboost", true);
        preferences.end(); // Close preferences
        return true;
    }
    else if(!tempBoostActive ) {
        batry.tempBoostActive = false;
        heaterPID.SetTunings(float(batry.pidP), ki, kd);
        preferences.begin("btry", false); // Open preferences with namespace "battery_settings"
        preferences.putBool("tboost", false);
        preferences.end(); // Close preferences
        return true;
    }
    else 
        return false;
}

bool Battery::getActivateTemperatureBoost() {
    if (batry.tempBoostActive)
        return true;
    else
        return false;
}

bool Battery::activateVoltageBoost(bool voltBoostActive) {
    if(voltBoostActive) {
        batry.voltBoostActive = true;
        firstRun = false;
        preferences.begin("btry", false); // Open preferences with namespace "battery_settings"
        preferences.putBool("vboost", true);
        preferences.end(); // Close preferences
        return true;
    }
    else if (!voltBoostActive) {
        batry.voltBoostActive = false;
        preferences.begin("btry", false); // Open preferences with namespace "battery_settings"
        preferences.putBool("vboost", false);
        preferences.end(); // Close preferences
        charger(false);
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
    if(chargerCurrent > 0 && chargerCurrent < 11) {
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
    float volts = 0.0f;
    volts = batry.milliVoltage / (float)(1000.0F);
    return volts;
}

int Battery::getBatteryApprxSize() {
    return batry.sizeApprx;
}

void Battery::charger(bool state) {
    if (state) {
        gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_NUM_25, HIGH);
        // Serial.print (" Chrgr: ON ");
        batry.chargerState = true;

    } else {

        gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_NUM_25, LOW);
        /// Serial.print(" Chrgr: OFF ");
        batry.chargerState = false;
    }
}

bool Battery::saveHostname(String hostname) {
    preferences.begin("batry", false);
    preferences.putString("myname", hostname);
    preferences.end();
    batry.myname = String(hostname);
    return true;
}

bool Battery::loadHostname() {
    preferences.begin("batry", true);
    String hostname = preferences.getString("myname", "Helmi");
    preferences.end();
    batry.myname = hostname;
    return true;
}

bool Battery::setResistance(uint8_t resistance) {
    if (resistance > 10 && resistance < 255) {
        batry.resistance = resistance;
        return true;
    } else {
        return false;
    }
}

int Battery::getResistance() {
    return batry.resistance;
}

void Battery::adjustHeaterSettings() {
    const float maxPower = 30.0; // Maximum power in watts

    // Calculate the maximum allowable power based on the resistance and voltage
    float power = (batry.size * float(3.7) * batry.size * float(3.7)  ) / batry.resistance;

    // Calculate the duty cycle needed to cap the power at maxPower
    float dutyCycle = maxPower / power;

    if (dutyCycle > 1) {
        dutyCycle = 1;
    }

    // Calculate the output limits for QuickPID
    batry.maxPower  = (dutyCycle * 255);

    // Ensure the output limit does not exceed the maximum allowable current
    // Ensure the output limit does not exceed the maximum allowable current
    if (batry.maxPower > 255) {
        batry.maxPower = 254;
    }
    if (batry.maxPower < 20) {
        batry.maxPower = 20;
    }

    // Adjust the QuickPID output limits
    heaterPID.SetOutputLimits(float(0), float(batry.maxPower));
}

bool Battery::setPidP(int p) {
    if (p >= 0 && p < 250) {
        batry.pidP = p;
        preferences.begin("batry", false);
        preferences.putInt("pidP", constrain(p, 0, 250));
        preferences.end();
        // heaterPID.SetTunings(float(batry.pidP), float(0.05), 0);
        // heaterPID.SetProportionalMode(float(batry.pidP));      
        return true;
    } else {
        return false;
    }
}

int Battery::getPidP() {
    return batry.pidP;
}

bool Battery::getChargerStatus() {
    return batry.chargerState;
}