#include "Battery.h"
#include <esp_adc_cal.h>
#include <Preferences.h>
#include <OneWire.h>
#include <ESPUI.h>
#include <sTune.h>
#include <PubSubClient.h>
#include <AsyncTelegram2.h>
#include <WiFiClientSecure.h>




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
#define LOGS_SIZE 10

// batterys Battery::state;

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

//Battery::Battery(int tempSensor, int voltagePin, int heaterPin, int chargerPin, int greenLed, 
//                    int yellowLed, int redLed);


Battery::~Battery() {
    digitalWrite(heaterPin, LOW);    // Turn off heater
    digitalWrite(chargerPin, LOW);   // Turn off charger
    
    // Save settings before destruction
    saveSettings(ALL);
    
    // Stop PWM
    ledcDetachPin(heaterPin);
    
    // Turn off LEDs
    red.stop();
    yellow.stop();
    green.stop();
}

void Battery::setup() {

        pinMode(CHARGER_PIN, OUTPUT);
        pinMode(heaterPin, OUTPUT);

        dallas.begin();

        adc1_config_width(ADC_WIDTH_12Bit);
        adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH_BIT_12, V_REF, &characteristics);

        heaterPID = QuickPID(&pidInput, &pidOutput, &pidSetpoint, kp, ki, kd, QuickPID::Action::direct);
        heaterPID.SetTunings(kp, ki, kd); //apply PID gains
        heaterPID.SetOutputLimits(0, 255);
        heaterPID.SetSampleTimeUs(1000 * 1000); // 1 second.
        heaterPID.SetMode(QuickPID::Control::manual);   

        tuner.Configure(inputSpan, outputSpan, outputStart, outputStep, testTimeSec, settleTimeSec, samples);
        tuner.SetEmergencyStop(tempLimit);

        ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
        ledcAttachPin(heaterPin, PWM_CHANNEL);

        green.start();
        green.setDelay(1000);

        yellow.start();
        yellow.setDelay(1000);

        preferences.begin("btry", false);
        float tempppi = float(30);
        preferences.putFloat("temperature", tempppi);
        preferences.end();

        red.start();
        red.setDelay(1000, 2000);

        telegram.setUpdateTime(1000);
        telegram.setTelegramToken((battery.telegram.token).c_str());
        configTzTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
     
        #if USE_CLIENTSSL == false
        client.setCACert(telegram_cert);
        #endif

        client.setInsecure();  // Skip certificate validation
/*
        telegram.begin();
                   // Add reply keyboard
            isKeyboardActive = false;
                        
            myMainMenuKbd.addButton(Status, KeyboardButtonSimple);               // Voltage, temp, boost, boost, Ip, etc
            myMainMenuKbd.addButton(Boost, KeyboardButtonSimple);                 // activate Temp or Volt Boost
            myMainMenuKbd.addRow();
            myMainMenuKbd.addButton("Manual", KeyboardButtonSimple);               // External URL -> github
            myMainMenuKbd.addButton("Settings", KeyboardButtonSimple);           // eco & boost limits
            myMainMenuKbd.addRow();
            myMainMenuKbd.addButton("Logs", KeyboardButtonSimple);
            myMainMenuKbd.addButton("End", KeyboardButtonSimple);

            StatusKbd.addButton("Status", Status, KeyboardButtonQuery);
            StatusKbd.addRow();
            StatusKbd.addButton("Voltage", STATUSCB, KeyboardButtonQuery);
            StatusKbd.addButton("Temperature", STATUSCB, KeyboardButtonQuery);
            StatusKbd.addRow();
            StatusKbd.addButton(" V   ", BOOSTCB, KeyboardButtonQuery);
            StatusKbd.addButton(" C   ", BOOSTCB, KeyboardButtonQuery);    

            
            StatusKbd.addButton("Temperature: C", KeyboardButtonSimple);
            StatusKbd.addRow();
            StatusKbd.addRow();
            StatusKbd.addButton("Volt Boost: 80% ");
            StatusKbd.addButton("Temp Boost: 32 °C ");
            StatusKbd.addRow();
            StatusKbd.addButton("To: 80%");
            StatusKbd.addButton("To: 32 °C");
            StatusKbd.addRow();
            StatusKbd.addButton("Volt: Start", KeyboardButtonSimple);
            StatusKbd.addButton("Temp: Start", KeyboardButtonSimple);
            StatusKbd.addRow();
            StatusKbd.addButton("Volt: Stop", KeyboardButtonSimple);
            StatusKbd.addButton("Temp: Stop", KeyboardButtonSimple);
            StatusKbd.addRow();
            StatusKbd.addButton("Back", KeyboardButtonSimple);
            

            char welcome_msg[128];
            snprintf(welcome_msg, 128, "BOT @%s online\n/help all commands avalaible.", telegram.getBotName());
            telegram.sendTo(battery.telegram.chatId, welcome_msg);
            */


        // General settings from preferences
        loadSettings(ALL);
        mqttSetup();
}
void Battery::loop() {

    red.blink();
    green.blink();   
    yellow.blink();  

    handleBatteryControl();         // Logic for battery control

    // readVoltage(5);                 // Voltage reading

    updateHeaterPID();              // temperature control
    
    //readTemperature();              // Temperature reading


    handleMqtt();                   // handle mqtt loop & connection & enable



    if (this->battery.telegram.enable) {
        TBMessage msg;

        if (millis() - battery.telegram.lastMessageTime > 3600) {  // 1 hour
            if ((battery.milliVoltage - battery.telegram.deltaVoltage) > 500 || (battery.telegram.deltaVoltage - battery.milliVoltage) > 500) {
                char voltage[64];
                snprintf(voltage, 64, "Voltage: %0.1f V \n Temp:   %0.1f °C", (battery.milliVoltage / float(1000)), battery.temperature );
                telegram.sendTo(battery.telegram.chatId, voltage);
                battery.telegram.lastMessageTime = millis();
            }
            battery.telegram.deltaVoltage = battery.milliVoltage;
        }
            // feedback loop time
        if (millis() - tgLoop > 500) {
            tgLoop = millis();
            
            if(telegram.getNewMessage(msg)) {
            
            // check what kind of message I received
            MessageType msgType = msg.messageType;
            String msgText = msg.text;
 
            switch (msgType) {
              case MessageText:
                // received a text message
                Serial.print("\nMsg received: ");
                Serial.println(msgText);
                // check if is show keyboard command
                if (msgText.equalsIgnoreCase("/start")) {
                  // the user is asking to show the reply keyboard --> show it
                  telegram.sendMessage(msg, "Battery Jeesus", myMainMenuKbd);
                }
                else if (msgText.equalsIgnoreCase("/status")) {
                  telegram.sendMessage(msg, "Battery Jeesus:", StatusKbd);
                  isKeyboardActive = true;
                }

                // check if the reply keyboard is active
                else if (isKeyboardActive) {
                  // is active -> manage the text messages sent by pressing the reply keyboard buttons
                  if (msgText.equalsIgnoreCase("/end")) {
                    // sent the "hide keyboard" message --> hide the reply keyboard
                    telegram.removeReplyKeyboard(msg, "Reply keyboard removed");
                    isKeyboardActive = false;
                  } else {
                    // print every others messages received
                    telegram.sendMessage(msg, msg.text);
                  }
                }

                // the user write anything else and the reply keyboard is not active --> show a hint message
                else {
                  telegram.sendMessage(msg, "Try /start or /inline or /end");
                }
                break;

              case MessageQuery:
                // received a callback query message
                msgText = msg.callbackQueryData;
                Serial.print("\nCallback query message received: ");
                Serial.println(msg.callbackQueryData);

                if (msgText.equalsIgnoreCase(Status)) {
                    telegram.sendMessage(msg, "Battery Jeesus:", StatusKbd);
                    isKeyboardActive = true;
                }
                else if (msgText.equalsIgnoreCase(Boost)) {
                    telegram.sendMessage(msg, "Boost:", BoostKbd);
                    isKeyboardActive = true;
                }
                break;

              case MessageLocation: {
                  // received a location message
                  String reply = "Longitude: ";
                  reply += msg.location.longitude;
                  reply += "; Latitude: ";
                  reply += msg.location.latitude;
                  Serial.println(reply);
                  telegram.sendMessage(msg, reply);
                  break;
                }

              case MessageContact: {
                  // received a contact message
                  String reply = "Contact information received:";
                  reply += msg.contact.firstName;
                  reply += " ";
                  reply += msg.contact.lastName;
                  reply += ", mobile ";
                  reply += msg.contact.phoneNumber;
                  Serial.println(reply);
                  telegram.sendMessage(msg, reply);
                  break;
                }

              default:
                break;
            }    
        }
   
    }


/*
    // MQTT on or off
    if (battery.mqtt.enabled) {
        mqtt.loop();
        if (millis() - battery.mqtt.lastMessageTime > 30000) {
            char buffer[16];
            char topicBuffer[48];
            sprintf(buffer, "%u", battery.size);
            sprintf(topicBuffer, "battery/%s/size", String("Helmis"));
            mqtt.publish(topicBuffer, buffer);
        }
    }
*/


 }
// end loop
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

firstRun = false;
    if(firstRun) {
        pidSetpoint = pidSetpoint;
        pidInput = battery.temperature;
        tuner.softPwm(heaterPin, pidInput, pidOutput, pidSetpoint, outputSpan, 1);
            switch (tuner.Run()) {
                case tuner.sample: // active once per sample during test
                  this->pidInput = battery.temperature;
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
                  this->pidInput = battery.temperature;
                  heaterPID.Compute();
                  tuner.plotter(pidInput, pidOutput, pidSetpoint, 1.0f, 60);
                  tuner.GetAutoTunings(&kp, &ki, &kd); // sketch variables updated by sTune
                  break;
                default:
                    Serial.println("unknown type in the Tuner");
                break;
            }
    }

    else if (( millis() - heaterTimer >= 1000)) {
        heaterTimer = millis();

        Battery::adjustHeaterSettings();

        this->pidInput = battery.temperature; // Update the input with the current temperature
        this->pidSetpoint = battery.wantedTemp; // Update the setpoint with the wanted temperature
        /*
        Serial.print("PID: ");
        Serial.print("O: ");
        Serial.print(pidOutput);
        Serial.print(" ");
        Serial.print(battery.heater.maxPower);  
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
        Serial.print(" ");
        Serial.print(" Temp: ");
        Serial.print(battery.temperature);
        */
        uint8_t powerOutput = pidOutput;
        // Serial.println(powerOutput);
        
        heaterPID.Compute();
        controlHeaterPWM(powerOutput); // Control the heater with the PID output
    }
}
/*
        Dallas temp sensor function

        @bried: Reads the temperature from the Dallas sensor
        @return: float
*/
void Battery::readTemperature() {

    if (millis() - dallasTime  >= 2500) {
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
        }
    }
}


void Battery::handleBatteryControl() {
    uint32_t currentMillis = millis(); 
    
    if (currentMillis - lastVoltageStateTime >= 1000) {
        battery.previousVState = battery.vState;
        battery.previousTState = battery.tState;

        // battery.previousVState = battery.vState;
        // battery.previousTState = battery.tState;

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

        if(battery.temperature > float(0) && battery.temperature < float(40)) { 
            vState = getVoltageState(battery.voltageInPrecent, battery.ecoVoltPrecent, battery.boostVoltPrecent);  
        }
        else {   
            charger(false);
            tState = getTempState(battery.temperature, battery.heater.ecoTemp, battery.heater.boostTemp);         
        }
    switch (vState) {
            case LAST_RESORT:
                    tState = getTempState(battery.temperature, 2, 3);
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

                    tState = getTempState(battery.temperature, battery.heater.ecoTemp, battery.heater.ecoTemp);

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

                    tState = getTempState(battery.temperature, battery.heater.ecoTemp, battery.heater.boostTemp);

                    if(tState != SUBZERO && tState != TEMP_WARNING) { charger(true); }

                break;
            case ECONOMY:

                    red.setDelay(1000, 0);
                    green.setDelay(0, 1000);
                    yellow.setDelay(1000);

                    tState = getTempState(battery.temperature, battery.heater.ecoTemp, battery.heater.boostTemp);

                    if(tState != SUBZERO && tState != TEMP_WARNING ) 
                        { charger(true); }

                break;
            case BOOST:

                    red.setDelay(1000, 0);
                    green.setDelay(0, 1000);
                    yellow.setDelay(1000);

                    tState = getTempState(battery.temperature, battery.heater.ecoTemp, battery.heater.boostTemp);

                    if ((tState != SUBZERO && tState != TEMP_WARNING ) && battery.voltBoost)   // if not in cold or warning state, and boost is active
                        { charger(true); }                                                         // then charge
                    else 
                        { charger(false); }
                break;
            case VBOOST_RESET:

                    red.setDelay(1000, 0);
                    yellow.setDelay(1000, 0);
                    green.setDelay(0, 1000);

                    tState = getTempState(battery.temperature, battery.heater.ecoTemp, battery.heater.boostTemp);

                    if (!battery.voltBoost) 
                        { charger(false); }

                    if(getActivateVoltageBoost()) 
                        { activateVoltageBoost(false); }

                break;
            default:
                Serial.println("unknown type in the state machine");
            break;
        }


    switch (tState) {
            case SUBZERO:

                    red.setDelay(0, 1000);

                    charger(false);

                 break;
            case ECO_TEMP:

                    if(battery.tempBoost) { battery.wantedTemp = battery.heater.boostTemp; }
                    else { battery.wantedTemp = battery.heater.ecoTemp; }

                break;
            case BOOST_TEMP:

                    if(battery.tempBoost) { battery.wantedTemp = battery.heater.boostTemp; }
                    else { battery.wantedTemp = battery.heater.ecoTemp; }

                break;
            case OVER_TEMP:

                    battery.wantedTemp = battery.heater.ecoTemp;
                    battery.tempBoost = false;

                 break;
            case TEMP_WARNING:

                    charger(false);
                    battery.wantedTemp = 0;
                    // controlHeaterPWM(0);

                break;
            case TBOOST_RESET:

                    battery.wantedTemp = battery.heater.ecoTemp;
                    battery.tempBoost = false;

                break;
           case DEFAULT_TEMP:

                    // controlHeaterPWM(0);
                    battery.wantedTemp = 0;

                break;
            default:
                Serial.println("unknown type in the temp state");
                break;
    }


    Serial.print("Temp");
    Serial.print(battery.temperature);

    Serial.print(" Kp: ");
    Serial.print(Battery::kp);

    Serial.print(" Ki: ");
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
    
    battery.vState = static_cast<int>(vState);
    battery.tState = static_cast<int>(tState);
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
    if ( voltagePrecent < 10) {
     return LAST_RESORT;  
    } else if (voltagePrecent >= 10 && voltagePrecent < 20) {
        return LAST_RESORT;                                                             // Lets shut all down
    } else if (voltagePrecent >= 20 && voltagePrecent < 30) {
        return LOW_VOLTAGE_WARNING;                                                     // lets warn the user with leds and even lower P value
    } else if (voltagePrecent >= 30 && voltagePrecent < 50) {
        return PROTECTED;                                                               // lets protect the battery by lowering the P term
    } else if (voltagePrecent >= 50 && voltagePrecent < ecoPrecentVoltage) {
        return ECONOMY;                                                                 // Normal operation
    } else if (voltagePrecent >= ecoPrecentVoltage && voltagePrecent < boostPrecentVoltage) {
        return BOOST;                                                                    // above economy voltage, in the wanted boost area
    } else if (voltagePrecent >= boostPrecentVoltage && voltagePrecent > 99) {
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

void Battery::saveSettings(SettingsType type) {
    preferences.begin("btry", false); // Open preferences with namespace "battery_settings" 

    // Save settings based on the type
    switch (type) {
        case SETUP:
            preferences.putString("myname",      String(this->battery.myname));
            preferences.putUChar("resistance",   constrain(this->battery.heater.resistance, 10, 255));
            preferences.putUChar("chrgr",        constrain(this->battery.charger.current, 1, 5));     
            preferences.putUChar("capct",        constrain(this->battery.capct, 1, 255));       
            preferences.putFloat("temperature",  constrain(this->battery.temperature, float(-40), float(40)));
            preferences.putUChar("currentVolt",  constrain(this->battery.voltageInPrecent, 1, 100));
            preferences.putUChar("ecoVolt",      constrain(this->battery.ecoVoltPrecent, 1, 100));
            preferences.putUChar("boostVolt",    constrain(this->battery.boostVoltPrecent, 1, 100));
            preferences.putUChar("size",         constrain(this->battery.size, 10, 20));
            preferences.putUChar("boostTemp",    constrain(this->battery.heater.boostTemp, 1, 40));
            preferences.putUChar("ecoTemp",      constrain(this->battery.heater.ecoTemp, 1, 30));
            preferences.putUChar("maxPower",     constrain(this->battery.heater.maxPower, 1, 255));

            // Quickpanel settings
            preferences.putBool("tboost",        constrain(this->battery.tempBoost, 0, 1));
            preferences.putBool("vboost",        constrain(this->battery.voltBoost, 0, 1));

            break;        
        case WIFI:
            preferences.putString("wssid", this->battery.wlan.ssid);
            preferences.putString("wpass", this->battery.wlan.pass);
            break;
        case HTTP:
            preferences.putBool("httpen", this->battery.http.enable);
            preferences.putString("httpusr", this->battery.http.username);
            preferences.putString("httppass", this->battery.http.password);
            break;
        case MQTT:
            preferences.putBool("mqtten", this->battery.mqtt.enable);
            preferences.putString("mqttip", this->battery.mqtt.server);
            preferences.putUInt("mqttport", this->battery.mqtt.port);
            preferences.putString("mqttusr", this->battery.mqtt.username);
            preferences.putString("mqttpass", this->battery.mqtt.password);
            break;
        case TELEGRAM:
            preferences.putBool("tgen", this->battery.telegram.enable);
            preferences.putString("tgtoken", this->battery.telegram.token);
            preferences.putInt("tgusr", this->battery.telegram.chatId);
            break;
        case PID:
            preferences.putFloat("pidP", this->battery.heater.pidP);
            preferences.putFloat("pidI", this->battery.heater.pidI);
            preferences.putFloat("pidD", this->battery.heater.pidD);
            break;
        case ALL: // Save all settings
            // Call each case to save all settings
            saveSettings(SETUP);
            saveSettings(WIFI);
            saveSettings(HTTP);
            saveSettings(MQTT);
            saveSettings(TELEGRAM);
            saveSettings(PID);

            break;
    }

    preferences.end(); // Close preferences, commit changes
}

void Battery::loadSettings(SettingsType type = ALL) {
    preferences.begin("btry", true); // Open preferences with namespace "battery_settings" in read-only mode

    // Load settings based on the type
    switch (type) {
        case SETUP:
            this->battery.myname = preferences.getString("myname", "Helmi");
            this->battery.size = constrain(preferences.getUChar("size", 10), 7, 20);
            this->battery.temperature = preferences.getFloat("temperature");
            this->battery.voltageInPrecent = constrain(preferences.getUChar("currentVolt", 1), 5, 100);
            this->battery.heater.resistance = constrain(preferences.getUChar("resistance", 1), 2, 255);
            this->battery.ecoVoltPrecent = constrain(preferences.getUChar("ecoVolt", 1), 5, 255);
            this->battery.boostVoltPrecent = constrain(preferences.getUChar("boostVolt", 1), 5, 255);
            this->battery.heater.boostTemp = constrain(preferences.getUChar("boostTemp", 1), 5, 40);
            this->battery.heater.ecoTemp = constrain(preferences.getUChar("ecoTemp", 1), 5, 30);
            this->battery.capct = constrain(preferences.getUChar("capct", 1), 1, 255);
            this->battery.charger.current = constrain(preferences.getUChar("chrgr", 0), 1, 5);
            this->battery.heater.maxPower = constrain(preferences.getUChar("maxPower", 1), 2, 255);
            this->battery.tempBoost = preferences.getBool("tboost");
            this->battery.voltBoost = preferences.getBool("vboost");
            break;

        case WIFI:
            this->battery.wlan.ssid = preferences.getString("wssid");
            this->battery.wlan.pass = preferences.getString("wpass");
            break;

        case HTTP:
            this->battery.http.enable = preferences.getBool("httpen");
            this->battery.http.username = preferences.getString("httpusr");
            this->battery.http.password = preferences.getString("httppass");
            break;

        case MQTT:
            this->battery.mqtt.enable = preferences.getBool("mqtten");
            this->battery.mqtt.username = preferences.getString("mqttusr");
            this->battery.mqtt.password = preferences.getString("mqttpass");
            this->battery.mqtt.server = preferences.getString("mqttip");
            // this->battery.mqtt.port = preferences.getInt("mqttport");
            break;

        case TELEGRAM:
            this->battery.telegram.enable = preferences.getBool("tgen");
            this->battery.telegram.token = preferences.getString("tgtoken"); // Load Telegram token
            this->battery.telegram.chatId = preferences.getInt("tgusr"); // Load Telegram chat ID
            // Add other telegram settings as needed
            break;

        case PID:
            this->battery.heater.pidP = preferences.getFloat("pidP");
            this->battery.heater.pidI = preferences.getFloat("pidI");
            this->battery.heater.pidD = preferences.getFloat("pidD");
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

    preferences.end(); // Close preferences
}

/*
    Battery voltage reading function call. 
        -   In the future we could have some gaussian filter, 
            to compare reading in the previous reading. Like Moving avarage
            but in a array. And this would lead into actionable data.
*/
void Battery::readVoltage(uint8_t intervalSeconds) {
    this->currentMillis = millis();

    if (millis() - voltageMillis >= (intervalSeconds * uint16_t(1000))) {
        this->voltageMillis = millis();

        // #ifndef VERSION_1  // different hardware setup
        // if(!battery.chargerState) {
        // gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
        // gpio_set_level(GPIO_NUM_25, HIGH); 
        // delay(5); }
        // #else
        // digitalWrite(CHARGER_PIN, HIGH);
        // delay(5);
        // #endif
        
        u_int32_t voltVal = adc1_get_raw(ADC_CHANNEL);
        u_int32_t calibratedVoltage = esp_adc_cal_raw_to_voltage(voltVal, &characteristics) * 30.81;

        /*
        #ifndef VERSION_1
        if(!battery.chargerState) {
        gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_NUM_25, LOW);   }
        #else
        digitalWrite(CHARGER_PIN, LOW);
        #endif
        */
    
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

        // Serial.print("movingAverage: ");
        // Serial.println(movingAverage);


        // Check if the moving average is within the valid range
        if (movingAverage > 10000 && movingAverage < 100000) {
            // Update battery size approximation if the new series is larger
            int newSeries = determineBatterySeries(movingAverage);
            if (this->battery.sizeApprx < newSeries) {
                this->battery.sizeApprx = newSeries;
                Serial.print(" BattApproxSize: ");
                Serial.println(this->battery.sizeApprx);
            }

            // Update voltage and accurate voltage
            this->battery.milliVoltage = movingAverage;

            Serial.print(" Voltage: ");
            Serial.println(this->battery.milliVoltage);
            
            // Update battery voltage in percentage
            this->battery.voltageInPrecent = getVoltageInPercentage(movingAverage);
        } else {
            this->battery.milliVoltage = 0; // Set the accurate voltage to 0 in case of reading error
            this->battery.voltageInPrecent = 0; // Set the voltage in percentage to 0 in case of reading error
            Serial.println(" Voltage reading error ");
            Serial.print(" Voltage: ");
            Serial.println(movingAverage);
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
        milliVolts = (this->battery.size * 3000);
        milliVolts += (this->battery.size * (1200 * (int)precent ) / 100);
        return milliVolts / 1000;
    }
    else {

    float volttis = ((this->battery.size * 3) + (this->battery.size * (float)1.2 * (precent / 100)));
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
    if (milliVoltage < (minVoltage * battery.size)) {
        return 0;  // Below minimum voltage, 0% usable area
        Serial.println("Below minimum voltage");
    } else if (milliVoltage > (maxVoltage * battery.size)) {
        return 0;  // Above maximum voltage, 100% usable area
        Serial.println("Above maximum voltage");
    } else {
        uint32_t voltageRange = (maxVoltage - minVoltage) * this->battery.size;
        uint32_t voltageDifference = milliVoltage - ( minVoltage * this->battery.size) ;
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
    return this->battery.voltageInPrecent;
}
/*

    Setters and Getters for battery economy voltage settings.
        ESPUI interface for the user to set the battery voltage settings.
    
    ECONOMY CHARGING
*/
bool Battery::setEcoPrecentVoltage(int value) {
    if(value > 49 && value < 101 && value < this->battery.boostVoltPrecent) {
        this->battery.ecoVoltPrecent = value;
        return true;
    }
    else return false;
}

// @brief Get the battery economy voltage percentage endpoint
uint8_t Battery::getEcoPrecentVoltage() {
    return this->battery.ecoVoltPrecent;
}
/*



    Setters and Getters for battery BOOST settings
 
        todo: This is for the user API through ESPUI. 
        Have some filtering, maybe some error handling. Lets worry about that later on.
*/
bool Battery::setBoostPrecentVoltage(int boostPrecentVoltage) {
        if(boostPrecentVoltage > this->battery.ecoVoltPrecent && boostPrecentVoltage < 101) {
            this->battery.boostVoltPrecent = boostPrecentVoltage;
            return true;
        }
        else return false;        
}

// @brief Get the battery boost voltage percentage endpoint
uint8_t Battery::getBoostPrecentVoltage() {
    return this->battery.boostVoltPrecent;
}
/*


    Battery DS18B20 temp sensor handling. 

    Maybe have some moving avarage sum, where we can get the average temperature over a period of time.
    This is to compare the reading, if it differs from the previous reading, we can have a failsafe.
*/
float Battery::getTemperature() {
    float tempReturn = 0.0f;
    tempReturn = this->battery.temperature;

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
            this->battery.size = size;
            return true;
        }
    else return false;
}

uint8_t Battery::getNominalString() {
    return this->battery.size;
}
/*


    Setter and Getter for battery boost temperature settings. 
        - ESPUI interface for the user to set the battery temperature settings.
*/
uint8_t Battery::getEcoTemp() {
    return this->battery.heater.ecoTemp;
}

bool Battery::setEcoTemp(int ecoTemp) {
    if(ecoTemp > 0 && ecoTemp < 31 && ecoTemp < this->battery.heater.boostTemp) {
        this->battery.heater.ecoTemp = ecoTemp;
        return true;
    }
    else return false;
}
/*


    SetGetter for Temperature boost settings. 
        - ESPUI interface for the user to set the battery temperature settings.
*/
uint8_t Battery::getBoostTemp() {
    return this->battery.heater.boostTemp;
}

bool Battery::setBoostTemp(int boostTemp) {
    if(boostTemp > 9 && boostTemp < 41 && boostTemp > this->battery.heater.ecoTemp) {
        this->battery.heater.boostTemp = boostTemp;
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
bool Battery::activateTemperatureBoost(bool tempBoost) {
    if(tempBoost) {
            preferences.begin("btry", false);
            this->battery.tempBoost = true;
            preferences.putBool("tboost", true);
            preferences.end(); // Close preferences
            return true;
    }
    else if(!tempBoost ) {
        if(preferences.begin("btry", false))
        {
            this->battery.tempBoost = false;
            preferences.putBool("tboost", false);
            preferences.end(); // Close preferences
        }
        else return false;
    return true;
    }
else return false;
}

bool Battery::getActivateTemperatureBoost() {
    if (this->battery.tempBoost)
        return true;
    else
        return false;
}

bool Battery::activateVoltageBoost(bool voltBoost) {
    if(voltBoost) {
        this->battery.voltBoost = true;
        preferences.begin("btry", false);           // Open preferences with namespace "battery_settings"
        preferences.putBool("vboost", true);
        preferences.end();                          // Close preferences
        return true;
    }
    else if (!voltBoost) {
        this->battery.voltBoost = false;
        preferences.begin("btry", false);           // Open preferences with namespace "battery_settings"
        preferences.putBool("vboost", false);
        preferences.end();                          // Close preferences
        return true;
    }
    else return false;
}

bool Battery::getActivateVoltageBoost() {
    if (this->battery.voltBoost)
        return true;
    else
        return false;
}

float Battery::calculateChargeTime(int initialPercentage, int targetPercentage) {
    // Calculate the charge needed in Ah
    float chargeNeeded = this->battery.capct * ((targetPercentage - initialPercentage) / (float)100.0);
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
        this->battery.charger.current = chargerCurrent;
        return true;
    }
    else return false;
}

// Getter for charger current
int Battery::getCharger() {
    return this->battery.charger.current;
}

// Setter for battery capacity
bool Battery::setCapacity(int capacity) {
    if (capacity > 0 && capacity < 250) {
        this->battery.capct = capacity;
        return true;
    } else {
        return false;
    }
}

// Getter for battery capacity
int Battery::getCapacity()  {
    return this->battery.capct;
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

    // Tarkistetaan, ylittääkö jäännös 5% kennojännitteestä
    if (remainder_mV * toleranceDivisor > voltagePerCell_mV * toleranceMultiplier) {
        return measuredSeries + 1; // Siirrytään seuraavaan sarjaan
    } else {
        return measuredSeries; // Pysytään samassa sarjassa
    }
}

float Battery::getCurrentVoltage() {
    float volts = 0.0f;
    volts = this->battery.milliVoltage / (float)(1000.0F);  // old, no need?
    return volts;
}

int Battery::getBatteryApprxSize() {
    return this->battery.sizeApprx;
}

void Battery::charger(bool chargerState) {
    if (chargerState) {
        gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_NUM_25, HIGH);
        // Serial.print (" Chrgr: ON ");
        this->battery.chargerState = true;

    } else {

        gpio_set_direction(GPIO_NUM_25, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_NUM_25, LOW);
        /// Serial.print(" Chrgr: OFF ");
        this->battery.chargerState = false;
    }
}

bool Battery::saveHostname(String hostname) {
    preferences.begin("btry", false);
    preferences.putString("myname", hostname);
    preferences.end();
    this->battery.myname = String(hostname);
    return true;
}

bool Battery::loadHostname() {
    preferences.begin("btry", true);
    String hostname = preferences.getString("myname");
    preferences.end();
    this->battery.myname = hostname;
    return true;
}

bool Battery::setResistance(uint8_t resistance) {
    if (resistance > 10 && resistance < 255) {
        this->battery.heater.resistance = resistance;
        return true;
    } else {
        return false;
    }
}

int Battery::getResistance() {
    return this->battery.heater.resistance;
}

void Battery::adjustHeaterSettings() {
    // const float maxPower = 30.0; // Maximum power in watts

    // Calculate the maximum allowable power based on the resistance and voltage
    float power = (this->battery.size * float(3.7) * this->battery.size * float(3.7)  ) / this->battery.heater.resistance;

    // Calculate the duty cycle needed to cap the power at maxPower
    float dutyCycle = this->battery.heater.maxPower / power;

    if (dutyCycle > 1) {
        dutyCycle = 1;
    }

    // Calculate the output limits for QuickPID
    this->battery.heater.maxPower  = (dutyCycle * 255);

    // Ensure the output limit does not exceed the maximum allowable current
    // Ensure the output limit does not exceed the maximum allowable current
    if (this->battery.heater.maxPower > 255) {
        this->battery.heater.maxPower = 254;
    }
    if (this->battery.heater.maxPower < 20) {
        this->battery.heater.maxPower = 20;
    }

    // Adjust the QuickPID output limits
    heaterPID.SetOutputLimits(float(0), float(this->battery.heater.maxPower));
}

bool Battery::setPidP(float p) {
    if (p >= 0 && p < 250) {
        this->battery.heater.pidP = p;

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
    return this->battery.heater.pidP;
}

bool Battery::getChargerStatus() {
    return this->battery.chargerState;
}

void Battery::setupTelegram(const char* token, const char* chatId) {
        // strncpy(battery.telegram.token, token, sizeof(battery.telegram.token)-1);
        // strncpy(battery.telegram.chatId, chatId, sizeof(battery.telegram.chatId)-1);  
        client.setInsecure();  // Skip certificate validation
        telegram.setTelegramToken(token);
        telegram.begin();
        // battery.telegram.enable = true;
    }

void Battery::publishBatteryData() {
        String baseTopic = "battery/" + String(battery.myname) + "/";
        mqtt.publish((baseTopic + "size").c_str(),                 String(this->battery.size).c_str());
        mqtt.publish((baseTopic + "temperature").c_str(),          String(this->battery.temperature).c_str());
        mqtt.publish((baseTopic + "voltageInPrecent").c_str(),     String(this->battery.voltageInPrecent).c_str());
        mqtt.publish((baseTopic + "ecoVoltPrecent").c_str(),       String(this->battery.ecoVoltPrecent).c_str());
        mqtt.publish((baseTopic + "boostVoltPrecent").c_str(),     String(this->battery.boostVoltPrecent).c_str());
        mqtt.publish((baseTopic + "ecoTemp").c_str(),              String(this->battery.heater.ecoTemp).c_str());
        mqtt.publish((baseTopic + "boostTemp").c_str(),            String(this->battery.heater.boostTemp).c_str());
        mqtt.publish((baseTopic + "resistance").c_str(),           String(this->battery.heater.resistance).c_str());
        mqtt.publish((baseTopic + "capct").c_str(),                String(this->battery.capct).c_str());
        mqtt.publish((baseTopic + "chrgr").c_str(),                String(this->battery.charger.current).c_str());
        mqtt.publish((baseTopic + "maxPower").c_str(),             String(this->battery.heater.maxPower).c_str());
        mqtt.publish((baseTopic + "pidP").c_str(),                 String(this->battery.heater.pidP).c_str());
        mqtt.publish((baseTopic + "pidI").c_str(),                 String(this->battery.heater.pidI).c_str());
        mqtt.publish((baseTopic + "pidD").c_str(),                 String(this->battery.heater.pidI).c_str());
        mqtt.publish((baseTopic + "tempBoost").c_str(),            String(this->battery.tempBoost).c_str());
        mqtt.publish((baseTopic + "voltBoost").c_str(),            String(this->battery.voltBoost).c_str());
        // Publish MQTT settings
        mqtt.publish((baseTopic + "mqtt/enable").c_str(),         String(this->battery.mqtt.enable).c_str());
        // Publish Telegram settings
        // mqtt.publish((baseTopic + "telegram/enable/").c_str(), String(battery.telegram.enable).c_str());
        // }
}

void Battery::mqttSetup() {
    preferences.begin("btry", true);
        this->battery.mqtt.enable     = preferences.getBool("mqtten");
        this->battery.mqtt.server     = preferences.getString("mqttip");
        this->battery.mqtt.port       = preferences.getInt("mqttport");
        this->battery.mqtt.username   = preferences.getString("mqttusr");
        this->battery.mqtt.password   = preferences.getString("mqttpass");
    preferences.end();
    Serial.print("mqtt enable: ");
    Serial.println(this->battery.mqtt.enable);
    mqtt.setServer(this->battery.mqtt.server.c_str(), this->battery.mqtt.port);
}

void Battery::handleMqtt() {

    if(this->battery.mqtt.enable) {
        mqtt.loop();
        if (millis() - this->battery.mqtt.lastMessageTime > 10000 ) {
            this->battery.mqtt.lastMessageTime = millis();
            if(!mqtt.connected()) {
                Serial.print("Connecting to MQTT...");
                // Attempt to connect
                if (mqtt.connect("BatteryJesus", this->battery.mqtt.username.c_str(), this->battery.mqtt.password.c_str())) {
                    Serial.println("connected");
                    publishBatteryData();
                } else {
                    Serial.print("failed, rc=");
                    Serial.print(mqtt.state());
                    Serial.println(" try again in 5 mSeconds");
                    delay(5);
                }
            }
            else publishBatteryData();
        }
    }}

bool Battery::getMqttState() {
    return this->battery.mqtt.enable;
}

bool Battery::setMqttState(bool status) {
    if (status) {
        this->battery.mqtt.enable = true;
        return true;
    }
    else {
        this->battery.mqtt.enable = false;
        return true;
    }
    return false;
}


bool Battery::getTelegramEn() {
    return this->battery.telegram.enable;
}

bool Battery::setTelegramEn(bool status) {
    if (status) {
        this->battery.telegram.enable = true;
        return true;
    }
    else {
        this->battery.telegram.enable = false;
        return false;
    }
}


bool Battery::getHttpEn() {
    return this->battery.http.enable;
}

bool Battery::setHttpEn(bool status) {
    if(status) {
        this->battery.http.enable = true;
        return true;
    }
    else {
        this->battery.http.enable = false;
        return false;
    }
}