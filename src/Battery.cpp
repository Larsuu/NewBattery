#include "Battery.h"
#include <esp_adc_cal.h>
#include <Preferences.h>
#include <OneWire.h>
#include <ESPUI.h>
#include <sTune.h>
#include <WiFiClientSecure.h>
#include <cctype>
#include <functional>
#define DEBUG
#define LOG_BUFFER 50
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

Battery::~Battery() {
    digitalWrite(heaterPin, LOW);    // Turn off heater
    digitalWrite(chargerPin, LOW);   // Turn off charger
    // Save settings before destruction
    saveSettings(ALL);
    // Stop PWM
    ledcDetachPin(heaterPin);
    // Turn off LEDs
    //red.stop();
    yellow.stop();
    green.stop();
}

void Battery::setup() {

        pinMode(chargerPin, OUTPUT);
        pinMode(heaterPin, OUTPUT);
        pinMode(redLed, OUTPUT);
        pinMode(greenLed, OUTPUT);
        pinMode(yellowLed, OUTPUT);

        digitalWrite(heaterPin, LOW);
        digitalWrite(chargerPin, LOW);
        digitalWrite(redLed, HIGH);
        digitalWrite(greenLed, HIGH);
        digitalWrite(yellowLed, LOW);

        dallas.begin();

        tuner.Configure(battery.stune.inputSpan, battery.stune.outputSpan, battery.stune.outputStart, battery.stune.outputStep, battery.stune.testTimeSec, battery.stune.settleTimeSec, battery.stune.samples);
        tuner.SetEmergencyStop(battery.stune.tempLimit);

        green.start();
        green.setDelay(1000, 0);

        yellow.start();
        yellow.setDelay(1000, 0);

        // red.start();
        // red.setDelay(1000, 2000);

    loadSettings(ALL);
}

void Battery::loop() {



    readTemperature();              // Temperature reading

    readVoltage(5000);                 // Voltage reading

    handleBatteryControl();         // Logic for battery control

    handleMqtt();  

    green.blink(); 
    yellow.blink();  
    // red.blink();

    if(init())
    {
        updateHeaterPID();              // PID calibration -> runtune()

        controlHeaterPWM();

    }
}  // end loop


bool Battery::init() {

    if(battery.init) {
        return true;
    }
    else {
        batteryInit();
        return false;
    }
}

void Battery::batteryInit() {

        if(battery.size > 5 && battery.size < 21) {
            if(battery.heater.resistance > 20 && battery.heater.resistance < 255) {
                if (abs(battery.sizeApprx - battery.size) < 3) {
                    battery.init = true;
                    adjustHeaterSettings();
                    #ifdef DEBUG
                        Serial.println("Battery init success");

                    #endif
                }
            }
        }
}

/*
    the QuickPID Wrapping Function, executed every second. 
*/
void Battery::updateHeaterPID() {

    if(battery.stune.enable) {
                runTune();
    }
}


void Battery::runTune() {
    if (!battery.stune.done || battery.stune.error) {

        // boostButtonPress_Time + lenght_stuneRunTime is greater than current time
        if(battery.stune.startTime + (battery.stune.lenTime * 1000)  > millis()) {
       
        auto& stune = battery.stune; 
        auto& heater = battery.heater;

        if(!stune.firstRun) {
        heaterPID.SetMode(QuickPID::Control::manual);
        stune.firstRun = true;
       }

        tuner.softPwm(heaterPin, stune.pidInput, stune.pidOutput, stune.pidInput, stune.outputSpan, 0);
            switch (tuner.Run()) {
                case tuner.sample: // Active once per sample during test
                    tuner.plotter(stune.pidInput, stune.pidOutput, stune.pidSetpoint, 0.1f, 10); // Plotting
                    break;


                case tuner.tunings: // Active just once when sTune is done
                    tuner.GetAutoTunings(&stune.pidP, &stune.pidI, &stune.pidD); // Get tuned values
                    if ((tuner.GetTau() / tuner.GetDeadTime()) > 0.5) {
                        // Good tunability - complete the tuning process
                        stune.done = true;
                        stune.error = false;

                        heaterPID.SetOutputLimits(0, stune.outputSpan * 0.1);
                        heaterPID.SetSampleTimeUs((stune.outputSpan - 1) * 1000);
                        heaterPID.SetMode(QuickPID::Control::automatic);
                        heaterPID.SetProportionalMode(QuickPID::pMode::pOnMeas);
                        heaterPID.SetAntiWindupMode(QuickPID::iAwMode::iAwClamp);

                        adjustHeaterSettings();

                        if (stune.pidP >= 0.1 && stune.pidP < 100) {
                            if (stune.pidI >= 0.0 && stune.pidI < 10) {
                                if (stune.pidD >= 0.0 && stune.pidD < 10) {
                                    heater.pidP = stune.pidP;
                                    heater.pidI = stune.pidI;
                                    heater.pidD = stune.pidD;
                                }  
                            }
                        }

                        heaterPID.SetTunings(battery.heater.pidP, battery.heater.pidI, battery.heater.pidD);
                        heaterPID.Initialize();
                        battery.stune.enable = false;
                        battery.heater.enable = true;
                        saveSettings(PID);
                    }
                    else { 
                        stune.error = true;
                        for (stune.runTimes = 0; stune.runTimes < 5; stune.runTimes++) {
                            Serial.print("Tuning: ");
                            Serial.println(stune.runTimes + 1);

                            stune.outputStep += constrain((255 / 5), 25, 255); // Ensure it stays within bounds
                            stune.testTimeSec += constrain((300 / 5), 30, 360); // Ensure it stays within bounds
                            stune.samples = stune.testTimeSec;
                              
                            // Reconfigure tuner with new parameters
                            tuner.Configure(
                                battery.stune.inputSpan,
                                battery.stune.outputSpan,
                                battery.stune.outputStart,
                                battery.stune.outputStep,
                                battery.stune.testTimeSec,
                                battery.stune.settleTimeSec,
                                battery.stune.samples );

                                break; // Exit the loop if tuning is successful
                            }                        
                        }
                    

                    #ifdef DEBUG
                    Serial.println("Tunings");
                    Serial.print("tunability: ");
                    Serial.println(tuner.GetTau() / tuner.GetDeadTime());
                    Serial.print("heater pidP: ");
                    Serial.println(heater.pidP);
                    Serial.print("heater pidI: ");
                    Serial.println(heater.pidI);
                    Serial.print("heater pidD: ");
                    Serial.println(heater.pidD);

                    Serial.print("stune pidP: ");
                    Serial.println(stune.pidP);
                    Serial.print("stune pidI: ");
                    Serial.println(stune.pidI);
                    Serial.print("stune pidD: ");
                    Serial.println(stune.pidD);
                    #endif

                    break;

                case tuner.runPid:
                    Serial.println("RunPid ready");
                    break;
            }     
        } 
    }
}
/*
    Control the PWM with updateHeaterPID function
*/
void Battery::controlHeaterPWM() {
    if(heaterPID.Compute()) {
       if(battery.stune.done && battery.heater.enable) {
        ledcWrite(PWM_CHANNEL, static_cast<uint32_t>(battery.heater.pidOutput));
       }
       else {
        ledcWrite(PWM_CHANNEL, 0);
       }
    }
}
/*
        Dallas temp sensor function
        @bried: Reads the temperature from the Dallas sensor
        @return: float
*/
void Battery::readTemperature() {

    if (millis() - dallasTime  >= 1000) {
        dallasTime = millis();
        dallas.requestTemperatures();
        float temperature = dallas.getTempCByIndex(0);
        // Serial.print("Dallas: Temperature: ");
        // Serial.println(temperature);
        if (temperature == DEVICE_DISCONNECTED_C) {
            Serial.println(" Error: Could not read temperature data ");
            battery.temperature = 127;
        } 
        else {
            battery.temperature = temperature;
            battery.stune.pidInput = temperature;
            battery.heater.pidInput = temperature;

        }
    }
}

void Battery::handleBatteryControl() {
    uint32_t currentMillis = millis(); 
    
    if (currentMillis - lastVoltageStateTime >= 2500) {
        battery.prevVState = battery.vState;    // copying the previous state to the previous state
        battery.prevTState = battery.tState;    // copying the previous state to the previous state

        if(battery.temperature > 0 && battery.temperature < 40) { 
            battery.vState = getVoltageState(battery.voltageInPrecent);  
        }
        else {   
            charger(false);
            battery.tState = getTempState(battery.temperature);         
        }

    switch (battery.vState) {
            case ALERT:
                    battery.tState = getTempState(battery.temperature);

                    if(battery.tState != SUBZERO && battery.tState != TEMP_WARNING) 
                        {
                            charger(true); 
                            // red.setDelay(1000, 1000);
                            yellow.setDelay(1000, 0);
                            green.setDelay(1000, 0);
                        }
                    else 
                        { 
                            charger(false);
                            // red.setDelay(0, 1000);
                        }
                break;
            case WARNING:
                    battery.tState = getTempState(battery.temperature);

                    if(battery.tState != SUBZERO && battery.tState != TEMP_WARNING) 
                        { 
                            charger(true); 
                            //red.setDelay(1000);
                            yellow.setDelay(1000, 1000);
                            green.setDelay(1000, 0);
                            
                        }
                    else 
                        { 
                            charger(false); 
                            //red.setDelay(0, 1000);
                        }
                break;
            case LOVV:
                    // red.setDelay(1000, 0);
                    yellow.setDelay(1000, 1000);
                    green.setDelay(1000, 1000);

                    battery.tState = getTempState(battery.temperature);

                    if(battery.tState != SUBZERO && battery.tState != TEMP_WARNING) { charger(true); }

                break;
            case ECO:
                    // red.setDelay(1000, 0);
                    yellow.setDelay(2000, 0);
                    green.setDelay(0, 1000);
                    
                    battery.tState = getTempState(battery.temperature);

                    if(battery.tState != SUBZERO && battery.tState != TEMP_WARNING ) 
                        { charger(true); }

                break;
            case BOOST:
                    //red.setDelay(1000, 0);
                    yellow.setDelay(2000, 0);
                    green.setDelay(1000, 1000);

                    battery.tState = getTempState(battery.temperature);

                    if ((battery.tState != SUBZERO && battery.tState != TEMP_WARNING ) && battery.voltBoost)   // if not in cold or warning state, and boost is active
                        { charger(true); }                                                         // then charge
                    else 
                        { charger(false); }
                break;
            case FULL:
                    // red.setDelay(1000, 0);
                    yellow.setDelay(2000, 0);
                    green.setDelay(500, 500);

                    battery.tState = getTempState(battery.temperature);

                    if(getActivateVoltageBoost()) 
                        { activateVoltageBoost(false); }

                break;
            default:
                Serial.println("unknown type in the state machine");
                digitalWrite(redLed, LOW);
                charger(false);
            break;
        }

    switch (battery.tState) {
            case SUBZERO:
                    digitalWrite(redLed, LOW);
                    charger(false);

                 break;
            case COLD:
                    digitalWrite(redLed, HIGH);
                    if(battery.tempBoost) { battery.heater.pidSetpoint = battery.heater.ecoTemp; }
                    else { battery.heater.pidSetpoint = battery.heater.ecoTemp; }
                break;
            case ECO_TEMP:
                    digitalWrite(redLed, HIGH);
                    if(battery.tempBoost) { battery.heater.pidSetpoint = battery.heater.boostTemp; }
                    else { battery.heater.pidSetpoint = battery.heater.ecoTemp; }

                break;
            case ECO_READY:
                    digitalWrite(redLed, HIGH);
                    if(battery.tempBoost) { battery.heater.pidSetpoint = battery.heater.boostTemp; }
                    else { battery.heater.pidSetpoint = battery.heater.ecoTemp; }
                break;
            case BOOST_TEMP:
                    digitalWrite(redLed, HIGH);
                    if(battery.tempBoost) { battery.heater.pidSetpoint = battery.heater.boostTemp; }
                    else { battery.heater.pidSetpoint = battery.heater.ecoTemp; }

                break;
            case BOOST_READY:
                    digitalWrite(redLed, HIGH);
                    if(battery.tempBoost) { battery.heater.pidSetpoint = battery.heater.boostTemp; }
                    else { battery.heater.pidSetpoint = battery.heater.ecoTemp; }
                    activateTemperatureBoost(false);
                break;
            case TEMP_WARNING:
                    digitalWrite(redLed, LOW);
                    charger(false);
                    battery.heater.pidSetpoint = 2;

                    if( battery.heater.boostTemp - battery.temperature > 5) {
                        battery.stune.error = true;
                    }
                break;
            case UNKNOWN_TEMP:
                digitalWrite(redLed, LOW);
                charger(false);
                battery.stune.error = true;
                battery.heater.pidSetpoint = 2;
                heaterPID.SetMode(QuickPID::Control::manual);
                break;
            default:
                Serial.println("unknown type in the temp state");
                digitalWrite(redLed, LOW);
                break;
    }
 

    #ifdef DEBUG
    Serial.print("Temp: ");    
    Serial.print(" Vstate: ");
    Serial.print(static_cast<int>(battery.vState));
    Serial.print(", Tstate: ");
    Serial.println(static_cast<int>(battery.tState)); 
    Serial.print("Power Limit: "); Serial.println(battery.heater.powerLimit);

    Serial.print("Oput: "); 
    Serial.print(battery.heater.pidOutput);
    Serial.print(" ");
    Serial.println(battery.heater.pidSetpoint);

    #endif
    battery.vState = battery.vState;
    battery.tState = battery.tState;
    lastVoltageStateTime = currentMillis;
    }
}
/*
    State Machine for battery voltage levels and temperature levels
        Todo: add Boost reset state?
        @brief: Simplifies the logic for the battery control
        @return: VoltageState enum
*/
VoltageState Battery::getVoltageState(int voltagePrecent) {
    if (voltagePrecent < 10) {
        return ALERT;
    } else if (voltagePrecent >= 10 && voltagePrecent < 20) {
        return ALERT;
    } else if (voltagePrecent >= 20 && voltagePrecent < 30) {
        return WARNING;
    } else if (voltagePrecent >= 30 && voltagePrecent < 50) {
        return LOVV;
    } else if (voltagePrecent >= 50 && voltagePrecent < battery.ecoVoltPrecent) {
        return ECO;
    } else if (voltagePrecent >= battery.ecoVoltPrecent && voltagePrecent < battery.boostVoltPrecent) {
        return BOOST;
    } else if (voltagePrecent >= battery.boostVoltPrecent && voltagePrecent > 99) {
        return FULL;
    } else {
        return ALERT;
    }
}
/*
    State machine for temperature levels, to simplify the logic 
    for the battery control.
*/
TempState Battery::getTempState(float temperature) {                                                
        if (temperature < 0) {
            return SUBZERO;
        } else if (temperature >= 0 && temperature < 10) {
            return COLD;
        } else if (temperature >= 10 && temperature <= battery.heater.ecoTemp) {
            return ECO_TEMP;
        } else if (temperature >= battery.heater.ecoTemp && temperature <= (battery.heater.ecoTemp + 1) ) {
            return ECO_READY;
        } else if (temperature > (battery.heater.ecoTemp + 1) && temperature <= battery.heater.boostTemp) {
            return BOOST_TEMP;
        } else if (temperature > battery.heater.boostTemp && temperature < 40) {
            return BOOST_READY;
        } else if (temperature >= 40) {
            return TEMP_WARNING;
        } else {
            return UNKNOWN_TEMP;
        }
}
 
void Battery::saveSettings(SettingsType type) {
    preferences.begin("btry", false); 


    
    switch (type) {
        case SETUP:
            // Direct access to battery members
            preferences.putString("myname", String(battery.name));
            preferences.putUChar("size", constrain(battery.size, 0, 21));

            preferences.putUChar("chrgr",        constrain(battery.chrgr.current, 0, 15));     
            preferences.putUChar("capct",        constrain(battery.capct, 0, 255));       
            preferences.putFloat("temperature",  constrain(battery.temperature, float(-40), float(40)));
            preferences.putUChar("currentVolt",  constrain(battery.voltageInPrecent, 1, 100));
            preferences.putUChar("ecoVolt",      constrain(battery.ecoVoltPrecent, 1, 100));
            preferences.putUChar("boostVolt",    constrain(battery.boostVoltPrecent, 1, 100));

            preferences.putUChar("resistance",   constrain(battery.heater.resistance, 0, 255));
            preferences.putUChar("boostTemp",    constrain(battery.heater.boostTemp, 1, 40));
            preferences.putUChar("ecoTemp",      constrain(battery.heater.ecoTemp, 1, 30));
            preferences.putUChar("maxPower",     constrain(battery.heater.powerLimit, 1, 255));

            preferences.putBool("tboost",        constrain(battery.tempBoost, 0, 1));
            preferences.putBool("vboost",        constrain(battery.voltBoost, 0, 1));

#ifdef DEBUG
            // Print saved settings for debugging
            Serial.println("Saved Settings (SETUP):");
            Serial.print("Name: "); Serial.println(battery.name);
            Serial.print("Size: "); Serial.println(battery.size);
            Serial.print("Resistance: "); Serial.println(battery.heater.resistance);
            Serial.print("Charger Current: "); Serial.println(battery.chrgr.current);
            Serial.print("Capacity: "); Serial.println(battery.capct);
            Serial.print("Temperature: "); Serial.println(battery.temperature);
            Serial.print("Current Voltage: "); Serial.println(battery.voltageInPrecent);
            Serial.print("Eco Voltage Percent: "); Serial.println(battery.ecoVoltPrecent);
            Serial.print("Boost Voltage Percent: "); Serial.println(battery.boostVoltPrecent);
            Serial.print("Boost Temperature: "); Serial.println(battery.heater.boostTemp);
            Serial.print("Eco Temperature: "); Serial.println(battery.heater.ecoTemp);
            Serial.print("Max Power: "); Serial.println(battery.heater.maxPower);
            Serial.print("Power Limit: "); Serial.println(battery.heater.powerLimit);
            Serial.print("Temperature Boost: "); Serial.println(battery.tempBoost);
            Serial.print("Voltage Boost: "); Serial.println(battery.voltBoost);
#endif
            break;        
        case WIFI:
            preferences.putString("wssid", battery.wlan.ssid);  // Save the actual stored SSID
            preferences.putString("wpass", battery.wlan.pass);  // Save the actual stored password

#ifdef DEBUG
            // Print saved WiFi settings for debugging
            Serial.println("Saved Settings (WIFI):");
            Serial.print("SSID: "); Serial.println(battery.wlan.ssid);
            Serial.print("Password: "); Serial.println(battery.wlan.pass);
#endif
            break;

        case HTTP:
            preferences.putBool("httpen", battery.http.enable);
            preferences.putString("httpusr", battery.http.username);
            preferences.putString("httppass", battery.http.password);

#ifdef DEBUG
            // Print saved HTTP settings for debugging
            Serial.println("Saved Settings (HTTP):");
            Serial.print("HTTP Enabled: "); Serial.println(battery.http.enable);
            Serial.print("Username: "); Serial.println(battery.http.username);
            Serial.print("Password: "); Serial.println(battery.http.password);
#endif
            break;

        case MQTT:
            preferences.putBool("mqtten", battery.mqtt.enable);
            preferences.putString("mqttip", battery.mqtt.server);
            preferences.putUInt("mqttport", battery.mqtt.port);
            preferences.putString("mqttusr", battery.mqtt.username);
            preferences.putString("mqttpass", battery.mqtt.password);

#ifdef DEBUG
            // Print saved MQTT settings for debugging
            Serial.println("Saved Settings (MQTT):");
            Serial.print("MQTT Enabled: "); Serial.println(battery.mqtt.enable);
            Serial.print("Server: "); Serial.println(battery.mqtt.server);
            Serial.print("Port: "); Serial.println(battery.mqtt.port);
            Serial.print("Username: "); Serial.println(battery.mqtt.username);
            Serial.print("Password: "); Serial.println(battery.mqtt.password);
#endif
            break;

        case TELEGRAM:
            preferences.putBool("tgen", battery.telegram.enable);
            preferences.putString("tgtoken", battery.telegram.token); // Load Telegram token
            preferences.putInt("tgusr", battery.telegram.chatId); // Load Telegram chat ID

#ifdef DEBUG
            // Print saved Telegram settings for debugging
            Serial.println("Saved Settings (TELEGRAM):");
            Serial.print("Telegram Enabled: "); Serial.println(battery.telegram.enable);
            Serial.print("Token: "); Serial.println(battery.telegram.token);
            Serial.print("Chat ID: "); Serial.println(battery.telegram.chatId);
#endif
            break;

        case PID:
            preferences.putFloat("pidP", battery.heater.pidP);
            preferences.putFloat("pidI", battery.heater.pidI);
            preferences.putFloat("pidD", battery.heater.pidD);
            preferences.putBool("tuneOk", battery.stune.done);
            preferences.putBool("heatOn", battery.heater.enable);
#ifdef DEBUG
            // Print saved PID settings for debugging
            Serial.println("Saved Settings (PID):");
            Serial.print("PID P: "); Serial.println(battery.heater.pidP);
            Serial.print("PID I: "); Serial.println(battery.heater.pidI);
            Serial.print("PID D: "); Serial.println(battery.heater.pidD);
            Serial.print("TuneOk: "); Serial.println(battery.stune.done);
            Serial.print("HeatOn: "); Serial.println(battery.heater.enable);
#endif
            break;

        case ALL: // Save all settings
            // Call each case to save all settings
            saveSettings(SETUP);
            saveSettings(WIFI);
            saveSettings(HTTP);
            saveSettings(MQTT);
            saveSettings(TELEGRAM);
            saveSettings(PID);

#ifdef DEBUG
            Serial.println("Saved All Settings.");
#endif
            break;
    }
    preferences.end();

    battery.init = false;
}

void Battery::resetSettings(bool reset) {
    preferences.begin("btry", false);

    if (reset) {
        // Reset all settings to default values
        preferences.putString("myname", "onni"); // Default name
        preferences.putUChar("size", 0);
        preferences.putUChar("chrgr", 0);
        preferences.putUChar("capct", 0);
        preferences.putFloat("temperature", 0.0);
        preferences.putUChar("currentVolt", 0);
        preferences.putUChar("ecoVolt", 0);
        preferences.putUChar("boostVolt", 0);
        preferences.putUChar("resistance", 0);
        preferences.putUChar("boostTemp", 25);
        preferences.putUChar("ecoTemp", 15);
        preferences.putUChar("maxPower", 0);
        preferences.putBool("tboost", false);
        preferences.putBool("vboost", false);

        // Reset WiFi settings
        #ifndef DEBUG
        preferences.putString("wssid", "");  // Default SSID
        preferences.putString("wpass", "");  // Default password
        #endif

        // Reset HTTP settings
        preferences.putBool("httpen", false);
        preferences.putString("httpusr", "");  // Default username
        preferences.putString("httppass", "");  // Default password

        // Reset MQTT settings
        preferences.putBool("mqtten", false);
        preferences.putString("mqttip", "");  // Default MQTT server
        preferences.putUInt("mqttport", 0);  // Default MQTT port
        preferences.putString("mqttusr", "");  // Default MQTT username
        preferences.putString("mqttpass", "");  // Default MQTT password

        // Reset Telegram settings
        preferences.putBool("tgen", false);
        preferences.putString("tgtoken", "");  // Default Telegram token
        preferences.putInt("tgusr", 0);  // Default Telegram chat ID

        // Reset PID settings
        preferences.putFloat("pidP", 0.0);
        preferences.putFloat("pidI", 0.0);
        preferences.putFloat("pidD", 0.0);
        preferences.putBool("tuneOk", false);
        preferences.putBool("heatOn", false);
        preferences.end();
    } else {
        // Optionally, you can save the current values or perform other actions
        // For example, you could log the current settings or perform a different operation
        #ifdef DEBUG
        Serial.println("Current settings retained.");
        #endif
    }



    // Optionally, reset the battery object state
    battery.init = false;
}


void Battery::loadSettings(SettingsType type) {
    preferences.begin("btry", true);
    
    switch (type) {
        case SETUP:
            // Direct access to battery members
            battery.name = String(preferences.getString("myname", "Helmi"));
            battery.size = preferences.getUChar("size", 0);
            battery.temperature = preferences.getFloat("temperature", 0);
            battery.voltageInPrecent = constrain(preferences.getUChar("currentVolt", 1), 5, 100);
            battery.heater.resistance = constrain(preferences.getUChar("resistance", 0), 0, 255);
            battery.ecoVoltPrecent = constrain(preferences.getUChar("ecoVolt", 1), 50, 255);
            battery.boostVoltPrecent = constrain(preferences.getUChar("boostVolt", 1), 50, 255);
            battery.heater.boostTemp = constrain(preferences.getUChar("boostTemp", 1), 5, 40);
            battery.heater.ecoTemp = constrain(preferences.getUChar("ecoTemp", 1), 5, 30);
            battery.capct = constrain(preferences.getUChar("capct", 1), 1, 255);
            battery.chrgr.current = constrain(preferences.getUChar("chrgr", 0), 1, 5);
            battery.heater.powerLimit = constrain(preferences.getUChar("maxPower", 1), 1, 255);
            battery.tempBoost = preferences.getBool("tboost");
            battery.voltBoost = preferences.getBool("vboost");

#ifdef DEBUG
            // Print loaded settings for debugging
            Serial.println("Loaded Settings:");
            Serial.print("Name: "); Serial.println(battery.name);
            Serial.print("Size: "); Serial.println(battery.size);
            Serial.print("Temperature: "); Serial.println(battery.temperature);
            Serial.print("Voltage in Percent: "); Serial.println(battery.voltageInPrecent);
            Serial.print("Heater Resistance: "); Serial.println(battery.heater.resistance);
            Serial.print("Eco Voltage Percent: "); Serial.println(battery.ecoVoltPrecent);
            Serial.print("Boost Voltage Percent: "); Serial.println(battery.boostVoltPrecent);
            Serial.print("Heater Boost Temp: "); Serial.println(battery.heater.boostTemp);
            Serial.print("Heater Eco Temp: "); Serial.println(battery.heater.ecoTemp);
            Serial.print("Capacity: "); Serial.println(battery.capct);
            Serial.print("Charger Current: "); Serial.println(battery.chrgr.current);
            Serial.print("Heater Max Power: "); Serial.println(battery.heater.maxPower);
            Serial.print("Temperature Boost: "); Serial.println(battery.tempBoost);
            Serial.print("Voltage Boost: "); Serial.println(battery.voltBoost);
#endif
            break;

        case WIFI:
            battery.wlan.ssid = String(preferences.getString("wssid"));
            battery.wlan.pass = String(preferences.getString("wpass"));

#ifdef DEBUG
            // Print loaded WiFi settings for debugging
            Serial.println("Loaded WiFi Settings:");
            Serial.print("SSID: "); Serial.println(battery.wlan.ssid);
            Serial.print("Password: "); Serial.println(battery.wlan.pass);
#endif
            break;

        case HTTP:
            battery.http.enable = preferences.getBool("httpen");
            battery.http.username = String(preferences.getString("httpusr"));
            battery.http.password = String(preferences.getString("httppass"));

#ifdef DEBUG
            // Print loaded HTTP settings for debugging
            Serial.println("Loaded HTTP Settings:");
            Serial.print("HTTP Enabled: "); Serial.println(battery.http.enable);
            Serial.print("Username: "); Serial.println(battery.http.username);
            Serial.print("Password: "); Serial.println(battery.http.password);
#endif
            break;

        case MQTT:
            battery.mqtt.enable = preferences.getBool("mqtten");
            battery.mqtt.username = String(preferences.getString("mqttusr"));
            battery.mqtt.password = String(preferences.getString("mqttpass"));
            battery.mqtt.server = String(preferences.getString("mqttip"));
            // battery.mqtt.port = preferences.getInt("mqttport");

#ifdef DEBUG
            // Print loaded MQTT settings for debugging
            Serial.println("Loaded MQTT Settings:");
            Serial.print("MQTT Enabled: "); Serial.println(battery.mqtt.enable);
            Serial.print("Username: "); Serial.println(battery.mqtt.username);
            Serial.print("Password: "); Serial.println(battery.mqtt.password);
            Serial.print("Server: "); Serial.println(battery.mqtt.server);
#endif
            break;

        case TELEGRAM:
            battery.telegram.enable = preferences.getBool("tgen");
            battery.telegram.token = preferences.getString("tgtoken");
            battery.telegram.chatId = preferences.getInt("tgusr");

#ifdef DEBUG
            // Print loaded Telegram settings for debugging
            Serial.println("Loaded Telegram Settings:");
            Serial.print("Telegram Enabled: "); Serial.println(battery.telegram.enable);
            Serial.print("Token: "); Serial.println(battery.telegram.token);
            Serial.print("Chat ID: "); Serial.println(battery.telegram.chatId);
#endif
            break;

        case PID:
            battery.heater.pidP = preferences.getFloat("pidP");
            battery.heater.pidI = preferences.getFloat("pidI");
            battery.heater.pidD = preferences.getFloat("pidD");
            battery.stune.done = preferences.getBool("tuneOk");
            battery.heater.enable = preferences.getBool("heatOn");

#ifdef DEBUG
            // Print loaded PID settings for debugging
            Serial.println("Loaded PID Settings:");
            Serial.print("PID P: "); Serial.println(battery.heater.pidP);
            Serial.print("PID I: "); Serial.println(battery.heater.pidI);
            Serial.print("PID D: "); Serial.println(battery.heater.pidD);
            Serial.print("TuneOk: "); Serial.println(battery.stune.done);
            Serial.print("HeatOn: "); Serial.println(battery.heater.enable);
#endif
            break;

        case ALL:
            // Load all settings by calling each case
            loadSettings(SETUP);
            loadSettings(WIFI);
            loadSettings(HTTP); 
            loadSettings(MQTT);
            loadSettings(TELEGRAM);
            loadSettings(PID);
            break;
    }
    preferences.end();

    battery.init = false;
}
/*
    Battery voltage reading function call. 
        -   In the future we could have some gaussian filter, 
            to compare reading in the previous reading. Like Moving avarage
            but in a array. And this would lead into actionable data.
*/
void Battery::readVoltage(uint32_t intervalSeconds) {
    // Check if enough time has passed since the last reading
    if (millis() - battery.adc.time >= intervalSeconds) {
        battery.adc.time = millis(); // Update the time in the adc struct

        if (!battery.chrgr.enable) {
            gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
            gpio_set_level(GPIO_NUM_25, HIGH);
            delay(6);
        }

        // Read the voltage from the ADC
        battery.adc.raw = adc1_get_raw(ADC_CHANNEL); // Store raw ADC value in the adc struct

        if (!battery.chrgr.enable) {
            gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
            gpio_set_level(GPIO_NUM_25, LOW);
        }

        if (battery.firstRun) {
            for (int i = 0; i < battery.adc.mAvg; i++) {
                battery.adc.readings[i] = battery.adc.raw; // Use the first reading
            }
            battery.firstRun = false; // Set firstRun to false after initialization
        }

        // Update the moving average array
        battery.adc.readings[battery.adc.index] = battery.adc.raw; // Store the latest raw reading
        battery.adc.index = (battery.adc.index + 1) % battery.adc.mAvg; // Increment index and wrap around

        // Calculate the moving average
        battery.adc.sum = 0; // Initialize sum to zero
        for (int i = 0; i < battery.adc.mAvg; i++) 
        {
            battery.adc.sum += battery.adc.readings[i]; // Sum all readings
        }

        battery.adc.avg = battery.adc.sum / battery.adc.mAvg; // Calculate average
        battery.milliVoltage = esp_adc_cal_raw_to_voltage(battery.adc.avg, &characteristics) * float(30.81);

        // Check if the moving average is within the valid range
        if (battery.milliVoltage > 9000 && battery.milliVoltage < 100000) {
 
            int newSeries = determineBatterySeries(battery.milliVoltage);
            if (battery.sizeApprx < newSeries) {
                battery.sizeApprx = newSeries;
            }

            battery.voltageInPrecent = getVoltageInPercentage(battery.milliVoltage);
        } else {
            battery.milliVoltage = 1; // Set the accurate voltage to 1 in case of reading error
            battery.voltageInPrecent = 1; // Set the voltage in percentage to 1 in case of reading error
            Serial.println(" Voltage reading error ");
            Serial.print(" Voltage: ");
            Serial.println(battery.adc.avg);
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
        milliVolts = (battery.size * 3000);
        milliVolts += (battery.size * (1200 * (float)precent ) / float(100));
        return milliVolts / 1000;
    }
    else {
        float volttis = ((battery.size * 3) + (battery.size * (float)1.2 * (precent / float(100))));
        return volttis;
    }
}
/*
    Lets convert the given milliVoltage to precentage at given series 
*/
int Battery::getVoltageInPercentage(uint32_t milliVoltage) {
    uint32_t minVoltage = 3000;  // Minimum voltage in millivolts (3V)
    uint32_t maxVoltage = 4200;  // Maximum voltage in millivolts (4.2V)

    milliVoltage = constrain(milliVoltage, 25000, 100000);
    if (milliVoltage < (minVoltage * battery.size)) {
        return 0;  // Below minimum voltage, 0% usable area
        Serial.println("Below minimum voltage");
    } else if (milliVoltage > (maxVoltage * battery.size)) {
        return 0;  // Above maximum voltage, 100% usable area
        Serial.println("Above maximum voltage");
    } else {
        uint32_t voltageRange = (maxVoltage - minVoltage) * battery.size;
        uint32_t voltageDifference = milliVoltage - ( minVoltage * battery.size) ;
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
    // Serial.println(battery.voltageInPrecent);
    return battery.voltageInPrecent;
}
/*
    Setters and Getters for battery economy voltage settings.
        ESPUI interface for the user to set the battery voltage settings.
    
    ECONOMY CHARGING
*/
bool Battery::setEcoPrecentVoltage(int value) {
    if(value > 49 && value < 101 ) {
        if (value < battery.boostVoltPrecent) {
            battery.ecoVoltPrecent = value;
        return true;
        }
        else return false;
    }
    else return false;
}

// @brief Get the battery economy voltage percentage endpoint
uint8_t Battery::getEcoPrecentVoltage() {
    return battery.ecoVoltPrecent;
}
/*
    Setters and Getters for battery BOOST settings
 
        todo: This is for the user API through ESPUI. 
        Have some filtering, maybe some error handling. Lets worry about that later on.
*/
bool Battery::setBoostPrecentVoltage(int boostPrecentVoltage) {
   
    if(boostPrecentVoltage > 49 && boostPrecentVoltage < 101) {
        
        if(boostPrecentVoltage > battery.ecoVoltPrecent) {
            battery.boostVoltPrecent = boostPrecentVoltage;
            return true;
        }
        else return false;       
    }
    else return false;
}

// @brief Get the battery boost voltage percentage endpoint
uint8_t Battery::getBoostPrecentVoltage() {
    return battery.boostVoltPrecent;
}
/*
    Battery DS18B20 temp sensor handling. 

    Maybe have some moving avarage sum, where we can get the average temperature over a period of time.
    This is to compare the reading, if it differs from the previous reading, we can have a failsafe.
*/
float Battery::getTemperature() {

    if(battery.temperature < float(80) && battery.temperature > float(-50)) {
        return battery.temperature;
    }
    else return 255;
}   
/*
    Battery's most important function. Settings its nominal string value. 
        For charging, economy and boost mode definitions.
        In the future this cuold maybe have automated, but thats too much work for now. 
*/
bool Battery::setNominalString(uint8_t size) {
    if(size > 6 && size < 22)
        {
            battery.size = size;
            Serial.print("nominalSize");
            Serial.println(size);
            return true;
        }
    else return false;
}

uint8_t Battery::getNominalString() {
    return battery.size;
}
/*
    Setter and Getter for battery boost temperature settings. 
        - ESPUI interface for the user to set the battery temperature settings.
*/
uint8_t Battery::getEcoTemp() {
    return battery.heater.ecoTemp;
}

bool Battery::setEcoTemp(int ecoTemp) {
    if(ecoTemp > 0 && ecoTemp < 31 )
        if(ecoTemp < battery.heater.boostTemp) {
            battery.heater.ecoTemp = ecoTemp;
            return true;
        }
        else return false;
    else return false;
}
/*
    SetGetter for Temperature boost settings. 
        - ESPUI interface for the user to set the battery temperature settings.
*/
uint8_t Battery::getBoostTemp() {
    return battery.heater.boostTemp;
}

bool Battery::setBoostTemp(int boostTemp) {

    if(boostTemp > 9 && boostTemp < 41)
        if(boostTemp > battery.heater.ecoTemp) {
            battery.heater.boostTemp = boostTemp;
            return true;
        }
        else return false;

    else return false;
}

/*
    Temp and Voltage boost settings. 
        - Activate and deactivate the boost settings. 
        - ESPUI interface for the user to set the battery temperature settings.

        Have it reset after certain timeout, just in case.
*/
bool Battery::activateTemperatureBoost(bool tempBoost) {
    if(tempBoost) {
            preferences.begin("btry", false);
            battery.tempBoost = true;
            preferences.putBool("tboost", true);
            preferences.end(); // Close preferences
            battery.stune.enable = true;
            battery.stune.startTime = millis();
            return true;
    }
    else {
            preferences.begin("btry", false);
            battery.tempBoost = false;
            preferences.putBool("tboost", false);
            preferences.end(); // Close preferences
            return false;
        }
    }


bool Battery::getActivateTemperatureBoost() {
    if (battery.tempBoost)
        return true;
    else
        return false;
}

bool Battery::activateVoltageBoost(bool voltBoost) {
    if(voltBoost) {
        battery.voltBoost = true;
        preferences.begin("btry", false);           // Open preferences with namespace "battery_settings"
        preferences.putBool("vboost", true);
        preferences.end();                          // Close preferences
        return true;
    }
    else{
        battery.voltBoost = false;
        preferences.begin("btry", false);           // Open preferences with namespace "battery_settings"
        preferences.putBool("vboost", false);
        preferences.end();                          // Close preferences
        return false;
    }
}

bool Battery::getActivateVoltageBoost() {
    if (battery.voltBoost)
        return true;
    else
        return false;
}

float Battery::calculateChargeTime(int initialPercentage, int targetPercentage) {
    // Calculate the charge needed in Ah
    float chargeNeeded = battery.capct * ((targetPercentage - initialPercentage) / (float)100.0);
    // Serial.println(chargeNeeded);
    // Calculate the time required in minutes
    float timeRequired = (chargeNeeded / battery.chrgr.current);
    // Serial.print("timeRequired: ");
    // Serial.println(timeRequired);
    battery.chrgr.time = timeRequired;
    return timeRequired;
}

    // Setter function to set charger current and battery capacity
bool Battery::setCharger(int chargerCurrent) {
    if(chargerCurrent > 0 && chargerCurrent < 11) {
        battery.chrgr.current = chargerCurrent;
        return true;
    }
    else return false;
}

// Getter for charger current
int Battery::getCharger() {
    return battery.chrgr.current;
}

// Setter for battery capacity
bool Battery::setCapacity(int capacity) {
    if (capacity > 0 && capacity < 250) {
        battery.capct = capacity;
        return true;
    } else {
        return false;
    }
}

// Getter for battery capacity
int Battery::getCapacity()  {
    return battery.capct;
}
/*
    // Getter function to get charger current and battery capacity
    uint32_t Battery::getCharger(uint32_t chargerCurrent, uint32_t batteryCapacity)  {
        return chargerCurrent = battery.chrgr, batteryCapacity = battery.capct;
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

    // Tarkistetaan, ylittääkö jäännös 5% kennojännitteestä in thousandths to make math work without floats
    if (remainder_mV * toleranceDivisor > voltagePerCell_mV * toleranceMultiplier) {
        return measuredSeries + 1; // Siirrytään seuraavaan sarjaan
    } else {
        return measuredSeries; // Pysytään samassa sarjassa
    }
}

float Battery::getCurrentVoltage() {
    float volts = 0.0f;
    volts = battery.milliVoltage / (float)(1000.0F);  // old, no need?
    return volts;
}

int Battery::getBatteryApprxSize() {
    return battery.sizeApprx;
}

void Battery::charger(bool chargerState) {
    if (chargerState) {
        gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_NUM_25, HIGH);
        // Serial.print (" Chrgr: ON ");
        battery.chrgr.enable = true;
    } else {
        gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_NUM_25, LOW);
        /// Serial.print(" Chrgr: OFF ");
        battery.chrgr.enable = false;
    }
}

bool Battery::isValidHostname(const String& hostname) {
    // Check length
    if (hostname.length() > 12) {
        return false; // Too long
    }

    // Check for valid characters
    for (size_t i = 0; i < hostname.length(); i++) {
        char c = hostname[i];
        // Use std::isalnum instead of isAlphanumeric
        if (!std::isalnum(c) && c != '-' && c != '_' && c != '.' && c != '~') {
            return false; // Invalid character found
        }
    }
    return true; // Valid hostname
}

bool Battery::setHostname(String hostname) {

    // if(isValidHostname(hostname)) {
        battery.name = hostname;
        return true;
    //}
    //else return false;
}

bool Battery::getHostname() {
    preferences.begin("btry", true);
    String hostname = preferences.getString("myname");
    preferences.end();
    battery.name = hostname;
    return true;
}

bool Battery::setResistance(uint8_t resistance) {
    if (resistance > 10 && resistance < 255) {
        battery.heater.resistance = resistance;
        return true;
    } else {
        return false;
    }
}

int Battery::getResistance() {
    return battery.heater.resistance;
}

void Battery::adjustHeaterSettings() {
    // const float maxPower = 30.0; // Maximum power in watts

    // Calculate the maximum allowable power based on the resistance and voltage
    float power = (battery.size * float(3.7) * battery.size * float(3.7)  ) / battery.heater.resistance;

    // Calculate the duty cycle needed to cap the power at maxPower
    float dutyCycle = battery.heater.maxPower / power;

    if (dutyCycle > 1) {
        dutyCycle = 1;
    }

    // Calculate the output limits for QuickPID
    battery.heater.powerLimit  = (dutyCycle * 255);

    // Ensure the output limit does not exceed the maximum allowable current
    // Ensure the output limit does not exceed the maximum allowable current
    if (battery.heater.powerLimit > 255) {
        battery.heater.powerLimit = 254;
    }
    if (battery.heater.powerLimit < 20) {
        battery.heater.powerLimit = 20;
    }

    // Adjust the QuickPID output limits
    heaterPID.SetOutputLimits(float(0), float(battery.heater.powerLimit));
}

bool Battery::setPidP(float p) {
    if (p >= 0 && p < 250) {
        battery.heater.pidP = p;

        // preferences.begin("btry", false);
        // preferences.putFloat("pidP", constrain(p, 0, 250));
        // preferences.end();
        // heaterPID.SetTunings(float(battery.pidP), float(0.05), 0);
        // heaterPID.SetProportionalMode(float(battery.pidP));      
        return true;
    } else {
        return false;
    }
}

int Battery::getPidP() {
    return battery.heater.pidP;
}

bool Battery::getChargerStatus() {
    return battery.chrgr.enable;
}

void Battery::publishBatteryData() {
    #ifdef MQTT_ENABLED
        String baseTopic = "battery/" + String(battery.name) + "/";
        mqtt.publish((baseTopic + "size").c_str(),                 String(battery.size).c_str());
        mqtt.publish((baseTopic + "temperature").c_str(),          String(battery.temperature).c_str());
        mqtt.publish((baseTopic + "voltageInPrecent").c_str(),     String(battery.voltageInPrecent).c_str());
        mqtt.publish((baseTopic + "ecoVoltPrecent").c_str(),       String(battery.ecoVoltPrecent).c_str());
        mqtt.publish((baseTopic + "boostVoltPrecent").c_str(),     String(battery.boostVoltPrecent).c_str());
        mqtt.publish((baseTopic + "ecoTemp").c_str(),              String(battery.heater.ecoTemp).c_str());
        mqtt.publish((baseTopic + "boostTemp").c_str(),            String(battery.heater.boostTemp).c_str());
        mqtt.publish((baseTopic + "resistance").c_str(),           String(battery.heater.resistance).c_str());
        mqtt.publish((baseTopic + "capct").c_str(),                String(battery.capct).c_str());
        mqtt.publish((baseTopic + "chrgr").c_str(),                String(battery.chrgr.current).c_str());
        mqtt.publish((baseTopic + "maxPower").c_str(),             String(battery.heater.maxPower).c_str());
        mqtt.publish((baseTopic + "pidP").c_str(),                 String(battery.heater.pidP).c_str());
        mqtt.publish((baseTopic + "pidI").c_str(),                 String(battery.heater.pidI).c_str());
        mqtt.publish((baseTopic + "pidD").c_str(),                 String(battery.heater.pidD).c_str());
        mqtt.publish((baseTopic + "tempBoost").c_str(),            String(battery.tempBoost).c_str());
        mqtt.publish((baseTopic + "voltBoost").c_str(),            String(battery.voltBoost).c_str());
     
        // Publish MQTT settings
        mqtt.publish((baseTopic + "mqtt/enable").c_str(),          String(battery.mqtt.enable).c_str());
        // Publish Telegram settings
        mqtt.publish((baseTopic + "telegram/enable").c_str(),      String(battery.telegram.enable).c_str());
        // }
        #endif
}

void Battery::mqttSetup() {
    #ifdef MQTT_ENABLED
    preferences.begin("btry", true);
        battery.mqtt.enable     = preferences.getBool("mqtten");
        battery.mqtt.server     = preferences.getString("mqttip");
        battery.mqtt.port       = preferences.getInt("mqttport", 1883);
        battery.mqtt.username   = preferences.getString("mqttusr");
        battery.mqtt.password   = preferences.getString("mqttpass");
    preferences.end();
    mqtt.setServer(battery.mqtt.server.c_str(), uint16_t(battery.mqtt.port));
    battery.mqtt.setup = true;
    #endif
}

void Battery::handleMqtt() {
    #ifdef MQTT_ENABLED
    
    if(!battery.mqtt.setup)  mqttSetup();

    if(battery.mqtt.enable) { 
        mqtt.loop();
        if (millis() - battery.mqtt.lastMessageTime > 60000 ) {
            battery.mqtt.lastMessageTime = millis();
            if(WiFi.isConnected()) {
                if(mqtt.connected()) publishBatteryData();
                else mqtt.connect(battery.name.c_str(), battery.mqtt.username.c_str(), battery.mqtt.password.c_str());
            }
            else WiFi.reconnect();
        }
    }

    #endif
}

bool Battery::getMqttState() {
    return battery.mqtt.enable; 

}

bool Battery::setMqttState(bool status) {
    if (status) {
        battery.mqtt.enable = true;
        return true;
    }
    else {
        battery.mqtt.enable = false;
        return false;
    }
}

bool Battery::getTelegramEn() {
    return battery.telegram.enable;
}

bool Battery::setTelegramEn(bool status) {
    if (status) {
        battery.telegram.enable = true;
        return true;
    }
    else {
        battery.telegram.enable = false;
        return false;
    }
}

bool Battery::getHttpEn() {
    return battery.http.enable;
}

bool Battery::setHttpEn(bool status) {
    if(status) {
        battery.http.enable = true;
        return true;
    }
    else {
        battery.http.enable = false;
        return false;
    }
}

bool Battery::telegramSendMessage(const char *message) {
#ifdef TELEGRAM_ENABLED
  WiFiClientSecure client;
  HTTPClient https;
  bool result = false;
  client.setInsecure();
  if (https.begin(client, String("https://api.telegram.org/bot") + String(battery.telegram.token) + "/")) {
    String post;
    https.addHeader("Content-Type", "application/json");
    https.addHeader("Connection", "close");
    post = "{\"method\":\"sendMessage\",\"chat_id\":" + String(battery.telegram.chatIdStr) + ",\"text\":\"" + message + "\"}";
    //Serial.println(">> " + post);
    result = https.POST(post) == 200;
    // Serial.println("<< " + https.getString());
    https.end();
  }
  return result;
#else
    return false;
#endif
}




