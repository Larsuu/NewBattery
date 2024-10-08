#include "Battery.h"
#include <Arduino.h>
#include <ESPUI.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <string>
#include <functional>

#if defined(ESP32)
#include <WiFi.h>
#include <ESPmDNS.h>
#else

// esp8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <umm_malloc/umm_heap_select.h>
#ifndef CORE_MOCK
#ifndef MMU_IRAM_HEAP
#warning Try MMU option '2nd heap shared' in 'tools' IDE menu (cf. https://arduino-esp8266.readthedocs.io/en/latest/mmu.html#option-summary)
#warning use decorators: { HeapSelectIram doAllocationsInIRAM; ESPUI.addControl(...) ... } (cf. https://arduino-esp8266.readthedocs.io/en/latest/mmu.html#how-to-select-heap)
#warning then check http://<ip>/heap
#endif // MMU_IRAM_HEAP
#ifndef DEBUG_ESP_OOM
#error on ESP8266 and ESPUI, you must define OOM debug option when developping
#endif
#endif
#endif

#define ENABLE_SERIAL_PRINTS true

#if ENABLE_SERIAL_PRINTS
#define SERIAL_PRINT(x) Serial.print(x)
#define SERIAL_PRINTLN(x) Serial.println(x)
#else
#define SERIAL_PRINT(x)
#define SERIAL_PRINTLN(x)
#endif


//Settings for ESPUI
#define SLOW_BOOT 0
#define HOSTNAME "veijo"
#define FORCE_USE_HOTSPOT 0


int tempest;
int batteryInSeries;
unsigned int mittausmillit = 0;  // battery voltage and heat reading millis()

//UI handles
uint16_t tab1, tab2, tab3;
String clearLabelStyle, switcherLabelStyle;
uint16_t battInfo, permInfo;
uint16_t labelId, APSwitcher, ipLabel, ipText;
uint16_t httpUserAccess, httpUsername, httpPassword;
uint16_t voltLabel, tempLabel;
uint16_t tempBoostSwitcher, voltBoostSwitcher; 
uint16_t wifitab;
uint16_t oldEcoTempLabel, oldBoostTempLabel;  // removable
uint16_t ecoTempLabel, boostTempLabel, ecoTempMemoryLabel, boostTempMemoryLabel;
uint16_t tempsLabels, group1, text1, text2, text3, text4, text5, text6, text7, text8, text9, text10, text11, text12;
uint16_t verticalVoltGroup, VerticalTempGroup, quickControls, horizontalGroup;
uint16_t voltSwitcher, tempSwitcher, infoLabel, timeLabel, timeString, extraLabel, ultraLabel, chargerTimespan, quickPanelVoltage;

//UI handles
uint16_t wifi_ssid_text, wifi_pass_text;
uint16_t graph;
volatile bool updates = false;

//ESPUI Configs page integers:
uint16_t mainSelector, permSwitcher;
uint16_t ecoTempNum, ecoVoltNum;
uint16_t boostTemp, boostVolts, nameLabel, hostnameLabel, ecoVoltLabel, ecoVoltUnitLabel, boostVoltUnitLabel, boostVoltLabel;
uint16_t saveButton;
uint16_t textSeriesConfig, seriesConfigNum, vertgroupswitcher, chargerTimeFeedback;


const std::string CLEAR_LABEL_STYLE = "background-color: unset; width: 100%;";
const std::string SWITCHER_LABEL_STYLE = "width: 25%; background-color: unset;";
const std::string LABEL_STYLE = "font-family: serif; background-color: unset; width: 100%; text-align: left;";
const std::string BUTTON_STYLE = "background-color: #d3d3d3; width: 20%; text-align: center; font-size: medium; font-family: serif; margin-top: 20px; margin-bottom: 20px; border-radius: 15px;";

Battery batt;

//Function Prototypes for ESPUI
void connectWifi();
void setUpUI();
void textCallback(Control *sender, int type);
void randomString(char *buf, int len);
void paramCallback(Control* sender, int type, int param);
void generalCallback(Control *sender, int type);


// Battery callbacks for Number input  --> Voltage
void ecoVoltCallback(Control *sender, int type);
void boostVoltCallback(Control *sender, int type);

// Battery callbacks for Number input  --> Temperature
void ecoTempCallback(Control *sender, int type);
void boostTempCallback(Control *sender, int type);

// Battery callbacks for Switcher input  --> Boost & Temp
void boostTempSwitcherCallback(Control *sender, int type);
void boostVoltageSwitcherCallback(Control *sender, int type);

// Select a batterys size 
void selectBattery(Control *sender, int type);


// Function to apply style and optionally a callback to a control
void applyStyle(uint16_t controlID, const std::string& style, std::function<void(Control*, int)> callback = nullptr) {
    ESPUI.setElementStyle(controlID, style.c_str());
    if (callback) {
        ESPUI.addControlCallback(controlID, callback);
    }
}

void setUpUI() {


  ESPUI.setVerbosity(Verbosity::VerboseJSON);
  SERIAL_PRINTLN(WiFi.localIP());
  ESPUI.captivePortal = true;
  ESPUI.sliderContinuous = false;



    auto tab1 = ESPUI.addControl(Tab, "Info", "Info");
    auto tab2 = ESPUI.addControl(Tab, "Setup", "Setup");
    auto tab3 = ESPUI.addControl(Tab, "Access", "Access");
  /*
   * Tab: Basic Controls
   * This tab contains all the basic ESPUI controls, and shows how to read and update them at runtime.
   *-----------------------------------------------------------------------------------------------------------*/
  


  
  clearLabelStyle = "background-color: unset; width: 100%;";
  switcherLabelStyle = "width: 25%; background-color: unset;";

  // ESPUI.addControl(Separator, "Quick controls", "", None);
  auto vertgroupswitcher = ESPUI.addControl(Label, "Temp Boost & Voltage Boost", "", Peterriver);
  ESPUI.setVertical(vertgroupswitcher); 

  ESPUI.setElementStyle(vertgroupswitcher, "background-color: unset;");

  chargerTimeFeedback = ESPUI.addControl(Label, "Charger Time", "0.00", None, vertgroupswitcher);
                        ESPUI.setElementStyle(chargerTimeFeedback, switcherLabelStyle);

  tempLabel =  ESPUI.addControl(Label, "TempLabel", "0", None, vertgroupswitcher);
                ESPUI.setElementStyle(tempLabel , switcherLabelStyle);
    

  voltLabel =   ESPUI.addControl(Label, "VoltLabel", "0", None, vertgroupswitcher);
                ESPUI.setElementStyle(voltLabel, switcherLabelStyle);

  quickPanelVoltage = ESPUI.addControl(Label, "QuickbtrToVolts", "99.9", None, vertgroupswitcher);  // NewlineLabel
                      ESPUI.setElementStyle(quickPanelVoltage, switcherLabelStyle);

  ESPUI.setElementStyle(ESPUI.addControl(Label, "emptyLine", "", None, vertgroupswitcher), clearLabelStyle);  // NewlineLabel
  

  extraLabel = ESPUI.addControl(Label, "ExtraLabel", "1", None, vertgroupswitcher); 
                ESPUI.setVertical(extraLabel);
                ESPUI.setElementStyle(extraLabel, switcherLabelStyle);


  voltSwitcher =  ESPUI.addControl(Switcher, "", "0", None, vertgroupswitcher, boostVoltageSwitcherCallback);
                  // ESPUI.setElementStyle(voltSwitcher, switcherLabelStyle);
                  ESPUI.setVertical(voltSwitcher);

  tempSwitcher  = ESPUI.addControl(Switcher, "", "0", None, vertgroupswitcher, boostTempSwitcherCallback);
                  // ESPUI.setElementStyle(tempSwitcher, switcherLabelStyle);
                  ESPUI.setVertical(tempSwitcher);

  ultraLabel = ESPUI.addControl(Label, "", "0", None, vertgroupswitcher);  // NewlineLabel
              ESPUI.setVertical(ultraLabel);
              ESPUI.setElementStyle(ultraLabel, switcherLabelStyle);


  // ESPUI.setVertical(ESPUI.addControl(Switcher, "", "0", None, vertgroupswitcher, boostTempSwitcherCallback));
  


  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, vertgroupswitcher), clearLabelStyle);  // NewlineLabel

  // chargerTimeFeedback = ESPUI.addControl(Label, "Charger Time", "0.00", None, vertgroupswitcher);

  // ESPUI.setVertical(chargerTimeFeedback);

  // ESPUI.addControl(Separator, "", "", None);
  
 /*
  * First tab
  *_____________________________________________________________________________________________________________*/

  /* auto configs = ESPUI.addControl(Tab, "", "Configuration"); */

  infoLabel = ESPUI.addControl(Label, "How To Use", "Battery Assistant<br><br>How To:<br><br>For initial setup, the most important thing is to define your battery configuration. Is your battery a 7S or 16S defines how your charger is controlled.<ul><li>7S is 24V</li><li>10S is 36V</li><li>13S is 48V</li><li>16S is 60V</li><li>20S is 72V</li></ul><p>Get to know how to use this even more from the link below</p>", Peterriver, tab1, textCallback);
  ESPUI.setElementStyle(infoLabel, "font-family: serif; background-color: unset; width: 100%; text-align: left;");
  labelId = ESPUI.addControl(Label, "Uptime:", "", None, tab1);
  ESPUI.setElementStyle(labelId, "font-family: serif; background-color: unset; width: 100%; text-align: left;");
  ipText = ESPUI.addControl(Label, "IP", "ipAddress", ControlColor::None, tab1);
 







group1      = ESPUI.addControl(Label, "Battery Settings", "", Turquoise, tab2);
              ESPUI.setElementStyle(group1, "background-color: unset; width: 100%; color: black; font-size: medium;");

    

    nameLabel =  ESPUI.addControl(Text, "Hostname ", "Veijo", Alizarin, group1, generalCallback);
                    ESPUI.setElementStyle(nameLabel, "background-color: unset; width: 50%; text-align: center; font-size: medium;");
                    ESPUI.setElementStyle(ESPUI.addControl(Label, "veijo.local", "URL:  veijo.local ", Alizarin, nameLabel), "background-color: unset; width: 100%; text-align: left;");
                    ESPUI.setElementStyle(ESPUI.addControl(Label, "ip", "IP: ", Alizarin, nameLabel), "background-color: unset; width: 20%; text-align: left;");
                    ESPUI.setElementStyle( ipLabel = ESPUI.addControl(Label, "", "IP", None, nameLabel), "background-color: unset; width: 80%; text-align: left;");
                    ESPUI.setElementStyle(nameLabel, "font-size: x-large; font-family: serif; width: 100%; text-align: center;");



text8 = ESPUI.addControl(Label, "Chrgr", "Battery Charger Info", Emerald, group1);
ESPUI.setElementStyle(text8, "text-align: left; font-size: small; font-family: serif; width: 100%; background-color: unset; color: black; margin-top: 20px; border-bottom: 1px solid darkslategray; padding-bottom: 5px;");


text9          =  ESPUI.addControl(Label, "Battery Charger", "Charger (A)", None, group1);    
                  ESPUI.setElementStyle(text9, "background-color: unset; width: 50%; text-align: left; font-size: small; color: darkslategray;");


text10 =      ESPUI.addControl(Number, "Charger Settings ", "2", Emerald, group1, generalCallback);
              ESPUI.addControl(Min, "", "1", None, text10);
              ESPUI.addControl(Max, "", "2", None, text10);
              ESPUI.setElementStyle(text10, "background-color: transparent; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield; width: 25%;");

              ESPUI.setElementStyle(ESPUI.addControl(Label, "A", "A", Emerald, group1), "font-size: medium; font-family: serif; width: 25%; text-align: left; background-color: unset; color: darkslategray;");



text11          = ESPUI.addControl(Label, "Battery Capacity", "Charger (Ah)", None, group1);    
                  ESPUI.setElementStyle(text11, "background-color: unset; width: 50%; text-align: left; font-size: small; color: darkslategray;");


text12 =      ESPUI.addControl(Number, "Capacity Settings ", "2", Emerald, group1, generalCallback);
              ESPUI.addControl(Min, "", "1", None, text12);
              ESPUI.addControl(Max, "", "2", None, text12);
              ESPUI.setElementStyle(text12, "background-color: transparent; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield; width: 25%;");

              ESPUI.setElementStyle(ESPUI.addControl(Label, "A", "Ah", Emerald, group1), "font-size: medium; font-family: serif; width: 25%; text-align: left; background-color: unset; color: darkslategray;");



text7 = ESPUI.addControl(Label, "Tempsis", "Batterys Nominal Value", Emerald, group1);
ESPUI.setElementStyle(text7, "text-align: left; font-size: small; font-family: serif; width: 100%; background-color: unset; color: black; margin-top: 20px; border-bottom: 1px solid darkslategray; padding-bottom: 5px;");


// Add Battery Series Configuration
textSeriesConfig = ESPUI.addControl(Label, "Battery Series Configuration", "Series Config", None, group1);
ESPUI.setElementStyle(textSeriesConfig, "background-color: unset; width: 50%; text-align: left; font-size: small; color: darkslategray;");

seriesConfigNum = ESPUI.addControl(Number, "Series Configuration", "7", Emerald, group1, selectBattery);
ESPUI.addControl(Min, "", "7", None, seriesConfigNum);
ESPUI.addControl(Max, "", "21", None, seriesConfigNum);
ESPUI.setElementStyle(seriesConfigNum, "background-color: transparent; width: 25%; border: none; text-align: center; ; -webkit-appearance: none; -moz-appearance: textfield;");

 ESPUI.setElementStyle(ESPUI.addControl(Label, "C", "Strings", Emerald, group1), "font-family: serif; width: 25%; text-align: center; background-color: unset;color: darkslategray;");



text6 = ESPUI.addControl(Label, "Tempsis", "Temperature Settings", Emerald, group1);
ESPUI.setElementStyle(text6, "text-align: left; font-size: small; font-family: serif; width: 100%; background-color: unset; color: black; margin-top: 20px; border-bottom: 1px solid darkslategray; padding-bottom: 5px;");


text1          =  ESPUI.addControl(Label, "Economy Temperature", "Eco temp", None, group1);    
                  ESPUI.setElementStyle(text1, "background-color: unset; width: 50%; text-align: left; font-size: small; color: darkslategray;");


ecoTempNum =  ESPUI.addControl(Number, "Temperature settings ", "0", Emerald, group1, ecoTempCallback);
              ESPUI.addControl(Min, "", "1", None, ecoTempNum);
              ESPUI.addControl(Max, "", "35", None, ecoTempNum);
              ESPUI.setElementStyle(ecoTempNum, "background-color: transparent; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield; width: 25%;");

              ESPUI.setElementStyle(ESPUI.addControl(Label, "C", "℃", Emerald, group1), "font-size: medium; font-family: serif; width: 25%; text-align: left; background-color: unset; color: darkslategray;");


text2       = ESPUI.addControl(Label, "Boost temp", "Boost temp", None, group1);
              ESPUI.setElementStyle(text2, "background-color: unset; width: 50%; text-align: left; color: darkslategray; font-size: small;");

boostTemp = ESPUI.addControl(Number, "Boost temp        'C", "25", Turquoise, group1, boostTempCallback);
            ESPUI.addControl(Min, "", "1", None, boostTemp);
            ESPUI.addControl(Max, "", "35", None, boostTemp);
            ESPUI.setElementStyle(boostTemp, "background-color: transparent; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield; width: 25%;");
            
            ESPUI.setElementStyle(ESPUI.addControl(Label, "C", "℃", Emerald, group1), "font-family: serif; width: 25%; text-align: left; background-color: unset;color: darkslategray;");

/*
text3 =      ESPUI.addControl(Label, "Voltage Settings", "Voltage Settings", Emerald, group1);
            ESPUI.setElementStyle(text3, "text-align: left; font-size: x-small; font-family: serif; width: 100%; background-color: unset; color: black; margin-top: 20px;");  

ESPUI.setElementStyle(ESPUI.addControl(Separator, "Voltage Settings", "Volts", None, group1), "font-family: serif; width: 50%; text-align: left; background-color: unset; color: darkslategray;");
*/

text3 = ESPUI.addControl(Label, "Voltage Settings", "Voltage Settings", Emerald, group1);
ESPUI.setElementStyle(text3, "text-align: left; font-size: small; font-family: serif; width: 100%; background-color: unset; color: black; margin-top: 20px; border-bottom: 1px solid darkslategray; padding-bottom: 5px;");



// ESPUI.addControl(Separator, "Voltage Settings", "", None, tab2);

// Economy Charging text
text4 = ESPUI.addControl(Label, "Eco Charge", "Eco Charge", None, group1);
        ESPUI.setElementStyle(text4, "background-color: unset; width: 50%; text-align: left; font-size: small; color: darkslategray;");

// economy charging precent value in a number box
ecoVoltNum = ESPUI.addControl(Number, "Eco Charge", "70", Emerald, group1, ecoVoltCallback);
ESPUI.addControl(Min, "", "50", None, ecoVoltNum);
ESPUI.addControl(Max, "", "100", None, ecoVoltNum);
ESPUI.setElementStyle(ecoVoltNum, "background-color: transparent; border: none; font-size: medium; text-align: center; -webkit-appearance: none; -moz-appearance: textfield; width: 25%;");

// @brief add a precent label
ESPUI.setElementStyle(ecoVoltUnitLabel = ESPUI.addControl(Label, "precent", "%", Emerald, group1), "font-size: small; font-family: serif; width: 7%; background-color: unset; text-align: left; color: darkslategray;");

// placeholder for the udated value
ESPUI.setElementStyle(ecoVoltLabel = ESPUI.addControl(Label, "Volts", "0", Emerald, group1), "text-align: center; font-size: small; font-family: serif; width: 18%; background-color: unset; color: darkslategray;");

// ECO value UNIT
// ESPUI.setElementStyle(ESPUI.addControl(Label, "", "V", None, group1), "font-size: x-small; font-family: serif; width: 3%; background-color: unset; text-align: right; color: darkslategray;");

// Boost Charge text
text5 = ESPUI.addControl(Label, "Boost Charge", "Boost Charge", None, group1);
        ESPUI.setElementStyle(text5, "background-color: unset; width: 50%; text-align: left; font-size: small; color: darkslategray;");

boostVolts = ESPUI.addControl(Number, "Boost Charge", "90", Turquoise, group1, boostVoltCallback);
ESPUI.addControl(Min, "", "60", None, boostVolts);
ESPUI.addControl(Max, "", "100", None, boostVolts);
ESPUI.setElementStyle(boostVolts, "background-color: transparent; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield; width: 25%;");

ESPUI.setElementStyle(boostVoltUnitLabel = ESPUI.addControl(Label, "precent", "%", Emerald, group1), "font-size: small; font-family: serif; width: 7%; background-color: unset; text-align: left; color: darkslategray;");

ESPUI.setElementStyle(boostVoltLabel = ESPUI.addControl(Label, "Volts", "0", Emerald, group1), "text-align: center; font-size: small; font-family: serif; width: 18%; background-color: unset; color: darkslategray;");

// ESPUI.setElementStyle(ESPUI.addControl(Label, "", "V", None, group1), "font-size: x-small; font-family: serif; width: 3%; background-color: unset; text-align: right; color: darkslategray;");

chargerTimespan = ESPUI.addControl(Label, "Charger Time", "0.00", None, group1);
ESPUI.setElementStyle(chargerTimespan, "color: black; background-color: unset; width: 100%; text-align: right; font-size: medium; font-family: serif; margin-right: 15px; margin-top: 5px; border-top: 1px solid darkslategray; padding-bottom: 5px;");

text7 = ESPUI.addControl(Label, "Perms", "Save to memory", Emerald, group1);
ESPUI.setElementStyle(text7, "text-align: left; font-size: small; font-family: serif; width: 100%; background-color: unset; color: black; margin-top: 20px; border-bottom: 1px solid darkslategray; padding-bottom: 5px;");

// Save to EEPROM
// ESPUI.addControl(Separator, "Mem", "Memory", None, tab2);

saveButton = ESPUI.addControl(Button, "Memory", "Save", None, group1, [](Control *sender, int type) {
    if (type == B_UP) {
        Serial.println("Saving battery structs...");
        Serial.println(ESPUI.getControl(ecoTempNum)->value);
        Serial.println(ESPUI.getControl(ecoVoltNum)->value);
        Serial.println(ESPUI.getControl(boostTemp)->value);
        Serial.println(ESPUI.getControl(boostVolts)->value);
        Serial.println(ESPUI.getControl(seriesConfigNum)->value);
        Serial.println(ESPUI.getControl(text10)->value);
        Serial.println(ESPUI.getControl(text12)->value);

        if (batt.setNominalString(ESPUI.getControl(seriesConfigNum)->value.toInt()) &&
          batt.setEcoPrecentVoltage(ESPUI.getControl(ecoVoltNum)->value.toInt()) &&
          batt.setEcoTemp(ESPUI.getControl(ecoTempNum)->value.toInt()) &&
          batt.setBoostPrecentVoltage(ESPUI.getControl(boostVolts)->value.toInt()) &&
          batt.setBoostTemp(ESPUI.getControl(boostTemp)->value.toInt()) &&
          batt.setCharger(ESPUI.getControl(text10)->value.toInt()) &&
          batt.setCapacity(ESPUI.getControl(text12)->value.toInt())) {

          Serial.println("All ok! Saving settings to memory...");  
          batt.saveSettings();
        } else {
          Serial.println("Error: Invalid input");
        }

        Serial.println("Saved in memory...");
        Serial.println(batt.getNominalString());
        Serial.println(batt.getEcoPrecentVoltage());
        Serial.println(batt.getEcoTemp());
        Serial.println(batt.getBoostPrecentVoltage());
        Serial.println(batt.getBoostTemp());
        Serial.println(batt.getCharger());
        Serial.println(batt.getCapacity());

    }
});

ESPUI.setElementStyle(saveButton, "background-color: #d3d3d3; width: 20%; text-align: center; font-size: medium; font-family: serif; margin-top: 20px; margin-bottom: 20px; border-radius: 15px;");


  APSwitcher = ESPUI.addControl(Switcher, "Run only as Access Point", "", None, tab3, generalCallback);

  httpUserAccess =  ESPUI.addControl(Label, "User Access for LAN", "HttpAccess", Peterriver, tab3, generalCallback);
  httpUsername =    ESPUI.addControl(Text, "Username", "", Dark, httpUserAccess, textCallback);
  httpPassword =    ESPUI.addControl(Text, "Password", "", Dark, httpUserAccess, textCallback);

  ESPUI.setElementStyle(httpUserAccess, "font-family: serif; background-color: unset;");

  /*
   
   
    Tab: WiFi Credentials
    You use this tab to enter the SSID and password of a wifi network to autoconnect to.
   
   
   *-----------------------------------------------------------------------------------------------------------*/

  wifitab = ESPUI.addControl(Tab, "", "WiFi");
  wifi_ssid_text = ESPUI.addControl(Text, "SSID", "", Alizarin, wifitab, textCallback);
  //Note that adding a "Max" control to a text control sets the max length
  ESPUI.addControl(Max, "", "32", None, wifi_ssid_text);
  wifi_pass_text = ESPUI.addControl(Text, "Password", "", Alizarin, wifitab, textCallback);
  ESPUI.addControl(Max, "", "64", None, wifi_pass_text);
  ESPUI.addControl(Button, "Save", "Save", Peterriver, wifitab,
      [](Control *sender, int type) {
      if(type == B_UP) {
        Serial.println("Saving credentials to EPROM...");
        Serial.println(ESPUI.getControl(wifi_ssid_text)->value);
        Serial.println(ESPUI.getControl(wifi_pass_text)->value);
        unsigned int i;
        EEPROM.begin(100);
        for(i = 0; i < ESPUI.getControl(wifi_ssid_text)->value.length(); i++) {
          EEPROM.write(i, ESPUI.getControl(wifi_ssid_text)->value.charAt(i));
          if(i==30) break; //Even though we provided a max length, user input should never be trusted
        }
        EEPROM.write(i, '\0');

        for(i = 0; i < ESPUI.getControl(wifi_pass_text)->value.length(); i++) {
          EEPROM.write(i + 32, ESPUI.getControl(wifi_pass_text)->value.charAt(i));
          if(i==94) break; //Even though we provided a max length, user input should never be trusted
        }
        EEPROM.write(i + 32, '\0');
        EEPROM.end();
      }
      });



    ESPUI.updateControlValue(ecoTempNum, String(batt.getEcoTemp()));
    ESPUI.updateControlValue(ecoVoltNum, String(batt.getEcoPrecentVoltage()));
    ESPUI.updateControlValue(boostTemp, String(batt.getBoostTemp()));
    ESPUI.updateControlValue(boostVolts, String(batt.getBoostPrecentVoltage()));
    ESPUI.updateControlValue(seriesConfigNum, String(batt.getNominalString()));
    ESPUI.updateControlValue(ecoVoltLabel, String(batt.btryToVoltage(batt.getEcoPrecentVoltage()), 1) + " V");
    ESPUI.updateControlValue(boostVoltLabel, String(batt.btryToVoltage(batt.getBoostPrecentVoltage()), 1) + " V");
    ESPUI.updateControlValue(text10, String(batt.getCharger()));
    ESPUI.updateControlValue(text12, String(batt.getCapacity()));
    ESPUI.updateLabel(chargerTimespan, String(batt.calculateChargeTime(batt.getEcoPrecentVoltage(), batt.getBoostPrecentVoltage()), 2) + " h");



  //Add a callback to the main selector to print the selected value
  //This is a good example of how to use the "param" argument in the callback
  //to pass additional information to the callback function.
  //In this case, we pass the ID of the control that we want to update.
  // ESPUI.getControl(mainSelector)->callback = selectBattery;
  // ESPUI.getControl(mainSelector)->param = mainSelector;



  //Finally, start up the UI.
  //This should only be called once we are connected to WiFi.
  ESPUI.begin(HOSTNAME);
}

//Most elements in this test UI are assigned this generic callback which prints some
//basic information. Event types are defined in ESPUI.h
void generalCallback(Control *sender, int type) {
  Serial.print("CB: id(");
  Serial.print(sender->id);
  Serial.print(") Type(");
  Serial.print(type);
  Serial.print(") '");
  Serial.print(sender->label);
  Serial.print("' = ");
  Serial.println(sender->value);
}
/*







    EcoVoltage Iumber Field - update to Voltage
*/
void ecoVoltCallback(Control* sender, int type) {

  // ESPUI.updateLabel(ecoVoltLabel, String(batt.getEcoPrecentVoltage(), 0));

   switch (type)
    {
    case 9:
		if(sender->value.toInt() != 0)
		{
			if(batt.setEcoPrecentVoltage(sender->value.toInt()))
        {
          ESPUI.updateLabel(ecoVoltLabel, String(batt.btryToVoltage(sender->value.toInt()), 2));
			   	Serial.print("Ecomode Voltage set to: ");
          Serial.print(batt.getEcoPrecentVoltage());
          Serial.println(" Precent");
        }
      else
      {
          ESPUI.updateLabel(ecoVoltLabel, String(batt.getEcoPrecentVoltage(), 2));
			   	Serial.print("Ecomode Voltage set to: ");
          Serial.print(batt.getEcoPrecentVoltage());
          Serial.println(" Precent");
      }
		break;

	case S_INACTIVE:
		Serial.println("Ecomode Inactive");
		break;
    }
  }
  // ESPUI.updateLabel(boostVoltLabel, String(batt.btryToVoltage(sender->value.toInt()), 2));




  // ESPUI.updateLabel(ecoVoltLabel, String(batt.btryToVoltage(sender->value.toInt()), 2));

}
void selectBattery(Control* sender, int type) {
	int current = batt.getNominalString();

    switch (sender->value.toInt())
    {
    case 7:
        Serial.print("7S Battery selected");
        break;

    case 10:
        Serial.print("10S Battery selected");
        break;

    case 12:
        Serial.print("12S Battery selected");
        break;
    case 13:
        Serial.print("13S Battery selected");
        break;

    case 14:
        Serial.print("14S Battery selected");
        break;

    case 16:
        Serial.print("16S Battery selected");
        break;

    case 20:
        Serial.print("20S Battery selected");
        break;
	default: 
		current:
		Serial.print("new Battery selected");
		break;
    }

    String battEmpty = "<br><br>      Empty             0%   =   " + String((sender->value.toInt() * (float)3 ), 1);
    String battNom   = "    <br>      Nominal        50%   =   " + String((sender->value.toInt() * (float)3.7 ), 1);
    String battFull =  "    <br>      Full              100%   =  " +  String((sender->value.toInt() * (float)4.2 ), 1);      
    String allBatt = battEmpty + battNom + battFull;
    ESPUI.updateLabel(battInfo, allBatt);



  Serial.print("CB: id(");
  Serial.print(sender->id);
  Serial.print(") Type(");
  Serial.print(type);
  Serial.print(") '");
  Serial.print(sender->label);
  Serial.print("' = ");
  Serial.println(sender->value);

}
/*







	The numbers value button, for defining a boost voltage level in voltage
*/
void boostVoltCallback(Control* sender, int type) {
    // ESPUI.updateLabel(boostVoltLabel, String(batt.getBoostPrecentVoltage(), 0));


    switch (type) {
    case 9:
		if(sender->value.toInt() != 0)
		{
			if(batt.setBoostPrecentVoltage(sender->value.toInt()))
        {
          int bootVoltLabel = sender->value.toInt();
          Serial.print("Boost Voltage set to: ");
          Serial.print(bootVoltLabel);
          Serial.print("  ");
          Serial.print(batt.btryToVoltage(sender->value.toInt()));
          ESPUI.updateLabel(boostVoltLabel, String(batt.btryToVoltage(sender->value.toInt()), 1));
			   	Serial.print("Boost VoltagePRecent set to: ");
          Serial.print(batt.getBoostPrecentVoltage());
          Serial.println(" V");
        }
      else 
      {
          ESPUI.updateLabel(boostVoltLabel, String(batt.getBoostPrecentVoltage(), 2));
         	Serial.print("Boost VoltagePrecent set to: ");
          Serial.print(batt.getBoostPrecentVoltage());
          Serial.println(" V");}
		}
		break;

	case S_INACTIVE:
		Serial.println("Ecomode Inactive");
		break;
    }

  // ESPUI.updateLabel(boostVoltLabel, String(batt.btryToVoltage(sender->value.toInt()), 2));

  // ESPUI.updateLabel(boostVoltLabel, String(batt.getNominalString(), 0));

}

/*




ESPUI.setElementStyle(oldEcoTempLabel = ESPUI.addControl(Label, "Volts", "0", Emerald, ecoVoltNum), " text-align: center; font-size: large; font-family: serif; width: 70px; background-color: unset;"); 



*/
void ecoTempCallback(Control *sender, int type) {
  switch (type) {
    case 9:
		if(sender->value.toInt() != 0)
		{
			if(batt.setEcoTemp(sender->value.toInt()))
        {
			   	Serial.print("Ecomode Temp set to: ");
          Serial.print(batt.getEcoTemp());
          Serial.println(" C");
          // ESPUI.updateLabel(oldEcoTempLabel, String(batt.getEcoTemp(), 0));
          
        }
      else
       {
        	Serial.print("Ecomode Failed: ");
          Serial.print(batt.getEcoTemp());
          Serial.println(" C");
       }
		}
		break;
	case S_INACTIVE:
		Serial.println("Ecomode Inactive");
		break;
  }
}
/*







	Temperature Boost Switch

*/
void boostTempCallback(Control *sender, int type) {
  switch (type) {
    case 9:
		if(sender->value.toInt() != 0)
		{
			if(batt.setBoostTemp(sender->value.toInt()))
        {
			   	Serial.print("Boost Temp set to: ");
          Serial.print(batt.getBoostTemp());
          Serial.println(" C");
          // ESPUI.updateLabel(oldBoostTempLabel, String(batt.getBoostTemp(), 0));
        }
      else
      {
         	Serial.print("Boost Temp set to: ");
          Serial.print(batt.getBoostTemp());
          Serial.println(" C");
		  }
    }
		break;
	case S_INACTIVE:
		Serial.println("Boost Temp Inactive");
		break;
  }
}
/*








*/
void boostVoltageSwitcherCallback(Control *sender, int type) {
    switch (type)  {
    case S_ACTIVE:
			if(batt.activateVoltageBoost(true));
			  Serial.print("Voltage Boost Activated");
		break;

	case S_INACTIVE:
      if(batt.activateVoltageBoost(false));
		    Serial.println("Voltage Boost Inactive");
		break;
    }
}
/*







	Temperature Boost Switch on the Front Page's Quick Controls

*/
void  boostTempSwitcherCallback(Control *sender, int type) {
    switch (type)    {
    case S_ACTIVE:
	if(batt.activateTemperatureBoost(true))
        Serial.print("Temperature Boost Active:");
        break;

    case S_INACTIVE:
        if(batt.activateTemperatureBoost(false))
        	Serial.println("Temperature Boost Inactive");
		else
        	Serial.println("Failed");
        break;
    }
}


// Most elements in this test UI are assigned this generic callback which prints some
// basic information. Event types are defined in ESPUI.h
// The extended param can be used to pass additional information
void paramCallback(Control* sender, int type, int param)
{
    Serial.print("CB: id(");
    Serial.print(sender->id);
    Serial.print(") Type(");
    Serial.print(type);
    Serial.print(") '");
    Serial.print(sender->label);
    Serial.print("' = ");
    Serial.println(sender->value);
    Serial.print("param = ");
    Serial.println(param);
}

void setup() {
  
    batt.setup();
    randomSeed(0);
	  Serial.begin(115200);

#if defined(ESP32)
    WiFi.setHostname(HOSTNAME);
#else
    WiFi.hostname(hostname);
#endif
	while(!Serial);
	if(SLOW_BOOT) delay(5000); //Delay booting to give time to connect a serial monitor
	connectWifi();
	#if defined(ESP32)
		WiFi.setSleep(true); //For the ESP32: turn off sleeping to increase UI responsivness (at the cost of power use)
	#endif

    // dnsServer.start(DNS_PORT, "*", apIP);

    Serial.println("\n\nWiFi parameters:");
    Serial.print("Mode: ");
    Serial.println(WiFi.getMode() == WIFI_AP ? "Station" : "Client");
    Serial.print("IP address: ");
    Serial.println(WiFi.getMode() == WIFI_AP ? WiFi.softAPIP() : WiFi.localIP());


  batt.loadSettings();  // needs to be before setUpUI!! 

	setUpUI();


}   

void loop() {

  batt.loop(); // Add battery.loop() here

  if(millis() - mittausmillit >= 10000)
  {
      mittausmillit = millis();

      String wlanIpAddress;
      wlanIpAddress = WiFi.localIP().toString();
      ESPUI.updateLabel(ipText, wlanIpAddress);
      ESPUI.updateLabel(ipLabel, wlanIpAddress);
      ESPUI.updateLabel(labelId, String((millis() / 60000), 1));

      ESPUI.updateLabel(voltLabel, String(batt.getBatteryDODprecent()) + " %");
      
      ESPUI.updateLabel(tempLabel, String(batt.getTemperature(), 1) + " ℃");

      ESPUI.updateLabel(chargerTimeFeedback, String(batt.calculateChargeTime(batt.getEcoPrecentVoltage(), batt.getBoostPrecentVoltage()), 0) + " h");

      ESPUI.updateLabel(boostVoltLabel, String(batt.btryToVoltage(batt.getBoostPrecentVoltage()), 0) + " V");

      ESPUI.updateLabel(quickPanelVoltage, String(batt.accuVolts(), 1) + " V");

      Serial.print(" loop: ");
      Serial.println(batt.accuVolts());


    }

}

void readStringFromEEPROM(String& buf, int baseaddress, int size) {
	buf.reserve(size);
	for (int i = baseaddress; i < baseaddress+size; i++) {
		char c = EEPROM.read(i);
		buf += c;
		if(!c) break;
	}	
}

void connectWifi() {
	int connect_timeout;

#if defined(ESP32)
	WiFi.setHostname(HOSTNAME);
#else
	WiFi.hostname(HOSTNAME);
#endif
	Serial.println("Begin wifi...");

	//Load credentials from EEPROM 
	if(!(FORCE_USE_HOTSPOT)) {
		yield();
		EEPROM.begin(100);
		String stored_ssid, stored_pass;
		readStringFromEEPROM(stored_ssid, 0, 32);
		readStringFromEEPROM(stored_pass, 32, 96);
		EEPROM.end();
	
		//Try to connect with stored credentials, fire up an access point if they don't work.
		#if defined(ESP32)
			WiFi.begin(stored_ssid.c_str(), stored_pass.c_str());
		#else
			WiFi.begin(stored_ssid, stored_pass);
		#endif
		connect_timeout = 28; //7 seconds
		while (WiFi.status() != WL_CONNECTED && connect_timeout > 0) {
			delay(250);
			Serial.print(".");
			connect_timeout--;
		}
	}
	
	if (WiFi.status() == WL_CONNECTED) {
		Serial.println(WiFi.localIP());
		Serial.println("Wifi started");

		if (!MDNS.begin(HOSTNAME)) {
			Serial.println("Error setting up MDNS responder!");
		}
	} else {
		Serial.println("\nCreating access point...");
		WiFi.mode(WIFI_AP);
		WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
		WiFi.softAP(HOSTNAME);

		connect_timeout = 20;
		do {
			delay(250);
			Serial.print(",");
			connect_timeout--;
		} while(connect_timeout);
	}
}

void enterWifiDetailsCallback(Control *sender, int type) {
	if(type == B_UP) {
		Serial.println("Saving credentials to EPROM...");
		Serial.println(ESPUI.getControl(wifi_ssid_text)->value);
		Serial.println(ESPUI.getControl(wifi_pass_text)->value);
		unsigned int i;
		EEPROM.begin(100);
		for(i = 0; i < ESPUI.getControl(wifi_ssid_text)->value.length(); i++) {
			EEPROM.write(i, ESPUI.getControl(wifi_ssid_text)->value.charAt(i));
			if(i==30) break; //Even though we provided a max length, user input should never be trusted
		}
		EEPROM.write(i, '\0');

		for(i = 0; i < ESPUI.getControl(wifi_pass_text)->value.length(); i++) {
			EEPROM.write(i + 32, ESPUI.getControl(wifi_pass_text)->value.charAt(i));
			if(i==94) break; //Even though we provided a max length, user input should never be trusted
		}
		EEPROM.write(i + 32, '\0');
		EEPROM.end();
	}
}

void textCallback(Control *sender, int type) {
	//This callback is needed to handle the changed values, even though it doesn't do anything itself.
}

void randomString(char *buf, int len) {
	for(auto i = 0; i < len-1; i++) 
		buf[i] = random(0, 26) + 'A';
	buf[len-1] = '\0';
}