#include "Battery.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <string>
#include <functional>
#include <Preferences.h>

#if defined(ESP32)
#include <WiFi.h>
#include <ESPmDNS.h>
#else

#include <ESPUI.h>
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

/*
#define ENABLE_SERIAL_PRINTS true

#if ENABLE_SERIAL_PRINTS
#define SERIAL_PRINT(x) Serial.print(x)
#define SERIAL_PRINTLN(x) Serial.println(x)
#else
#define SERIAL_PRINT(x)
#define SERIAL_PRINTLN(x)
#endif

*/


//Settings for ESPUI
#define SLOW_BOOT 0
#define HOSTNAME "Helmi"
#define FORCE_USE_HOTSPOT 0
#define LOG_BUFFER 50

Battery batt(TEMP_SENSOR, VOLTAGE_PIN, HEATER_PIN, CHARGER_PIN, GREEN_LED, YELLOW_LED, RED_LED);

int tempest;
int batteryInSeries;
unsigned int mittausmillit = 0;  // battery voltage and heat reading millis()


int previousVState = 0;
int previousTState = 0;
String logEntries;
String logBuffet;

//UI handles
uint16_t tab1, tab2, tab3, tab4, tab5;
String clearLabelStyle, switcherLabelStyle;
uint16_t battInfo, permInfo;
uint16_t labelId, APSwitcher, ipLabel, ipText, statsLabel, uptimeLabel, ipaddrLabel, mdnsLabel, mdnsaddrLabel;
uint16_t httpUserAccess, httpUsername, httpPassword;
uint16_t voltLabel, tempLabel;
uint16_t tempBoostSwitcher, voltBoostSwitcher; 
uint16_t oldEcoTempLabel, oldBoostTempLabel;  // removable
uint16_t ecoTempLabel, boostTempLabel, ecoTempMemoryLabel, boostTempMemoryLabel;
uint16_t tempsLabels, group1, text1, text2, text3, text4, text5, text6, text7, text8, text9, text10, text11, text12;
uint16_t verticalVoltGroup, VerticalTempGroup, quickControls, horizontalGroup;
uint16_t voltSwitcher, tempSwitcher, infoLabel, timeLabel, timeString, extraLabel, ultraLabel, chargerTimespan, quickPanelVoltage;
uint16_t logLabel, firstLogLabel, secondLogLabel, thirdLogLabel, fourthLogLabel;

uint16_t pidPLabel, pidPText, pidPNum;

uint16_t heater, heaterRes, heaterNum;

bool previousVoltBoostActive = false;
bool previousTempBoostActive = false;

uint32_t now = 0;

//UI handles
const char* hostname = "helmi";

uint16_t graph;
volatile bool updates = false;

uint16_t wifi_ssid_text, wifi_pass_text, wifiLabelSSID, wifiLabelPass, wifiButton;

uint16_t httpPass, httpUser, httpEnable, httpButton;

uint16_t mqttEnable, mqttUser, mqttPass, mqttIp, mqttUsername, mqttPassword, mqttIpaddr, mqttButton;

uint16_t tgEnable, tgUser, tgToken, tgLabelUser, tgLabelToken, tgButton;

uint16_t logTopic, logSeparator, localHosti, localAddr;

//ESPUI Configs page integers:
uint16_t mainSelector, permSwitcher;
uint16_t ecoTempNum, ecoVoltNum;
uint16_t boostTemp, boostVolts, nameLabel, hostnameLabel, ecoVoltLabel, ecoVoltUnitLabel, boostVoltUnitLabel, boostVoltLabel;
uint16_t saveButton;
uint16_t textSeriesConfig, seriesConfigNum, vertgroupswitcher, chargerTimeFeedback;

uint16_t autoSeriesDet, autoSeriesNum;

String stored_ssid;
String stored_pass;

Preferences preferences;

//Function Prototypes for ESPUI
void connectWifi();
void setUpUI();
void textCallback(Control *sender, int type);
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
void selectBattery(Control* sender, int type);
bool checkAndLogState();
bool checkAndLogBoostState();

String httpUserAcc = "batt";
String httpPassAcc = "ass";
bool httpEn = false;
bool mqttEn = false;
bool telegEn = false;



void setup() {
	  Serial.begin(115200);
    batt.setup();

  preferences.begin("btry", true);
    String hosting = preferences.getString("myname", String("Helmi"));
    httpEn = preferences.getBool("httpen", false);
    httpUserAcc = preferences.getString("httpusr");
    httpPassAcc = preferences.getString("httppwd");
    hostname = hosting.c_str();
  preferences.end();

  Serial.println("Hostname" + hosting);

  preferences.begin("wifi", true);
    stored_ssid = preferences.getString("ssid", String("Olohuone"));
    stored_pass = preferences.getString("pass", String("10209997"));
  preferences.end();

  Serial.println("Stored SSID:" + stored_ssid);
  Serial.println("Stored Pass:" + stored_pass);

  preferences.begin("http", true); // "wifi" is the namespace, false means read/write
  httpUserAcc = preferences.getString("user", "batt");
  httpPassAcc = preferences.getString("pass", "ass");
  preferences.end(); // Close Preferences  


/*
                // Open Preferences with a namespace
        preferences.begin("wifi", false); // "wifi" is the namespace, false means read/write
        // Save SSID and Password
        preferences.putString("ssid", ESPUI.getControl(wifi_ssid_text)->value);
        preferences.putString("password", ESPUI.getControl(wifi_pass_text)->value);
        preferences.end(); // Close Preferences  
*/

#if defined(ESP32)
    batt.loadHostname();  // needs to be before setUpUI!!   
#else
    WiFi.hostname(hostname);
#endif
	while(!Serial);
	if(SLOW_BOOT) delay(5000); //Delay booting to give time to connect a serial monitor
	connectWifi();
	#if defined(ESP32)
		WiFi.setSleep(true); //For the ESP32: turn off sleeping to increase UI responsivness (at the cost of power use)
	#endif
	  setUpUI();
}   

void setUpUI() {

  ESPUI.setVerbosity(Verbosity::Quiet);
  ESPUI.captivePortal = true;
  ESPUI.sliderContinuous = false;
  
    auto tab1 = ESPUI.addControl(Tab, "Info", "Info");
    auto tab2 = ESPUI.addControl(Tab, "Setup", "Setup");
    auto wifitab = ESPUI.addControl(Tab, "wifitab", "WiFi");
    auto tgtab = ESPUI.addControl(Tab, "telegram", "Telegram");

  /*-
   * Tab: Basic Controls
   * This tab contains all the basic ESPUI controls, and shows how to read and update them at runtime.
   *-----------------------------------------------------------------------------------------------------------*/
  
  // Styles
  clearLabelStyle = "background-color: unset; width: 100%;";
  switcherLabelStyle = "width: 25%; background-color: unset;";

// ###########################################   QuickTAB  ########################################################
  auto vertgroupswitcher = ESPUI.addControl(Label, "QuickPanel", "", Peterriver);
  ESPUI.setVertical(vertgroupswitcher); 
  ESPUI.setElementStyle(vertgroupswitcher, "background-color: unset;");

  chargerTimeFeedback = ESPUI.addControl(Label, "Charger Time", "0.00", None, vertgroupswitcher);
                        ESPUI.setElementStyle(chargerTimeFeedback, switcherLabelStyle);

  tempLabel           = ESPUI.addControl(Label, "TempLabel", "0", None, vertgroupswitcher);
                        ESPUI.setElementStyle(tempLabel , switcherLabelStyle);
    
  quickPanelVoltage   = ESPUI.addControl(Label, "QuickbtrToVolts", "0", None, vertgroupswitcher);  // NewlineLabel
                        ESPUI.setElementStyle(quickPanelVoltage, switcherLabelStyle);

  voltLabel           = ESPUI.addControl(Label, "VoltLabel", "0", None, vertgroupswitcher);
                        ESPUI.setElementStyle(voltLabel, switcherLabelStyle);

                        ESPUI.setElementStyle(ESPUI.addControl(Label, "emptyLine", "", None, vertgroupswitcher), clearLabelStyle);  // NewlineLabel
  
  extraLabel          = ESPUI.addControl(Label, "ExtraLabel", "Temp Boost", None, vertgroupswitcher); 
                        ESPUI.setVertical(extraLabel);
                        ESPUI.setElementStyle(extraLabel, switcherLabelStyle);

  tempSwitcher        = ESPUI.addControl(Switcher, "", "", None, vertgroupswitcher, boostTempSwitcherCallback);
                        ESPUI.setVertical(tempSwitcher);
                        // ESPUI.updateSwitcher(tempSwitcher, "1");

  voltSwitcher        = ESPUI.addControl(Switcher, "", "", None, vertgroupswitcher, boostVoltageSwitcherCallback);
                        ESPUI.setVertical(voltSwitcher);
                        // ESPUI.updateSwitcher(voltSwitcher, "1");
                        

  ultraLabel          = ESPUI.addControl(Label, "", "Voltage Boost", None, vertgroupswitcher);  // NewlineLabel
                        ESPUI.setVertical(ultraLabel);
                        ESPUI.setElementStyle(ultraLabel, switcherLabelStyle);

                        ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, vertgroupswitcher), clearLabelStyle);  // NewlineLabel

/*
      STATS like IP and uptime
*/
  statsLabel         = ESPUI.addControl(Label, "Stats:", "", None, tab1);
                        ESPUI.setElementStyle(statsLabel, "background-color: unset; width: 100%; color: white; font-size: small; ");

  uptimeLabel =       ESPUI.addControl(Label, "Uptime:", "Uptime", None, statsLabel);
                        ESPUI.setElementStyle(uptimeLabel, "background-color: unset; width: 25%; color: white; font-size: small; text-align: left;  ");

  labelId           = ESPUI.addControl(Label, "Uptime:", "", None, statsLabel);
                      ESPUI.setElementStyle(labelId, "background-color: unset; width: 75%; color: white; font-size: small;  ");

  ipaddrLabel       = ESPUI.addControl(Label, "IP", "IP", None, statsLabel);
                        ESPUI.setElementStyle(ipaddrLabel, "background-color: unset; width: 25%; color: white; font-size: small; text-align: left; ");

  ipText            = ESPUI.addControl(Label, "IP", "ipAddress", None, statsLabel);
                      ESPUI.setElementStyle(ipText, "background-color: unset; width: 75%; color: white; font-size: small;  ");
 
  localHosti        =   ESPUI.addControl(Label, "Hostname", "Hostname", None, statsLabel);
                        ESPUI.setElementStyle(localHosti, "background-color: unset; width: 25%; color: white; font-size: small; text-align: left; ");

  localAddr         = ESPUI.addControl(Label, "IP", "http:\/\/helmi.local", None, statsLabel);
                      ESPUI.setElementStyle(localAddr, "background-color: transparent; border: 1px solid white; text-align: center; font-size: small; -webkit-appearance: none; -moz-appearance: textfield; width: 75%;");
                  
                      ESPUI.setElementStyle(ESPUI.addControl(Label, "emptyLine", "", None, statsLabel), clearLabelStyle);  // NewlineLabel



/*
  pidPNum           = ESPUI.addControl(Number, "PID_Pnum", "", Emerald, group1, generalCallback);
                      ESPUI.addControl(Min, "", "0", None, pidPNum);
                      ESPUI.addControl(Max, "", "10", None, pidPNum);
                      ESPUI.setElementStyle(pidPNum, "background-color: transparent; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield; width: 25%;");
                      ESPUI.setElementStyle(ESPUI.addControl(Label, "Precent", " ", Emerald, group1), "font-size: medium; font-family: serif; width: 25%; text-align: left; background-color: unset; color: darkslategray;");


*/



/*

  LOGGING OF STATES

*/

  // logTopic            = ESPUI.addControl(Label, "logging", "Logs", None, statsLabel);
  //                        ESPUI.setElementStyle(logTopic, "background-color: unset; width: 100%; color: white; font-size: small; ");

  logSeparator        = ESPUI.addControl(Label, "Histor", "Logs", Emerald, statsLabel);
                        ESPUI.setElementStyle(logSeparator, "text-align: left; font-size: small; font-family: serif; width: 100%; background-color: unset; color: white; margin-top: 10px; border-bottom: 1px solid darkslategray; padding-bottom: 5px;");

  logLabel            = ESPUI.addControl(Label, "Log", "", None, statsLabel);
                        ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, logLabel), clearLabelStyle);  // NewlineLabel

  firstLogLabel       = ESPUI.addControl(Label, "FirstLog", "", None, statsLabel);
                        ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, firstLogLabel), clearLabelStyle);  // NewlineLabel
                        ESPUI.setElementStyle(firstLogLabel, "background-color: unset; width: 100%; color: white; font-size: small; ");

// #################################################################################################
 /*
  * First tab
  *_____________________________________________________________________________________________________________*/
/*


*/
  infoLabel         = ESPUI.addControl(Label, "How To Use", "Battery Assistant<br><br>How To:<br><br>For initial setup, the most important thing is to define your battery configuration. Is your battery a 7S or 16S defines how your charger is controlled.<ul><li>7S is 24V</li><li>10S is 36V</li><li>13S is 48V</li><li>16S is 60V</li><li>20S is 72V</li></ul><p>Get to know how to use this even more from the link below</p>", Peterriver, tab1, textCallback);
                      ESPUI.setElementStyle(infoLabel, "font-family: serif; background-color: unset; width: 100%; text-align: left;");
  
  group1            = ESPUI.addControl(Label, "Battery Settings", "", Turquoise, tab2);
                      ESPUI.setElementStyle(group1, "background-color: unset; width: 100%; color: black; font-size: medium;         ");

  nameLabel         = ESPUI.addControl(Text, "Hostname ", "Helmi", Alizarin, group1, textCallback);
                      ESPUI.setElementStyle(nameLabel, "background-color: unset; width: 50%; text-align: center; font-size: medium;");
                      ESPUI.setElementStyle(nameLabel, "font-size: x-large; font-family: serif; width: 50%; text-align: center;");
//    Heater Resistance
  heater            = ESPUI.addControl(Label, "Chrgr", "Heater", Emerald, group1);
                      ESPUI.setElementStyle(heater, "text-align: left; font-size: small; font-family: serif; width: 100%; background-color: unset; color: black; margin-top: 20px; border-bottom: 1px solid darkslategray; padding-bottom: 5px;");

  heaterRes         = ESPUI.addControl(Label, "Battery Charger", "Resistance", None, group1);    
                      ESPUI.setElementStyle(heaterRes, "background-color: unset; width: 50%; text-align: left; font-size: small; color: darkslategray;");

  heaterNum         = ESPUI.addControl(Number, "Charger Settings ", "", Emerald, group1, generalCallback);
                      ESPUI.addControl(Min, "", "0", None, heaterNum);
                      ESPUI.addControl(Max, "", "300", None, heaterNum);
                      ESPUI.setElementStyle(heaterNum, "background-color: transparent; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield; width: 25%;");
                      ESPUI.setElementStyle(ESPUI.addControl(Label, "A", "Ohm", Emerald, group1), "font-size: medium; font-family: serif; width: 25%; text-align: left; background-color: unset; color: darkslategray;");
//   PID P Term
  pidPText          = ESPUI.addControl(Label, "PID_Ptext", "Proportional", None, group1);    
                      ESPUI.setElementStyle(pidPText, "background-color: unset; width: 50%; text-align: left; font-size: small; color: darkslategray;");

  pidPNum           = ESPUI.addControl(Number, "PID_Pnum", "", Emerald, group1, generalCallback);
                      ESPUI.addControl(Min, "", "0", None, pidPNum);
                      ESPUI.addControl(Max, "", "10", None, pidPNum);
                      ESPUI.setElementStyle(pidPNum, "background-color: transparent; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield; width: 25%;");
                      ESPUI.setElementStyle(ESPUI.addControl(Label, "Precent", " ", Emerald, group1), "font-size: medium; font-family: serif; width: 25%; text-align: left; background-color: unset; color: darkslategray;");

// ---------------------------
  text8             = ESPUI.addControl(Label, "Chrgr", "Battery Charger Info", Emerald, group1);
                      ESPUI.setElementStyle(text8, "text-align: left; font-size: small; font-family: serif; width: 100%; background-color: unset; color: black; margin-top: 20px; border-bottom: 1px solid darkslategray; padding-bottom: 5px;");

  text9             = ESPUI.addControl(Label, "Battery Charger", "Charger", None, group1);    
                      ESPUI.setElementStyle(text9, "background-color: unset; width: 50%; text-align: left; font-size: small; color: darkslategray;"); 

  text10 =            ESPUI.addControl(Number, "Charger Settings ", "2", Emerald, group1, generalCallback); 
                      ESPUI.addControl(Min, "", "1", None, text10); 
                      ESPUI.addControl(Max, "", "5", None, text10); 
                      ESPUI.setElementStyle(text10, "background-color: transparent; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield; width: 25%;");

                      ESPUI.setElementStyle(ESPUI.addControl(Label, "A", "A", Emerald, group1), "font-size: medium; font-family: serif; width: 25%; text-align: left; background-color: unset; color: darkslategray;");

text11              = ESPUI.addControl(Label, "Battery Capacity", "Capacity", None, group1);    
                      ESPUI.setElementStyle(text11, "background-color: unset; width: 50%; text-align: left; font-size: small; color: darkslategray;");

text12              = ESPUI.addControl(Number, "Capacity Settings ", "20", Emerald, group1, generalCallback);
                      ESPUI.addControl(Min, "", "1", None, text12);
                      ESPUI.addControl(Max, "", "20", None, text12);
                      ESPUI.setElementStyle(text12, "background-color: transparent; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield; width: 25%;");
                      ESPUI.setElementStyle(ESPUI.addControl(Label, "A", "Ah", Emerald, group1), "font-size: medium; font-family: serif; width: 25%; text-align: left; background-color: unset; color: darkslategray;");

text7               = ESPUI.addControl(Label, "Tempsis", "Batterys Nominal Value", Emerald, group1);
                      ESPUI.setElementStyle(text7, "text-align: left; font-size: small; font-family: serif; width: 100%; background-color: unset; color: black; margin-top: 20px; border-bottom: 1px solid darkslategray; padding-bottom: 5px;");

autoSeriesDet       = ESPUI.addControl(Label, "Battery Series Configuration", "Autodetect", None, group1);
                      ESPUI.setElementStyle(autoSeriesDet, "background-color: unset; width: 50%; text-align: left; font-size: small; color: darkslategray;");

autoSeriesNum       = ESPUI.addControl(Number, "Series Configuration", "7", Emerald, group1,  generalCallback);  // selectBattery);
                      ESPUI.addControl(Min, "", "7", None, autoSeriesNum);
                      ESPUI.addControl(Max, "", "21", None, autoSeriesNum);
                      ESPUI.setElementStyle(autoSeriesNum, "background-color: transparent; width: 25%; border: none; text-align: center; ; -webkit-appearance: none; -moz-appearance: textfield;");
                      ESPUI.setElementStyle(ESPUI.addControl(Label, "C", "Strings", Emerald, group1), "font-family: serif; width: 25%; text-align: center; background-color: unset;color: darkslategray;");

// Add Battery Series Configuration
textSeriesConfig = ESPUI.addControl(Label, "Battery Series Configuration", "Series Config", None, group1);
                  ESPUI.setElementStyle(textSeriesConfig, "background-color: unset; width: 50%; text-align: left; font-size: small; color: darkslategray;");

seriesConfigNum = ESPUI.addControl(Number, "Series Configuration", "13", Emerald, group1, generalCallback);           // selectBattery);
                  ESPUI.addControl(Min, "", "7", None, seriesConfigNum);
                  ESPUI.addControl(Max, "", "21", None, seriesConfigNum);
                  ESPUI.setElementStyle(seriesConfigNum, "background-color: transparent; width: 25%; border: none; text-align: center; ; -webkit-appearance: none; -moz-appearance: textfield;");

                  ESPUI.setElementStyle(ESPUI.addControl(Label, "C", "Strings", Emerald, group1), "font-family: serif; width: 25%; text-align: center; background-color: unset;color: darkslategray;");

text6 =           ESPUI.addControl(Label, "Tempsis", "Temperature Settings", Emerald, group1);
                  ESPUI.setElementStyle(text6, "text-align: left; font-size: small; font-family: serif; width: 100%; background-color: unset; color: black; margin-top: 20px; border-bottom: 1px solid darkslategray; padding-bottom: 5px;");


text1          =  ESPUI.addControl(Label, "Economy Temperature", "Eco temp", None, group1);    
                  ESPUI.setElementStyle(text1, "background-color: unset; width: 50%; text-align: left; font-size: small; color: darkslategray;");


ecoTempNum =  ESPUI.addControl(Number, "Temperature settings ", "15", Emerald, group1, generalCallback);     // ecoTempCallback);
              ESPUI.addControl(Min, "", "1", None, ecoTempNum);
              ESPUI.addControl(Max, "", "35", None, ecoTempNum);
              ESPUI.setElementStyle(ecoTempNum, "background-color: transparent; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield; width: 25%;");

              ESPUI.setElementStyle(ESPUI.addControl(Label, "C", "℃", Emerald, group1), "font-size: medium; font-family: serif; width: 25%; text-align: left; background-color: unset; color: darkslategray;");


text2       = ESPUI.addControl(Label, "Boost temp", "Boost temp", None, group1);
              ESPUI.setElementStyle(text2, "background-color: unset; width: 50%; text-align: left; color: darkslategray; font-size: small;");

boostTemp = ESPUI.addControl(Number, "Boost temp        'C", "25", Turquoise, group1, generalCallback);  // boostTempCallback);
            ESPUI.addControl(Min, "", "1", None, boostTemp);
            ESPUI.addControl(Max, "", "35", None, boostTemp);
            ESPUI.setElementStyle(boostTemp, "background-color: transparent; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield; width: 25%;");
            ESPUI.setElementStyle(ESPUI.addControl(Label, "C", "℃", Emerald, group1), "font-family: serif; width: 25%; text-align: left; background-color: unset;color: darkslategray;");

// Voltage tab
text3 =     ESPUI.addControl(Label, "Voltage Settings", "Voltage Settings", Emerald, group1);
            ESPUI.setElementStyle(text3, "text-align: left; font-size: small; font-family: serif; width: 100%; background-color: unset; color: black; margin-top: 20px; border-bottom: 1px solid darkslategray; padding-bottom: 5px;");

// Economy Charging text
text4 =     ESPUI.addControl(Label, "Eco Charge", "Eco Charge", None, group1);
            ESPUI.setElementStyle(text4, "background-color: unset; width: 50%; text-align: left; font-size: small; color: darkslategray;");

// economy charging precent value in a number box
ecoVoltNum = ESPUI.addControl(Number, "Eco Charge", "70", Emerald, group1, generalCallback);   // ecoVoltCallback);
              ESPUI.addControl(Min, "", "50", None, ecoVoltNum);
              ESPUI.addControl(Max, "", "100", None, ecoVoltNum);
              ESPUI.setElementStyle(ecoVoltNum, "background-color: transparent; border: none; font-size: medium; text-align: center; -webkit-appearance: none; -moz-appearance: textfield; width: 25%;");

// @brief add a precent label
ESPUI.setElementStyle(ecoVoltUnitLabel = ESPUI.addControl(Label, "precent", "%", Emerald, group1), "font-size: small; font-family: serif; width: 7%; background-color: unset; text-align: left; color: darkslategray;");

// placeholder for the udated value
ESPUI.setElementStyle(ecoVoltLabel = ESPUI.addControl(Label, "Volts", "0", Emerald, group1), "text-align: center; font-size: small; font-family: serif; width: 18%; background-color: unset; color: darkslategray;");

// Boost Charge text
text5 =       ESPUI.addControl(Label, "Boost Charge", "Boost Charge", None, group1);
              ESPUI.setElementStyle(text5, "background-color: unset; width: 50%; text-align: left; font-size: small; color: darkslategray;");

boostVolts =  ESPUI.addControl(Number, "Boost Charge", "90", Turquoise, group1, generalCallback);    //       boostVoltCallback);
              ESPUI.addControl(Min, "", "60", None, boostVolts);
              ESPUI.addControl(Max, "", "100", None, boostVolts);
              ESPUI.setElementStyle(boostVolts, "background-color: transparent; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield; width: 25%;");

              ESPUI.setElementStyle(boostVoltUnitLabel = ESPUI.addControl(Label, "precent", "%", Emerald, group1), "font-size: small; font-family: serif; width: 7%; background-color: unset; text-align: left; color: darkslategray;");
              ESPUI.setElementStyle(boostVoltLabel = ESPUI.addControl(Label, "Volts", "0", Emerald, group1), "text-align: center; font-size: small; font-family: serif; width: 18%; background-color: unset; color: darkslategray;");

chargerTimespan = ESPUI.addControl(Label, "Charger Time", "0.00", None, group1);
                  ESPUI.setElementStyle(chargerTimespan, "color: black; background-color: unset; width: 100%; text-align: right; font-size: medium; font-family: serif; margin-right: 15px; margin-top: 5px; border-top: 1px solid darkslategray; padding-bottom: 5px;");

text7 = ESPUI.addControl(Label, "Perms", "Save to memory", Emerald, group1);
        ESPUI.setElementStyle(text7, "text-align: left; font-size: small; font-family: serif; width: 100%; background-color: unset; color: black; margin-top: 20px; border-bottom: 1px solid darkslategray; padding-bottom: 5px;");

saveButton = ESPUI.addControl(Button, "Memory", "Save", None, group1, [](Control *sender, int type) {
    if (type == B_UP) {
        Serial.println("Saving battery structs...");
        Serial.println(ESPUI.getControl(nameLabel)->value);
        Serial.println(ESPUI.getControl(ecoTempNum)->value);
        Serial.println(ESPUI.getControl(ecoVoltNum)->value);
        Serial.println(ESPUI.getControl(boostTemp)->value);
        Serial.println(ESPUI.getControl(boostVolts)->value);
        Serial.println(ESPUI.getControl(seriesConfigNum)->value);
        Serial.println(ESPUI.getControl(text10)->value);
        Serial.println(ESPUI.getControl(text12)->value);
        Serial.println(ESPUI.getControl(heaterNum)->value);
        Serial.println(ESPUI.getControl(pidPNum)->value);
        Serial.println(ESPUI.getControl(nameLabel)->value);
      if (    batt.setNominalString(ESPUI.getControl(seriesConfigNum)->value.toInt())   )  {

              Serial.print("ecoPrecent: ");
              Serial.println(batt.setEcoPrecentVoltage(ESPUI.getControl(ecoVoltNum)->value.toInt()));

              Serial.print("setEcoTemp ");
              Serial.println(batt.setEcoTemp(ESPUI.getControl(ecoTempNum)->value.toInt()));

              Serial.print("BoostPrecentV ");
              Serial.println(batt.setBoostPrecentVoltage(ESPUI.getControl(boostVolts)->value.toInt()));

              Serial.print("BoostTemp: ");
              Serial.println(batt.setBoostTemp(ESPUI.getControl(boostTemp)->value.toInt()));

              Serial.print(" Charger: ");
              Serial.println(batt.setCharger(ESPUI.getControl(text10)->value.toInt()));

              Serial.print(" Resistance: ");
              Serial.println(batt.setResistance(ESPUI.getControl(heaterNum)->value.toInt()));

              Serial.print("Capacity: ");
              Serial.println(batt.setCapacity(ESPUI.getControl(text12)->value.toInt()));

              Serial.print("PIDP ");
              Serial.println(batt.setPidP(ESPUI.getControl(pidPNum)->value.toFloat()));

                Serial.println("All ok! Saving settings to memory...");  
                batt.saveHostname(ESPUI.getControl(nameLabel)->value);
                batt.saveSettings();
      } 
      else { Serial.println("Error: Invalid input"); }
        Serial.println("Saved in memory...");
        Serial.println(batt.getNominalString());
        Serial.println(batt.getEcoPrecentVoltage());
        Serial.println(batt.getEcoTemp());
        Serial.println(batt.getBoostPrecentVoltage());
        Serial.println(batt.getBoostTemp());
        Serial.println(batt.getCharger());
        Serial.println(batt.getCapacity());
        Serial.println(batt.getResistance());
        Serial.println(batt.loadHostname());
        Serial.println(batt.getPidP());
    }
});
ESPUI.setElementStyle(saveButton, "width: 20%; text-align: center; font-size: medium; font-family: serif; margin-top: 20px; margin-bottom: 20px; border-radius: 15px;");

/*

      WLAN Wifi tab 

*/
auto  wifiLabel    = ESPUI.addControl(Label, "WLAN", "", Peterriver, wifitab, generalCallback);
                    ESPUI.setElementStyle(wifiLabel, "background-color: unset; width: 100%; color: black; font-size: medium;         ");

  wifiLabelSSID = ESPUI.addControl(Label, "ssidLabel", "SSID:", Turquoise, wifiLabel);
                  ESPUI.setElementStyle(wifiLabelSSID, "background-color: unset; width: 25%; text-align: left; font-size: small; ");

  wifi_ssid_text = ESPUI.addControl(Text, "SSID", "", Alizarin, wifiLabel, textCallback);
                  //Note that adding a "Max" control to a text control sets the max length
                  ESPUI.addControl(Max, "", "32", None, wifi_ssid_text);
                  ESPUI.setElementStyle(wifi_ssid_text, "background-color: transparent; width: 75%; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield;");

  wifiLabelPass = ESPUI.addControl(Label, "passwd", "Password", Turquoise, wifiLabel);
                  ESPUI.setElementStyle(wifiLabelPass, "background-color: unset; width: 25%; text-align: left; font-size: small; ");

  wifi_pass_text = ESPUI.addControl(Text, "Password", "", Alizarin, wifiLabel, textCallback);
                  ESPUI.addControl(Max, "", "64", None, wifi_pass_text);
                  ESPUI.setElementStyle(wifi_pass_text, "background-color: transparent; width: 75%; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield;");
 
 wifiButton = ESPUI.addControl(Button, "Save", "Save", Peterriver, wifiLabel,
      [](Control *sender, int type) {
      if(type == B_UP) {
        Serial.println("Saving credentials to NVS...");
        Serial.println(ESPUI.getControl(wifi_ssid_text)->value);
        Serial.println(ESPUI.getControl(wifi_pass_text)->value);

        preferences.begin("wifi", false); // "wifi" is the namespace, false means read/write
        preferences.putString("ssid", ESPUI.getControl(wifi_ssid_text)->value);
        preferences.putString("pass", ESPUI.getControl(wifi_pass_text)->value);
        preferences.end(); // Close Preferences  
      }
      });
  ESPUI.setElementStyle(wifiButton,"width: 20%; text-align: center; font-size: medium; font-family: serif; margin-top: 20px; margin-bottom: 20px; border-radius: 15px;");

 //  clearLabelStyle = "background-color: unset; width: 100%;";
 //  switcherLabelStyle = "width: 25%; background-color: unset;";
//ESPUI.separator("HTTP");
// ----- HTTP 

auto      httpLabel    = ESPUI.addControl(Label, "HTTP", "Enable HTTP", Peterriver, wifitab, generalCallback);
          httpEnable   = ESPUI.addControl(Switcher, "Enable HTTP", "", None, httpLabel, generalCallback);
                          ESPUI.setElementStyle(ESPUI.addControl(Label, "emptyLine", "", None, httpLabel), clearLabelStyle);

httpUsername            = ESPUI.addControl(Label, "Battery Charger", "Username", None, httpLabel);    
                            ESPUI.setElementStyle(httpUsername, "background-color: unset; width: 25%; text-align: left; font-size: small;");
 
          httpUser     = ESPUI.addControl(Text, "Username", "", Dark, httpLabel, textCallback);
                          ESPUI.setElementStyle(httpUser, "background-color: transparent; width: 75%; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield;");
                                             

httpPassword            = ESPUI.addControl(Label, "Battery Charger", "Password", None,httpLabel);    
                            ESPUI.setElementStyle(httpPassword, "background-color: unset; width: 25%; text-align: left; font-size: small;");

          httpPass     = ESPUI.addControl(Text, "Password", "", Dark, httpLabel, textCallback);
                          ESPUI.setElementStyle(httpPass, "background-color: transparent; width: 75%; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield;");

                          ESPUI.setElementStyle(httpLabel, "font-family: serif; background-color: unset; width: 100%;");
                          ESPUI.setElementStyle(httpEnable, "text-align: center; font-size: medium; font-family: serif; margin-top: 5px; margin-bottom: 5px;");

uint16_t httpButton = ESPUI.addControl(Button, "Memory", "Save", None, httpLabel, [](Control *sender, int type) {
    if (type == B_UP) {

        Serial.println("Saving battery structs...");
        Serial.println(ESPUI.getControl(httpUser)->value);  
        Serial.println(ESPUI.getControl(httpPass)->value); 
        Serial.println(ESPUI.getControl(httpEnable)->value);

        // Open Preferences with a namespace
        preferences.begin("http", false); // "wifi" is the namespace, false means read/write
        preferences.putString("user", ESPUI.getControl(httpUser)->value);
        preferences.putString("pass", ESPUI.getControl(httpPass)->value);
        preferences.putBool("httpen", ESPUI.getControl(httpEnable)->value);
        preferences.end(); // Close Preferences  
        } });

        ESPUI.setElementStyle(httpButton, "width: 20%; text-align: center; font-size: medium; font-family: serif; margin-top: 20px; margin-bottom: 20px; border-radius: 15px;");

// ESPUI.separator("MQTT");
//  --- MQTT
        
 auto   mqttLabel    = ESPUI.addControl(Label, "MQTT", "Enable MQTT", Peterriver, wifitab, generalCallback);
                        ESPUI.setElementStyle(mqttLabel, "font-family: serif; background-color: unset; width: 100%;");

// MQTT ENABLE
        mqttEnable   = ESPUI.addControl(Switcher, "Enable MQTT", "", None, mqttLabel, generalCallback);
                        ESPUI.setElementStyle(mqttEnable, "text-align: center; font-size: medium; font-family: serif; margin-top: 5px; margin-bottom: 5px;");
                          ESPUI.setElementStyle(ESPUI.addControl(Label, "emptyLine", "", None, mqttLabel), clearLabelStyle);

// MQTT USERNAME
       mqttUsername = ESPUI.addControl(Label, "Username", "Username", Dark, mqttLabel);      
                        ESPUI.setElementStyle(mqttUsername, "background-color: unset; width: 25%; text-align: left; font-size: small;");
                        
        mqttUser     = ESPUI.addControl(Text, "Username", "", Dark, mqttLabel, textCallback);
                        ESPUI.setElementStyle(mqttUser, "background-color: transparent; width: 75%; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield;");

// MQTT PASSWORD
        mqttPassword = ESPUI.addControl(Label, "pass", "Password", Dark, mqttLabel);      
                        ESPUI.setElementStyle(mqttPassword, "background-color: unset; width: 25%; text-align: left; font-size: small;");
   
        mqttPass     = ESPUI.addControl(Text, "Password", "", Dark, mqttLabel, textCallback);
                        ESPUI.setElementStyle(mqttPass, "background-color: transparent; width: 75%; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield;");


// MQTT IP ADDRESS INPUT
        mqttIpaddr   = ESPUI.addControl(Label, "ip", "IP", Dark, mqttLabel);
                          ESPUI.setElementStyle(mqttIpaddr, "background-color: unset; width: 25%; text-align: left; font-size: small;");

        mqttIp       = ESPUI.addControl(Text, "IP", "", Dark, mqttLabel, textCallback);
                        ESPUI.setElementStyle(mqttIp, "background-color: transparent; width: 75%; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield;");


                         
// MQTT SaveButton  
  mqttButton = ESPUI.addControl(Button, "Memory", "Save", None, mqttLabel, [](Control *sender, int type) {
    if (type == B_UP) {
        Serial.println("Saving battery structs...");
        Serial.println(ESPUI.getControl(mqttEnable)->value);  
        Serial.println(ESPUI.getControl(mqttUser)->value); 
        Serial.println(ESPUI.getControl(mqttPass)->value);
        Serial.println(ESPUI.getControl(mqttIp)->value);

                // Open Preferences with a namespace
        preferences.begin("mqtt", false); // "wifi" is the namespace, false means read/write
        preferences.putString("user", ESPUI.getControl(mqttUser)->value);
        preferences.putString("pass", ESPUI.getControl(mqttPass)->value);
        preferences.putString("ip",   ESPUI.getControl(mqttIp)->value);
        preferences.putBool("mqtten", ESPUI.getControl(mqttEnable)->value);
        preferences.end(); // Close Preferences  

        } });
        ESPUI.setElementStyle(mqttButton, "width: 20%; text-align: center; font-size: medium; font-family: serif; margin-top: 20px; margin-bottom: 20px; border-radius: 15px;");


//  --- Telegram
      
auto  tgLabel    = ESPUI.addControl(Label, "Telegram", "Enable Telegram", Peterriver, tgtab, generalCallback);
                    ESPUI.setElementStyle(tgLabel, "font-family: serif; background-color: unset; width: 100%; ");

      tgEnable   = ESPUI.addControl(Switcher, "Enable Telegram", "", None, tgLabel, generalCallback);
                      ESPUI.setElementStyle(ESPUI.addControl(Label, "emptyLine", "", None, tgLabel), clearLabelStyle);
                      ESPUI.setElementStyle(tgEnable, "text-align: center; font-size: medium; font-family: serif; margin-top: 5px; margin-bottom: 5px;");

tgLabelUser       = ESPUI.addControl(Label, "Username", "Username", Dark, tgLabel);      
                        ESPUI.setElementStyle(tgLabelUser, "background-color: unset; width: 25%; text-align: left; font-size: small;");

      tgUser      = ESPUI.addControl(Text, "Username", "", Dark, tgLabel, textCallback);
                      ESPUI.setElementStyle(tgUser, "background-color: transparent; width: 75%; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield;");

tgLabelToken      = ESPUI.addControl(Label, "token", "Token", Dark, tgLabel);      
                        ESPUI.setElementStyle(tgLabelToken, "background-color: unset; width: 25%; text-align: left; font-size: small;");

      tgToken     = ESPUI.addControl(Text, "Token", "", Dark, tgLabel, textCallback);
                    ESPUI.setElementStyle(tgToken, "background-color: transparent; width: 75%; border: none; text-align: center; font-size: medium; -webkit-appearance: none; -moz-appearance: textfield;");


// telegram SaveButton
uint16_t tgButton = ESPUI.addControl(Button, "Memory", "Save", None, tgLabel, [](Control *sender, int type) {
    if (type == B_UP) {
        Serial.println("Saving telegram structs...");
        Serial.println(ESPUI.getControl(tgUser)->value);  
        Serial.println(ESPUI.getControl(tgToken)->value); 

        preferences.begin("telegram", false); // "wifi" is the namespace, false means read/write
        preferences.putString("user", ESPUI.getControl(tgUser)->value);
        preferences.putString("token", ESPUI.getControl(tgToken)->value);
        preferences.end(); // Close Preferences  

    } });
  ESPUI.setElementStyle(tgButton,"width: 20%; text-align: center; font-size: medium; font-family: serif; margin-top: 20px; margin-bottom: 20px; border-radius: 15px;");


  /*

    Tab: WiFi Credentials
    You use this tab to enter the SSID and password of a wifi network to autoconnect to.
   
   *-----------------------------------------------------------------------------------------------------------*/
    ESPUI.updateControlValue(ecoTempNum, String(batt.getEcoTemp()));
    ESPUI.updateControlValue(ecoVoltNum, String(batt.getEcoPrecentVoltage()));
    ESPUI.updateControlValue(boostTemp, String(batt.getBoostTemp()));
    ESPUI.updateControlValue(boostVolts, String(batt.getBoostPrecentVoltage()));
    ESPUI.updateControlValue(seriesConfigNum, String(batt.getNominalString()));
    ESPUI.updateControlValue(ecoVoltLabel, String(batt.btryToVoltage(batt.getEcoPrecentVoltage()), 1) + " V");
    ESPUI.updateControlValue(boostVoltLabel, String(batt.btryToVoltage(batt.getBoostPrecentVoltage()), 1) + " V");
    ESPUI.updateControlValue(text10, String(batt.getCharger()));
    ESPUI.updateControlValue(text12, String(batt.getCapacity()));
    ESPUI.updateControlValue(chargerTimeFeedback, String(batt.calculateChargeTime(batt.getEcoPrecentVoltage(), batt.getBoostPrecentVoltage()), 2) + " h");
    ESPUI.updateControlValue(chargerTimespan, String(batt.calculateChargeTime(batt.getEcoPrecentVoltage(), batt.getBoostPrecentVoltage()), 2) + " h");
    ESPUI.updateControlValue(heaterNum, String(batt.getResistance()));
    ESPUI.updateControlValue(pidPNum, String(batt.getPidP()));
    ESPUI.updateControlValue(nameLabel, String(batt.battery.myname));


    ESPUI.updateSwitcher(voltSwitcher, batt.getActivateVoltageBoost());
    ESPUI.updateSwitcher(tempSwitcher, batt.getActivateTemperatureBoost());



  //Finally, start up the UI.
  //This should only be called once we are connected to WiFi.


//  if(httpEn)
//    ESPUI.begin(hostname, httpUserAcc.c_str(), httpPassAcc.c_str());
//  else
    ESPUI.begin("BatteryJeesus");

      //ESPUI.begin(HOSTNAME);
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
    case S_ACTIVE:
			if(batt.setEcoPrecentVoltage(sender->value.toInt()))
        {
          ESPUI.updateLabel(ecoVoltLabel, String(batt.btryToVoltage(sender->value.toInt()), 1));
			   	Serial.print("Ecomode Voltage set to: ");
          Serial.print(batt.getEcoPrecentVoltage());
          Serial.println(" Precent");
        }
      else
      {
          ESPUI.updateLabel(ecoVoltLabel, String(batt.getEcoPrecentVoltage(), 1));
			   	Serial.print("Ecomode Voltage set to: ");
          Serial.print(batt.getEcoPrecentVoltage());
          Serial.println(" Precent");
      }
		break;

	case S_INACTIVE:
		Serial.println("Ecomode Inactive");
		break;
  default:
      Serial.print(type);
      Serial.println("unknown type: EcoVCB");
      break;
  }
  // ESPUI.updateLabel(boostVoltLabel, String(batt.btryToVoltage(sender->value.toInt()), 2));
  // ESPUI.updateLabel(ecoVoltLabel, String(batt.btryToVoltage(sender->value.toInt()), 2));
}
/*

    not used anymore? Scrap select battery?
*/
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
		Serial.print("new Battery selected  ");
    Serial.println(type);
		break;
    }

/*
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
*/

}
/*

	The numbers value button, for defining a boost voltage level in voltage
*/
void boostVoltCallback(Control* sender, int type) {
  switch (type) {
    case S_ACTIVE:
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
          Serial.println(" V");
      }	  
		break;

	case S_INACTIVE:
		Serial.println("Ecomode Inactive");
		break;
  default:
      Serial.print(type);
      Serial.println("unknown type: BVCB");
      break;
    }
}


/*



*/
void ecoTempCallback(Control *sender, int type) {
  switch (type) {
    case S_ACTIVE:
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
		break;
	case S_INACTIVE:
		Serial.println("Ecomode Inactive");
		break;
  default:
      Serial.print(type);
      Serial.println("unknown type: eTCB ");
      break;
  }
}
/*



	Temperature Boost Switch

*/
void boostTempCallback(Control *sender, int type) {
  switch (type) {
    case S_ACTIVE:
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
		break;
	case S_INACTIVE:
		Serial.println("Boost Temp Inactive");
		break;
  default:
      Serial.print(type);
      Serial.println(" unknown type: BtCb ");
    break;
  }
}
/*
    Voltage Boost Switch 
*/
void boostVoltageSwitcherCallback(Control *sender, int type) {
    switch (type)  {
    case S_ACTIVE:
			if(batt.activateVoltageBoost(true))
			  Serial.print("Voltage Boost Activated: ");
        Serial.println(batt.getActivateVoltageBoost());
        // ESPUI.updateSwitcher(voltSwitcher, "1");
		break;
	case S_INACTIVE:
      if(batt.activateVoltageBoost(false))
		    Serial.println("Voltage Boost Inactive: ");
        Serial.println(batt.getActivateVoltageBoost());
        // ESPUI.updateSwitcher(voltSwitcher, "0");
		break;    
    default:
      Serial.print(type);
      Serial.println("  unknown type: BvswitcherCB ");
      break;
  
    }
}
/*
	  Temperature Boost Switch on the Front Page's Quick Controls
*/
void  boostTempSwitcherCallback(Control *sender, int type) {
    switch (type)    {
    case S_ACTIVE: 
	    if(batt.activateTemperatureBoost(true)) {
        Serial.print("Temperature Boost Active: ");
        Serial.println(batt.getActivateTemperatureBoost());
        // ESPUI.updateSwitcher(tempSwitcher, "1");
      }
        break;

    case S_INACTIVE:
        if(batt.activateTemperatureBoost(false)) {
        	Serial.print("Temperature Boost Inactive: ");
          Serial.println(batt.getActivateTemperatureBoost());
          // ESPUI.updateSwitcher(tempSwitcher, "0");
        }
		    else
        	Serial.println("Failed");
        break;
    default:
      Serial.print(type);
      Serial.println("  unknown type:  BtSwCb ");
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




unsigned long lastMsg = 0;
const long interval = 15000; // 1 minute
unsigned long mqttmillis = 0;

/*
void reconnect() {
        // Serial.print("Attempting MQTT connection...");
        if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
            // Serial.println("connected ");
        } else {
            //Serial.print("failed, rc=");
            Serial.print(client.state());
            //Serial.println(" try again in 5 seconds");
        }
    }

void publishMessage() {
    if (!client.connected()) {
        reconnect();
    }
    char buffer[16];
    char topicBuffer[48];

    sprintf(buffer, "%u", Battery::batry.size);
    sprintf(topicBuffer, "battery/%s/size", Battery::batry.myname);
    client.publish(topicBuffer, buffer);

    sprintf(buffer, "%u", Battery::batry.pidP);
    sprintf(topicBuffer, "battery/%s/pidP", Battery::batry.myname);
    client.publish(topicBuffer, buffer);

    sprintf(buffer, "%u", (millis() / uint32_t(60000)));
    sprintf(topicBuffer, "battery/%s/millis", Battery::batry.myname);
    client.publish(topicBuffer, buffer);
    
    sprintf(buffer, "%u", Battery::batry.sizeApprx);
    sprintf(topicBuffer, "battery/%s/sizeApprx", Battery::batry.myname);
    client.publish(topicBuffer, buffer);
    
    sprintf(buffer, "%u", Battery::batry.mV_max);
    sprintf(topicBuffer, "battery/%s/mV_max", Battery::batry.myname);
    client.publish(topicBuffer, buffer);
    
    sprintf(buffer, "%u", Battery::batry.mV_min);
    sprintf(topicBuffer, "battery/%s/mV_min", Battery::batry.myname);
    client.publish(topicBuffer, buffer);
    
    sprintf(buffer, "%.1f", Battery::batry.temperature);
    sprintf(topicBuffer, "battery/%s/temperature", Battery::batry.myname);
    client.publish(topicBuffer, buffer);
    
    sprintf(buffer, "%u", Battery::batry.milliVoltage);
    sprintf(topicBuffer, "battery/%s/milliVoltage", Battery::batry.myname);
    client.publish(topicBuffer, buffer);
    
    sprintf(buffer, "%u", Battery::batry.voltageInPrecent);
    sprintf(topicBuffer, "battery/%s/voltageInPrecent", Battery::batry.myname);
    client.publish(topicBuffer, buffer);
    
    sprintf(buffer, "%u", Battery::batry.ecoVoltPrecent);
    sprintf(topicBuffer, "battery/%s/ecoVoltPrecent", Battery::batry.myname);
    client.publish(topicBuffer, buffer);
    
    sprintf(buffer, "%u", Battery::batry.boostVoltPrecent);
    sprintf(topicBuffer, "battery/%s/boostVoltPrecent", Battery::batry.myname);
    client.publish(topicBuffer, buffer);
    
    sprintf(buffer, "%u", Battery::batry.ecoTemp);
    sprintf(topicBuffer, "battery/%s/ecoTemp", Battery::batry.myname);
    client.publish(topicBuffer, buffer);
    
    sprintf(buffer, "%u", Battery::batry.boostTemp);
    sprintf(topicBuffer, "battery/%s/boostTemp", Battery::batry.myname);
    client.publish(topicBuffer, buffer);
    
    sprintf(buffer, "%u", Battery::batry.resistance);
    sprintf(topicBuffer, "battery/%s/resistance", Battery::batry.myname);
    client.publish(topicBuffer, buffer);
    
    sprintf(buffer, "%u", Battery::batry.capct);
    sprintf(topicBuffer, "battery/%s/capct", Battery::batry.myname);
    client.publish(topicBuffer, buffer);
    
    sprintf(buffer, "%u", Battery::batry.chrgr);
    sprintf(topicBuffer, "battery/%s/chrgr", Battery::batry.myname);
    client.publish(topicBuffer, buffer);
    
    sprintf(buffer, "%d", Battery::batry.vState);
    sprintf(topicBuffer, "battery/%s/vState", Battery::batry.myname);
    client.publish(topicBuffer, buffer);
    
    sprintf(buffer, "%d", Battery::batry.tState);
    sprintf(topicBuffer, "battery/%s/tState", Battery::batry.myname);
    client.publish(topicBuffer, buffer);
    
    sprintf(buffer, "%u", Battery::batry.wantedTemp);
    sprintf(topicBuffer, "battery/%s/wantedTemp", Battery::batry.myname);
    client.publish(topicBuffer, buffer);

    sprintf(buffer, "%.3f", batt.kd);
    sprintf(topicBuffer, "battery/%s/pid/kd", Battery::batry.myname);
    client.publish(topicBuffer, buffer);

    sprintf(buffer, "%.3f", batt.ki);
    sprintf(topicBuffer, "battery/%s/pid/ki", Battery::batry.myname);
    client.publish(topicBuffer, buffer);

    sprintf(buffer, "%.3f", batt.kp);
    sprintf(topicBuffer, "battery/%s/pid/kp", Battery::batry.myname);
    client.publish(topicBuffer, buffer);

    sprintf(buffer, "%.2f", batt.pidInput);
    sprintf(topicBuffer, "battery/%s/pid/pidInput", Battery::batry.myname);
    client.publish(topicBuffer, buffer);

    sprintf(buffer, "%.2f", batt.pidOutput);
    sprintf(topicBuffer, "battery/%s/pid/pidOutput", Battery::batry.myname);
    client.publish(topicBuffer, buffer);

    sprintf(buffer, "%.2f", batt.pidSetpoint);
    sprintf(topicBuffer, "battery/%s/pid/pidSetpoint", Battery::batry.myname);
    client.publish(topicBuffer, buffer);
       
    sprintf(topicBuffer, "battery/%s/voltBoostActive", Battery::batry.myname);
    client.publish(topicBuffer, Battery::batry.voltBoostActive ? "true" : "false");
    
    sprintf(topicBuffer, "battery/%s/tempBoostActive", Battery::batry.myname);
    client.publish(topicBuffer, Battery::batry.tempBoostActive ? "true" : "false");
    
    // Serial.println("Battery data published");
    client.disconnect();
    // Serial.println("MQTT disconnected"); 
  }
  
  */

void loop() {

  batt.loop(); // Add battery.loop() here

  if(millis() - mittausmillit >= 3000)
  {
      mittausmillit = millis();
      String wlanIpAddress;

      wlanIpAddress = WiFi.localIP().toString();
      ESPUI.updateLabel(ipText, wlanIpAddress);
      ESPUI.updateLabel(ipLabel, wlanIpAddress);
      ESPUI.updateLabel(labelId, String((millis() / 60000)));
      ESPUI.updateLabel(voltLabel, String(batt.getBatteryDODprecent()) + " %");
      ESPUI.updateLabel(tempLabel, String(batt.getTemperature(), 1) + " ℃");
      ESPUI.updateLabel(chargerTimeFeedback, String(batt.calculateChargeTime(batt.getEcoPrecentVoltage(), batt.getBoostPrecentVoltage()), 2) + " h");
      ESPUI.updateLabel(boostVoltLabel, String(batt.btryToVoltage(batt.getBoostPrecentVoltage()), 0) + " V");
      ESPUI.updateLabel(quickPanelVoltage, String(batt.getCurrentVoltage(), 1) + " V");
      ESPUI.updateLabel(chargerTimespan, String(batt.calculateChargeTime(batt.getEcoPrecentVoltage(), batt.getBoostPrecentVoltage()), 2) + " h");
      ESPUI.updateLabel(autoSeriesNum, String(batt.getBatteryApprxSize()));

    
      

      if (checkAndLogBoostState()) {
        logEntries += String((millis() / 60000)) + "  " + String("Voltage boost: ") + String(batt.battery.voltBoost) + "     " + String("Temp boost: ") + String(batt.battery.tempBoost) + "\n";
      }

      if (checkAndLogState()) {
        logEntries += String((millis() / 60000)) + "  " + String("Voltage state: ") + String(batt.battery.vState) + "     " + String("Temp state: ")  + String(batt.battery.tState) + "\n";
      }

      ESPUI.updateLabel(firstLogLabel, logEntries);
    }

}

/*
void readStringFromEEPROM(String& buf, int baseaddress, int size) {
	buf.reserve(size);
	for (int i = baseaddress; i < baseaddress+size; i++) {
		char c = EEPROM.read(i);
		buf += c;
		if(!c) break;
	}	
}
*/

bool checkAndLogState() {
    if ((batt.battery.vState != batt.battery.previousVState) || (batt.battery.tState != batt.battery.previousTState)) {
      
      /*
        String newState = "VState: " + String(Battery::batry.vState) + ", TState: " + String(Battery::batry.tState);
        uint32_t timeing = (millis() - now) / 60000;
        String output;
        output = String(timeing) + "    " + newState + "\n";
        previousVState = Battery::batry.vState;
        previousTState = Battery::batry.tState;
        Serial.println(newState);
        return output;
        */

    return true;
    }
    else return false;
}

bool checkAndLogBoostState() {
    if ( batt.battery.voltBoost != previousVoltBoostActive || batt.battery.tempBoost != previousTempBoostActive) {
        previousTempBoostActive = batt.battery.tempBoost;
        previousVoltBoostActive = batt.battery.voltBoost;
        return true;
    }
    else return false;
}

void connectWifi() {
	int connect_timeout;

#if defined(ESP32)
	WiFi.setHostname(hostname);
#else
	WiFi.hostname(HOSTNAME);
#endif
	Serial.println("Begin wifi...");

	//Load credentials from EEPROM 
	if(!(FORCE_USE_HOTSPOT)) {
		yield();
	
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

		if (!MDNS.begin(hostname)) {
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

void textCallback(Control *sender, int type) {
	//This callback is needed to handle the changed values, even though it doesn't do anything itself.
}