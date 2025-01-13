#include "Battery.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <string>
#include <functional>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPUI.h>

//Settings for ESPUI
#define SLOW_BOOT 0
#define HOSTNAME "Helmi"
#define FORCE_USE_HOTSPOT 0
#define LOG_BUFFER 50
#define MAINDEBUG


/*
#define ENABLE_SERIAL_PRINTS true

#if ENABLE_SERIAL_PRINTS
#define SP(x) Serial.print(x)
#define SPLN(x) Serial.println(x)
#else
#define SP(x)
#define SPNL(x)
#endif
*/


// This is correct - external code needs the reference
Battery& batt = Battery::getInstance();


// AsyncTelegram2* bot;
//WiFiClient asiakas; 

Preferences preferences;

int tempest;
int batteryInSeries;
unsigned int mittausmillit = 0;  // battery voltage and heat reading millis()

unsigned long lastMsg = 0;
const long interval = 15000; // 1 minute
unsigned long mqttmillis = 0; 

int previousVState = 0;
int previousTState = 0;
int previousVoltBoostState = 0;
int previousTempBoostState = 0;

String logEntries;
String logTime;
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
uint16_t logLabel, firstLogLabel, firstLogTime, secondLogLabel, thirdLogLabel, fourthLogLabel;

// Heater
uint16_t calibName, calibPass, calibButton;

uint16_t heatName, heatPow, heatOn, heatOnLabel;

uint16_t resetLabel, resetEnable, resetButton;

// uint16_t pidPLabel, pidPText;

uint16_t heater, heaterRes, heaterNum;

bool previousVoltBoostActive = false;
bool previousTempBoostActive = false;

uint32_t now = 0;

//UI handles
const char* hostname = "helmi";

volatile bool updates = false;

uint16_t wifi_ssid_text, wifi_pass_text, wifiLabelSSID, wifiLabelPass, wifiButton;

uint16_t httpPass, httpUser, httpEnable, httpButton;

uint16_t mqttEnable, mqttUser, mqttPass, mqttIp, mqttUsername, mqttPassword, mqttIpaddr, mqttButton;

uint16_t tgEnable, tgUser, tgToken, tgLabelUser, tgLabelToken, tgButton;

uint16_t logTopic, logSeparator, localHosti, localAddr, localHosti2;

//ESPUI Configs page integers:
uint16_t mainSelector, permSwitcher;
uint16_t ecoTempNum, ecoVoltNum;
uint16_t boostTemp, boostVolts, nameLabel, hostnameLabel, ecoVoltLabel, ecoVoltUnitLabel, boostVoltUnitLabel, boostVoltLabel;
uint16_t saveButton;
uint16_t textSeriesConfig, seriesConfigNum, vertgroupswitcher, chargerTimeFeedback;

uint16_t autoSeriesDet, autoSeriesNum;

uint16_t headerTimeLabel, headerTempLabel, headerVoltage, headerPrecent;

String stored_ssid;
String stored_pass;


void updateControlValues() {
    char buffer[50]; // Buffer for formatted strings

    // Update control values using snprintf
    snprintf(buffer, sizeof(buffer), "%d", batt.battery.heater.ecoTemp);
    ESPUI.updateControlValue(ecoTempNum, buffer);

    snprintf(buffer, sizeof(buffer), "%d", batt.battery.ecoVoltPrecent);
    ESPUI.updateControlValue(ecoVoltNum, buffer);

    snprintf(buffer, sizeof(buffer), "%d", batt.battery.heater.boostTemp);
    ESPUI.updateControlValue(boostTemp, buffer);

    snprintf(buffer, sizeof(buffer), "%d", batt.battery.boostVoltPrecent);
    ESPUI.updateControlValue(boostVolts, buffer);

    snprintf(buffer, sizeof(buffer), "%d", batt.battery.size);
    ESPUI.updateControlValue(seriesConfigNum, buffer);

    snprintf(buffer, sizeof(buffer), "%.1f V", batt.btryToVoltage(batt.battery.ecoVoltPrecent));
    ESPUI.updateControlValue(ecoVoltLabel, buffer);

    snprintf(buffer, sizeof(buffer), "%.1f V", batt.btryToVoltage(batt.battery.boostVoltPrecent));
    ESPUI.updateControlValue(boostVoltLabel, buffer);

    snprintf(buffer, sizeof(buffer), "%d", batt.battery.chrgr.current);
    ESPUI.updateControlValue(text10, buffer);

    snprintf(buffer, sizeof(buffer), "%d", batt.battery.capct);
    ESPUI.updateControlValue(text12, buffer);

    snprintf(buffer, sizeof(buffer), "%.1f h", batt.calculateChargeTime(batt.battery.ecoVoltPrecent, batt.battery.boostVoltPrecent));
    ESPUI.updateControlValue(chargerTimeFeedback, buffer);

    snprintf(buffer, sizeof(buffer), "%.2f h", batt.calculateChargeTime(batt.battery.ecoVoltPrecent, batt.battery.boostVoltPrecent));
    ESPUI.updateControlValue(chargerTimespan, buffer);

    snprintf(buffer, sizeof(buffer), "%d", batt.battery.heater.resistance);
    ESPUI.updateControlValue(heaterNum, buffer);

    snprintf(buffer, sizeof(buffer), "%s", batt.battery.name.c_str());
    ESPUI.updateControlValue(nameLabel, buffer);

    snprintf(buffer, sizeof(buffer), "http://%s.local", batt.battery.name.c_str());
    ESPUI.updateControlValue(localAddr, buffer);
}


/*
struct ControlData {
    uint16_t ecoTempNum;
    uint16_t ecoVoltNum;
    uint16_t boostTemp;
    uint16_t boostVolts;
    uint16_t seriesConfigNum;
    uint16_t ecoVoltLabel;
    uint16_t boostVoltLabel;
    uint16_t text10;
    uint16_t text12;
    uint16_t chargerTimeFeedback;
    uint16_t chargerTimespan;
    uint16_t heaterNum;
    uint16_t nameLabel;
    uint16_t localAddr;
    uint16_t voltSwitcher;
    uint16_t tempSwitcher;
    uint16_t mqttEnable;
    uint16_t tgEnable;
    uint16_t httpEnable;

    void updateValues() {
        char buffer[50]; // Buffer for formatted strings

        // Update control values using snprintf
        snprintf(buffer, sizeof(buffer), "%c", batt.battery.heater.ecoTemp);
        ESPUI.updateControlValue(ecoTempNum, String(buffer));

        snprintf(buffer, sizeof(buffer), "%c", batt.battery.ecoVoltPrecent);
        ESPUI.updateControlValue(ecoVoltNum, String(buffer));

        snprintf(buffer, sizeof(buffer), "%c", batt.battery.heater.boostTemp);
        ESPUI.updateControlValue(boostTemp, String(buffer));

        snprintf(buffer, sizeof(buffer), "%c", batt.battery.boostVoltPrecent);
        ESPUI.updateControlValue(boostVolts, String(buffer));

        snprintf(buffer, sizeof(buffer), "%c", batt.battery.size);
        ESPUI.updateControlValue(seriesConfigNum, String(batt.battery.size));

        snprintf(buffer, sizeof(buffer), "%c V", batt.btryToVoltage(batt.battery.ecoVoltPrecent));
        ESPUI.updateControlValue(ecoVoltLabel, String(buffer));

        snprintf(buffer, sizeof(buffer), "%c V", batt.btryToVoltage(batt.battery.boostVoltPrecent));
        ESPUI.updateControlValue(boostVoltLabel, String(buffer));

        snprintf(buffer, sizeof(buffer), "%c", batt.battery.chrgr.current);
        ESPUI.updateControlValue(text10, String(buffer));

        snprintf(buffer, sizeof(buffer), "%d", batt.battery.capct);
        ESPUI.updateControlValue(text12, String(buffer));

        snprintf(buffer, sizeof(buffer), "%.1f h", batt.calculateChargeTime(batt.battery.ecoVoltPrecent, batt.battery.boostVoltPrecent));
        ESPUI.updateControlValue(chargerTimeFeedback, String(buffer));
        ESPUI.updateControlValue(chargerTimespan, String(buffer));

        // snprintf(buffer, sizeof(buffer), "%.2f h", batt.calculateChargeTime(batt.getEcoPrecentVoltage(), batt.getBoostPrecentVoltage()));
        // ESPUI.updateControlValue(chargerTimespan, String(buffer));

        snprintf(buffer, sizeof(buffer), "%d", batt.battery.heater.resistance);
        ESPUI.updateControlValue(heaterNum, String(buffer));

        snprintf(buffer, sizeof(buffer), "%s", batt.battery.name.c_str());
        ESPUI.updateControlValue(nameLabel, String(buffer));

        snprintf(buffer, sizeof(buffer), "http://%s.local", batt.battery.name.c_str());
        ESPUI.updateControlValue(localAddr, String(buffer));


        // Update switchers
        ESPUI.updateSwitcher(voltSwitcher, batt.getActivateVoltageBoost());
        ESPUI.updateSwitcher(tempSwitcher, batt.getActivateTemperatureBoost());
        ESPUI.updateSwitcher(mqttEnable, batt.battery.mqtt.enable);
        ESPUI.updateSwitcher(tgEnable, batt.battery.telegram.enable;);
        ESPUI.updateSwitcher(httpEnable, batt.battery.http.enable);
    }
};

ControlData controlData;
*/

struct UIData {
    char wlanIpAddress[16];      // Sufficient size for an IP address
    char uptime[20];             // Sufficient size for uptime string
    char batteryDOD[10];         // Sufficient size for battery DOD percentage
    char temperature[10];        // Sufficient size for temperature
    char boostVoltage[10];       // Sufficient size for boost voltage
    char quickPanelVoltage[10];  // Sufficient size for quick panel voltage
    char chargerTimespan[10];    // Sufficient size for charger timespan
    char autoSeriesNum[10];      // Sufficient size for auto series number
    char heatPower[10];          // Sufficient size for heat power
    char heatStatus[10];         // Sufficient size for heat status
    char calibStatus[50];        // Sufficient size for calibration status
    char ecoVoltage[10];         // Sufficient size for eco voltage
    char boostVoltageLabel[10];  // Sufficient size for boost voltage label
};

void preprocessUIData(UIData &data) {
    snprintf(data.wlanIpAddress, sizeof(data.wlanIpAddress), "%s", WiFi.localIP().toString().c_str());
    snprintf(data.uptime, sizeof(data.uptime), "%lu min", millis() / 60000);
    snprintf(data.batteryDOD, sizeof(data.batteryDOD), "%d %%", batt.battery.voltageInPrecent);
    snprintf(data.temperature, sizeof(data.temperature), "%.1f ℃", batt.battery.temperature);
    snprintf(data.boostVoltage, sizeof(data.boostVoltage), "%.0f V", batt.btryToVoltage(batt.battery.boostVoltPrecent));
    snprintf(data.quickPanelVoltage, sizeof(data.quickPanelVoltage), "%.1f V", batt.battery.milliVoltage / 1000.0);
    snprintf(data.chargerTimespan, sizeof(data.chargerTimespan), "%.2f h", batt.calculateChargeTime(batt.battery.ecoVoltPrecent, batt.battery.boostVoltPrecent));
    snprintf(data.autoSeriesNum, sizeof(data.autoSeriesNum), "%d", batt.getBatteryApprxSize());
    snprintf(data.heatPower, sizeof(data.heatPower), "%.1f W", batt.battery.heater.maxPower * batt.battery.heater.pidOutput / batt.battery.heater.powerLimit);
    snprintf(data.heatStatus, sizeof(data.heatStatus), "%s", batt.battery.init ? "Ok" : "Fail");
    snprintf(data.calibStatus, sizeof(data.calibStatus), "%s (P: %.2f I: %.2f D: %.2f)", batt.battery.stune.done ? "Ok" : "Fail", batt.battery.heater.pidP, batt.battery.heater.pidI, batt.battery.heater.pidD);
    snprintf(data.ecoVoltage, sizeof(data.ecoVoltage), "%.1f V", batt.btryToVoltage(batt.battery.ecoVoltPrecent));
    snprintf(data.boostVoltageLabel, sizeof(data.boostVoltageLabel), "%.1f V", batt.btryToVoltage(batt.battery.boostVoltPrecent));
}

void updateUILabels(const UIData &data) {
    ESPUI.updateLabel(ipText,           data.wlanIpAddress);
    ESPUI.updateLabel(ipLabel,          data.wlanIpAddress);
    ESPUI.updateLabel(labelId,          data.uptime);
    ESPUI.updateLabel(voltLabel,        data.batteryDOD);
    ESPUI.updateLabel(tempLabel,        data.temperature);
    ESPUI.updateLabel(boostVoltLabel,   data.boostVoltage);
    ESPUI.updateLabel(quickPanelVoltage, data.quickPanelVoltage);
    ESPUI.updateLabel(chargerTimespan,  data.chargerTimespan);
    ESPUI.updateLabel(autoSeriesNum,     data.autoSeriesNum);
    ESPUI.updateLabel(heatPow,           data.heatPower);
    ESPUI.updateLabel(heatOn,             data.heatStatus);
    ESPUI.updateLabel(calibPass,          data.calibStatus);
    ESPUI.updateLabel(ecoVoltLabel,       data.ecoVoltage);
    ESPUI.updateLabel(boostVoltLabel,     data.boostVoltageLabel);
}


//Function Prototypes for ESPUI
void connectWifi();
void setUpUI();
void textCallback(Control *sender, int type);
void paramCallback(Control* sender, int type, int param);
void generalCallback(Control *sender, int type);

void httpEnableCallback(Control *sender, int type);
void mqttEnableCallback(Control *sender, int type);
void telegramEnableCallback(Control *sender, int type);

// Battery callbacks for Switcher input  --> Boost & Temp
void boostTempSwitcherCallback(Control *sender, int type);
void boostVoltageSwitcherCallback(Control *sender, int type);

String getVoltageStateName(VoltageState state);
String getTempStateName(TempState state);

String httpUserAcc = "batt";
String httpPassAcc = "ass";
bool httpEn = false;
bool mqttEn = false;
bool telegEn = false;









UIData uiData; // Stack-allocated struct to hold UI data

void setup() {

	  Serial.begin(115200);
    batt.loadSettings(ALL);
    
    WiFi.setHostname(batt.battery.name.c_str());  // needs to be before setUpUI!!   

	  while(!Serial);

	  if(SLOW_BOOT) delay(5000); //Delay booting to give time to connect a serial monitor

	  connectWifi();

	  WiFi.setSleep(true); //For the ESP32: turn off sleeping to increase UI responsivness (at the cost of power use)

	  setUpUI();
}   


void setUpUI() {
  ESPUI.setVerbosity(Verbosity::Verbose);
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


  headerTimeLabel     = ESPUI.addControl(Label, "Charger Time", "Time", None, vertgroupswitcher);
                        ESPUI.setElementStyle(headerTimeLabel, "width: 25%; font-size: small; background-color: unset; color: lightgrey");

  headerTempLabel     = ESPUI.addControl(Label, "TempLabel", "Temp", None, vertgroupswitcher);
                        ESPUI.setElementStyle(headerTempLabel, "width: 25%; font-size: small; background-color: unset; color: lightgrey");

  headerVoltage       = ESPUI.addControl(Label, "QuickbtrToVolts", "Voltage", None, vertgroupswitcher);  // NewlineLabel
                        ESPUI.setElementStyle(headerVoltage, "width: 25%; font-size: small; background-color: unset; color: lightgrey");

  headerPrecent       = ESPUI.addControl(Label, "VoltLabel", "Battery", None, vertgroupswitcher);
                        ESPUI.setElementStyle(headerPrecent, "width: 25%; font-size: small; background-color: unset; color: lightgrey");

  chargerTimeFeedback = ESPUI.addControl(Label, "Charger Time", "0.00", None, vertgroupswitcher);
                        ESPUI.setElementStyle(chargerTimeFeedback, switcherLabelStyle);

  tempLabel           = ESPUI.addControl(Label, "TempLabel", "0", None, vertgroupswitcher);
                        ESPUI.setElementStyle(tempLabel , switcherLabelStyle);
    
  quickPanelVoltage   = ESPUI.addControl(Label, "QuickbtrToVolts", "0", None, vertgroupswitcher);  // NewlineLabel
                        ESPUI.setElementStyle(quickPanelVoltage, switcherLabelStyle);

  voltLabel           = ESPUI.addControl(Label, "VoltLabel", "0", None, vertgroupswitcher);
                        ESPUI.setElementStyle(voltLabel, switcherLabelStyle);

                        ESPUI.setElementStyle(ESPUI.addControl(Label, "emptyLine", "", None, vertgroupswitcher), clearLabelStyle);  // NewlineLabel
  
  extraLabel          = ESPUI.addControl(Label, "ExtraLabel", "Temp <br>Boost <br>", None, vertgroupswitcher); 
                        ESPUI.setVertical(extraLabel);
                        ESPUI.setElementStyle(extraLabel, "width: 25%; background-color: unset; text-align: center  ;");

  tempSwitcher        = ESPUI.addControl(Switcher, "", "", None, vertgroupswitcher, boostTempSwitcherCallback);
                        ESPUI.setVertical(tempSwitcher);
                       

  voltSwitcher        = ESPUI.addControl(Switcher, "", "", None, vertgroupswitcher, boostVoltageSwitcherCallback);
                        ESPUI.setVertical(voltSwitcher);
                       
                        

  ultraLabel          = ESPUI.addControl(Label, "", "Voltage <br>Boost <br>", None, vertgroupswitcher);  // NewlineLabel
                        ESPUI.setVertical(ultraLabel);
                        ESPUI.setElementStyle(ultraLabel, "width: 25%; background-color: unset; text-align: center  ;");

                        ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, vertgroupswitcher), clearLabelStyle);  // NewlineLabel

/*
      STATS like IP and uptime
*/
  statsLabel         = ESPUI.addControl(Label, "Stats", "", None, tab1);
                        ESPUI.setElementStyle(statsLabel, "background-color: unset; width: 100%; color: white; font-size: small; ");

  localHosti        =   ESPUI.addControl(Label, "Hostname", "", None, statsLabel);
                       ESPUI.setElementStyle(localHosti, "background-color: unset; width: 15%; color: white; font-size: small; text-align: left; ");

  localAddr         = ESPUI.addControl(Label, "IP", "http://helmi.local", None, statsLabel);
                      ESPUI.setElementStyle(localAddr, "background-color: transparent; border: 1px solid white; text-align: center; font-size: normal; text-padding: 2px; -webkit-appearance: none; -moz-appearance: textfield; width: 70%; letter-spacing: 1px;");

  localHosti2        =   ESPUI.addControl(Label, "Hostname", "", None, statsLabel);
                       ESPUI.setElementStyle(localHosti2, "background-color: unset; width: 15%; color: white; font-size: small; text-align: left; ");


  uptimeLabel =       ESPUI.addControl(Label, "Uptime:", "Uptime", None, statsLabel);
                        ESPUI.setElementStyle(uptimeLabel, "background-color: unset; width: 25%; color: white; font-size: small; text-align: left;  ");

  labelId           = ESPUI.addControl(Label, "Uptime:", "", None, statsLabel);
                      ESPUI.setElementStyle(labelId, "background-color: unset; width: 75%; color: white; font-size: small;  ");

  ipaddrLabel       = ESPUI.addControl(Label, "IP", "IP", None, statsLabel);
                        ESPUI.setElementStyle(ipaddrLabel, "background-color: unset; width: 25%; color: white; font-size: small; text-align: left; ");

  ipText            = ESPUI.addControl(Label, "IP", "ipAddress", None, statsLabel);
                      ESPUI.setElementStyle(ipText, "background-color: unset; width: 75%; color: white; font-size: small;  ");
                  


  heatOnLabel     = ESPUI.addControl(Label, "", "Heater", None, statsLabel);
                      ESPUI.setElementStyle(heatOnLabel, "background-color: unset; width: 25%; color: white; font-size: small; text-align: left; ");

  heatOn          = ESPUI.addControl(Label, "", "Disabled", None, statsLabel);
                      ESPUI.setElementStyle(heatOn, "background-color: unset; width: 75%; color: white; font-size: small;");


  calibName        = ESPUI.addControl(Label, "", "Calibration", None, statsLabel);
                      ESPUI.setElementStyle(calibName, "background-color: unset; width: 25%; color: white; font-size: small; text-align: left; ");

  calibPass         = ESPUI.addControl(Label, "", "Disabled", None, statsLabel);
                      ESPUI.setElementStyle(calibPass, "background-color: unset; width: 75%; color: white; font-size: small;");


  heatName        =   ESPUI.addControl(Label, "", "Power", None, statsLabel);
                        ESPUI.setElementStyle(heatName, "background-color: unset; width: 25%; color: white; font-size: small; text-align: left; ");

  heatPow         = ESPUI.addControl(Label, "", "Disabled", None, statsLabel);
                      ESPUI.setElementStyle(heatPow, "background-color: unset; width: 75%; color: white; font-size: small;");


                      ESPUI.setElementStyle(ESPUI.addControl(Label, "emptyLine", "", None, statsLabel), clearLabelStyle);  // NewlineLabel


/*

  LOGGING OF STATES

*/

  // logTopic            = ESPUI.addControl(Label, "logging", "Logs", None, statsLabel);
  //                        ESPUI.setElementStyle(logTopic, "background-color: unset; width: 100%; color: white; font-size: small; ");

  logSeparator        = ESPUI.addControl(Label, "Histor", "Logs", Emerald, statsLabel);
                        ESPUI.setElementStyle(logSeparator, "text-align: left; font-size: small; font-family: serif; width: 100%; background-color: unset; color: white; margin-top: 10px; border-bottom: 1px solid darkslategray; padding-bottom: 5px;");

  logLabel            = ESPUI.addControl(Label, "Log", "", None, statsLabel);
                        ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, logLabel), clearLabelStyle);  // NewlineLabel

  firstLogTime       = ESPUI.addControl(Label, "Time", "", None, statsLabel);
                        ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, firstLogTime), clearLabelStyle);  // NewlineLabel
                        ESPUI.setElementStyle(firstLogTime, "background-color: unset; width: 20%; color: white; font-size: small; ");
  firstLogLabel       = ESPUI.addControl(Label, "FirstLog", "", None, statsLabel);
                        ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, firstLogLabel), clearLabelStyle);  // NewlineLabel
                        ESPUI.setElementStyle(firstLogLabel, "background-color: unset; width: 80%; color: white; font-size: small; ");

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
                      ESPUI.setElementStyle(ESPUI.addControl(Label, "C", "", Emerald, group1), "font-family: serif; width: 25%; text-align: center; background-color: unset;color: darkslategray;");

// Add Battery Series Configuration
textSeriesConfig = ESPUI.addControl(Label, "Battery Series Configuration", "Series Config", None, group1);
                  ESPUI.setElementStyle(textSeriesConfig, "background-color: unset; width: 50%; text-align: left; font-size: small; color: darkslategray;");

seriesConfigNum = ESPUI.addControl(Number, "Series Configuration", "13", Emerald, group1, generalCallback);           // selectBattery);
                  ESPUI.addControl(Min, "", "7", None, seriesConfigNum);
                  ESPUI.addControl(Max, "", "21", None, seriesConfigNum);
                  ESPUI.setElementStyle(seriesConfigNum, "background-color: transparent; width: 25%; border: none; text-align: center; ; -webkit-appearance: none; -moz-appearance: textfield;");

                  ESPUI.setElementStyle(ESPUI.addControl(Label, "C", "", Emerald, group1), "font-family: serif; width: 25%; text-align: center; background-color: unset;color: darkslategray;");

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

      // const char* hostname = ESPUI.getControl(nameLabel)->value();

        #ifdef MAINDEBUG
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
        Serial.println(ESPUI.getControl(nameLabel)->value);
        Serial.println("getControls..");
        #endif

        batt.battery.name = ESPUI.getControl(nameLabel)->value;                    // Serial.println("Hostname set");

        if(batt.setNominalString(ESPUI.getControl(seriesConfigNum)->value.toInt()));         // Serial.println("Nominal string set");

        if(batt.setEcoTemp(ESPUI.getControl(ecoTempNum)->value.toInt()));                    // Serial.println("Eco temp set");

        if(batt.setEcoPrecentVoltage(ESPUI.getControl(ecoVoltNum)->value.toInt()));          // Serial.println("Eco precent voltage set");

        if(batt.setBoostTemp(ESPUI.getControl(boostTemp)->value.toInt()));                   // Serial.println("Boost temp set");

        if(batt.setBoostPrecentVoltage(ESPUI.getControl(boostVolts)->value.toInt()));        // Serial.println("Boost precent voltage set");

        if(batt.setCharger(ESPUI.getControl(text10)->value.toInt()));                        // Serial.println("Charger set");

        if(batt.setResistance(ESPUI.getControl(heaterNum)->value.toInt()));                  // Serial.println("Resistance set");

        if(batt.setCapacity(ESPUI.getControl(text12)->value.toInt()));                       // Serial.println("Capacity set");

        batt.saveSettings(SETUP); 

      }
    }
);


   

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
        /*
        Serial.println("Saving credentials to NVS...");
        Serial.println(ESPUI.getControl(wifi_ssid_text)->value);
        Serial.println(ESPUI.getControl(wifi_pass_text)->value);
        */

        preferences.begin("btry", false); // "wifi" is the namespace, false means read/write
        preferences.putString("wssid", ESPUI.getControl(wifi_ssid_text)->value);
        preferences.putString("wpass", ESPUI.getControl(wifi_pass_text)->value);
        preferences.end(); // Close Preferences  
      }
      });
  ESPUI.setElementStyle(wifiButton,"width: 20%; text-align: center; font-size: medium; font-family: serif; margin-top: 20px; margin-bottom: 20px; border-radius: 15px;");

 //  clearLabelStyle = "background-color: unset; width: 100%;";
 //  switcherLabelStyle = "width: 25%; background-color: unset;";
//ESPUI.separator("HTTP");
// ----- HTTP 

auto      httpLabel    = ESPUI.addControl(Label, "HTTP", "Enable HTTP", Peterriver, wifitab, generalCallback);

          httpEnable   = ESPUI.addControl(Switcher, "Enable HTTP", "", None, httpLabel, httpEnableCallback);
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
        /*
        Serial.println("Saving battery structs...");
        Serial.println(ESPUI.getControl(httpUser)->value);  
        Serial.println(ESPUI.getControl(httpPass)->value); 
        Serial.println(ESPUI.getControl(httpEnable)->value);
        */

        // Open Preferences with a namespace
        preferences.begin("btry", false); // "wifi" is the namespace, false means read/write
        preferences.putString("httpusr", ESPUI.getControl(httpUser)->value);
        preferences.putString("httppass", ESPUI.getControl(httpPass)->value);
        preferences.putBool("httpen", ESPUI.getControl(httpEnable)->value);
        preferences.end(); // Close Preferences  
        } });

        ESPUI.setElementStyle(httpButton, "width: 20%; text-align: center; font-size: medium; font-family: serif; margin-top: 20px; margin-bottom: 20px; border-radius: 15px;");

// ESPUI.separator("MQTT");
//  --- MQTT
        
 auto   mqttLabel    = ESPUI.addControl(Label, "MQTT", "Enable MQTT", Peterriver, wifitab, generalCallback);
                        ESPUI.setElementStyle(mqttLabel, "font-family: serif; background-color: unset; width: 100%;");

// MQTT ENABLE
        mqttEnable   = ESPUI.addControl(Switcher, "Enable MQTT", "", None, mqttLabel, mqttEnableCallback);
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
        /*
        Serial.println("Saving battery structs...");
        Serial.println(ESPUI.getControl(mqttEnable)->value);
        Serial.println(ESPUI.getControl(mqttUser)->value); 
        Serial.println(ESPUI.getControl(mqttPass)->value);
        Serial.println(ESPUI.getControl(mqttIp)->value);
        */

                // Open Preferences with a namespace
        preferences.begin("btry", false); // "btry" is the namespace, false means read/write
        preferences.putString("mqttusr",  ESPUI.getControl(mqttUser)->value);
        preferences.putString("mqttpass", ESPUI.getControl(mqttPass)->value);
        preferences.putString("mqttip",   ESPUI.getControl(mqttIp)->value);
        preferences.putBool("mqtten",   batt.battery.mqtt.enable);
        preferences.end(); // Close Preferences  

        } });
        ESPUI.setElementStyle(mqttButton, "width: 20%; text-align: center; font-size: medium; font-family: serif; margin-top: 20px; margin-bottom: 20px; border-radius: 15px;");


//  --- Telegram
      
auto  tgLabel    = ESPUI.addControl(Label, "Telegram", "Enable Telegram", Peterriver, tgtab, generalCallback);
                    ESPUI.setElementStyle(tgLabel, "font-family: serif; background-color: unset; width: 100%; ");

      tgEnable   = ESPUI.addControl(Switcher, "Enable Telegram", "", None, tgLabel, telegramEnableCallback);
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
      /*
        Serial.println("Saving telegram structs...");
        Serial.println(ESPUI.getControl(tgEnable)->value); 
        Serial.println(ESPUI.getControl(tgUser)->value);  
        Serial.println(ESPUI.getControl(tgToken)->value);
        */ 

        preferences.begin("btry", false); // "wifi" is the namespace, false means read/write
        preferences.putString("tgusr",    ESPUI.getControl(tgUser)->value);
        preferences.putString("tgtoken",  ESPUI.getControl(tgToken)->value);
        preferences.putBool("tgen",       batt.battery.telegram.enable);
        preferences.end(); // Close Preferences  

    } });
  ESPUI.setElementStyle(tgButton,"width: 20%; text-align: center; font-size: medium; font-family: serif; margin-top: 20px; margin-bottom: 20px; border-radius: 15px;");

        
auto  resetLabel    = ESPUI.addControl(Label, "Back toFactory Settings", "Reset Settings", Peterriver, tgtab, generalCallback);
                      ESPUI.setElementStyle(resetLabel, "font-family: serif; background-color: unset; width: 100%; ");

resetButton         = ESPUI.addControl(Button, "Memory", "Reset", None, resetLabel, [](Control *sender, int type) {
                     if (type == B_UP) {
                       Serial.println("Resetting settings...");
                       batt.resetSettings(true);
                     } });
                    ESPUI.setElementStyle(resetButton,"width: 20%; text-align: center; font-size: medium; font-family: serif; margin-top: 20px; margin-bottom: 20px; border-radius: 15px;");
  
  /*

    Tab: WiFi Credentials
    You use this tab to enter the SSID and password of a wifi network to autoconnect to.
   
   *-----------------------------------------------------------------------------------------------------------*/
  
  
  /*
    ESPUI.updateControlValue(ecoTempNum, String(batt.getEcoTemp()));
    ESPUI.updateControlValue(ecoVoltNum, String(batt.getEcoPrecentVoltage()));
    ESPUI.updateControlValue(boostTemp, String(batt.getBoostTemp()));
    ESPUI.updateControlValue(boostVolts, String(batt.getBoostPrecentVoltage()));
    ESPUI.updateControlValue(seriesConfigNum, String(batt.getNominalString()));
    ESPUI.updateControlValue(ecoVoltLabel, String(batt.btryToVoltage(batt.getEcoPrecentVoltage()), 1) + " V");
    ESPUI.updateControlValue(boostVoltLabel, String(batt.btryToVoltage(batt.getBoostPrecentVoltage()), 1) + " V");
    ESPUI.updateControlValue(text10, String(batt.getCharger()));
    ESPUI.updateControlValue(text12, String(batt.getCapacity()));
    ESPUI.updateControlValue(chargerTimeFeedback, String(batt.calculateChargeTime(batt.getEcoPrecentVoltage(), batt.getBoostPrecentVoltage()), 1) + " h");
    ESPUI.updateControlValue(chargerTimespan, String(batt.calculateChargeTime(batt.getEcoPrecentVoltage(), batt.getBoostPrecentVoltage()), 2) + " h");
    ESPUI.updateControlValue(heaterNum, String(batt.getResistance()));
    ESPUI.updateControlValue(nameLabel, String(batt.battery.name));
    ESPUI.updateControlValue(localAddr, String("http://" + batt.battery.name + ".local"));
*/

    //controlData.updateValues(); 

    updateControlValues();

    ESPUI.updateSwitcher(voltSwitcher, batt.battery.voltBoost);
    ESPUI.updateSwitcher(tempSwitcher, batt.battery.tempBoost);
    ESPUI.updateSwitcher(mqttEnable, batt.battery.mqtt.enable);
    ESPUI.updateSwitcher(tgEnable, batt.battery.telegram.enable);
    ESPUI.updateSwitcher(httpEnable, batt.battery.http.enable);



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
	The numbers value button, for defining a boost voltage level in voltage
*/
void mqttEnabelCallBack(Control *sender, int type) {
        switch (type) {
    case S_ACTIVE:
          batt.battery.mqtt.enable = true;
        break;
	case S_INACTIVE:
          batt.battery.mqtt.enable = false;
		break;
  default:
      Serial.print(type);
      Serial.println("unknown type: HTTP en CB");
      break;
    }
}



void httpEnableCallback(Control *sender, int type) {
  switch (type) {
    case S_ACTIVE:
          batt.battery.http.enable = true;
            preferences.begin("btry", false);
            preferences.putBool("httpen", true);
            preferences.end();
        break;
	case S_INACTIVE:
          batt.battery.http.enable = false;
            preferences.begin("btry", false);
            preferences.putBool("httpen", false);
            preferences.end();
		break;
  default:
      Serial.print(type);
      Serial.println("unknown type: HTTP en CB");
      break;
    }
}

void mqttEnableCallback(Control *sender, int type) {
  switch (type) {
    case S_ACTIVE:
          batt.battery.mqtt.enable = true;
            preferences.begin("btry", false);
            preferences.putBool("mqtten", true);
            preferences.end();
        break;
	case S_INACTIVE:
          batt.battery.mqtt.enable = false;
            preferences.begin("btry", false);
            preferences.putBool("mqtten", false);
            preferences.end();
		break;
  default:
      Serial.print(type);
      Serial.println("unknown type: MQTT en CB");
      break;
    }
}

void telegramEnableCallback(Control *sender, int type) {
  switch (type) {
    case S_ACTIVE:
          batt.battery.telegram.enable = true;
            preferences.begin("btry", false);
            preferences.putBool("tgen", true);
            preferences.end();

            //telegramSetup();
        break;
	case S_INACTIVE:
          batt.battery.telegram.enable = false;
            preferences.begin("btry", false);
            preferences.putBool("tgen", false);
            preferences.end();
		break;
  default:
      Serial.print(type);
      Serial.println("unknown type: Telegram en CB");
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









void loop() {

  batt.loop(); // Add battery.loop() here

  if(millis() - mittausmillit >= 3000)
  {
      mittausmillit = millis();

      preprocessUIData(uiData); // Preprocess data

      updateUILabels(uiData); // Update labels with preprocessed data

      if(batt.battery.voltBoost) {
        ESPUI.updateLabel(chargerTimeFeedback, String(batt.calculateChargeTime(batt.battery.voltageInPrecent, batt.battery.boostVoltPrecent), 2) + " h");
      }
      else {
        ESPUI.updateLabel(chargerTimeFeedback, String(batt.calculateChargeTime(batt.battery.voltageInPrecent, batt.battery.ecoVoltPrecent), 2) + " h");
      }
      






      /*
      String wlanIpAddress;
      wlanIpAddress = WiFi.localIP().toString();
      ESPUI.updateLabel(ipText, wlanIpAddress);
      ESPUI.updateLabel(ipLabel, wlanIpAddress);
      ESPUI.updateLabel(labelId, String(millis() / 60000) + " min");
      ESPUI.updateLabel(voltLabel, String(batt.getBatteryDODprecent()) + " %");
      ESPUI.updateLabel(tempLabel, String(batt.getTemperature(), 1) + " ℃");

      ESPUI.updateLabel(boostVoltLabel, String(batt.btryToVoltage(batt.getBoostPrecentVoltage()), 0) + " V");
      ESPUI.updateLabel(quickPanelVoltage, String(batt.getCurrentVoltage(), 1) + " V");
      ESPUI.updateLabel(chargerTimespan, String(batt.calculateChargeTime(batt.getEcoPrecentVoltage(), batt.getBoostPrecentVoltage()), 2) + " h");
      ESPUI.updateLabel(autoSeriesNum, String(batt.getBatteryApprxSize()));
      ESPUI.updateLabel(heatPow, String(batt.battery.heater.maxPower * batt.battery.heater.pidOutput / batt.battery.heater.powerLimit) + " W");
      ESPUI.updateLabel(heatOn, String(batt.battery.init ? "Ok" : "Fail"));
      ESPUI.updateLabel(calibPass, String(batt.battery.stune.done ? "Ok" : "Fail"));
      ESPUI.updateLabel(ecoVoltLabel, String(batt.btryToVoltage(batt.getEcoPrecentVoltage()), 1) + " V");
      ESPUI.updateLabel(boostVoltLabel, String(batt.btryToVoltage(batt.getBoostPrecentVoltage()), 1) + " V");
      */


      // Check and log voltage state
      if (batt.battery.vState != batt.battery.prevVState) {
          logEntries += String(getVoltageStateName(batt.battery.vState)) + "\n";
          logTime += String(millis() / 60000) + "  min" + "\n";
      }
      // Check and log temperature state
      if (batt.battery.tState != batt.battery.prevTState) {
          logEntries += String(getTempStateName(batt.battery.tState)) + "\n";
          logTime += String(millis() / 60000) + "  min" + "\n";
      }

      ESPUI.updateLabel(firstLogLabel, logEntries);
      ESPUI.updateLabel(firstLogTime, logTime);
    }

}

void connectWifi() {
	int connect_timeout;

	WiFi.setHostname(batt.battery.name.c_str());

	//Load credentials from EEPROM 
	if(!(FORCE_USE_HOTSPOT)) {
		yield();
			WiFi.begin(batt.battery.wlan.ssid.c_str(), batt.battery.wlan.pass.c_str());
    Serial.println("Connecting to WiFi...");
		connect_timeout = 28; //7 seconds
		while (WiFi.status() != WL_CONNECTED && connect_timeout > 0) {
			delay(250);
			connect_timeout--;
		}
	}
	
	if (WiFi.status() == WL_CONNECTED) {
		//SPLN("Wifi started");
    Serial.println("IP: " + WiFi.localIP().toString());

		if (!MDNS.begin(batt.battery.name.c_str())) {
			//SP("Error setting up MDNS responder!");
		}
	} else {
		//SP("Creating access point...");
		WiFi.mode(WIFI_AP);
		WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
		WiFi.softAP(HOSTNAME);

		connect_timeout = 20;
		do {
			delay(250);
			connect_timeout--;
		} while(connect_timeout);
	}
}

void textCallback(Control *sender, int type) {
	//This callback is needed to handle the changed values, even though it doesn't do anything itself.
}

// Most elements in this test UI are assigned this generic callback which prints some
// basic information. Event types are defined in ESPUI.h
// The extended param can be used to pass additional information
void paramCallback(Control* sender, int type, int param)
{
  /*
   SP("CB: id(");
   SP(sender->id);
   SP(") Type(");
   SP(type);
   SP(") '");
   SP(sender->label);
   SP("' = ");
   SP(sender->value);
   SPLN(param);
   */
}

String getVoltageStateName(VoltageState state) {
    switch (state) {
        case ALERT: return "ALERT";
        case WARNING: return "WARNING";
        case LOVV: return "Low Voltage";
        case ECO: return "Eco Voltage";
        case BOOST: return "Boost Voltage";
        case FULL: return "Full Battery";
        default: return "UNKNOWN";
    }
}

String getTempStateName(TempState state) {
    switch (state) {
        case SUBZERO: return "SUBZERO TEMP WARNING";
        case COLD: return "Cold Temp";
        case ECO_TEMP: return "Eco Temp";
        case ECO_READY: return "Eco Temp Ready"; 
        case BOOST_TEMP: return "Boost Temp";
        case BOOST_READY: return "Boost Temp Ready";
        case TEMP_WARNING: return "Temp Warning";
        case UNKNOWN_TEMP: return "UNKNOWN TEMP ALERT";
        default: return "ERROR";
    }
}