// #include <driver/adc.h>
// #include <esp_adc_cal.h>
#include <esp_task_wdt.h>
// #include <BluetoothSerial.h>
// #include <PubSubClient.h>

#include <Arduino.h>
#include <QuickPID.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <EEPROM.h>
#include "Battery/Battery.h"
#include "Battery/Battery.cpp"
#include <NonBlockingDallas.h>
#include "Blinker.h"
#include <AsyncTCP.h>
#include <ESPUI.h>
#include <ESPmDNS.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <esp_task_wdt.h>

//Settings
#define SLOW_BOOT 0
#define HOSTNAME "badass"
#define FORCE_USE_HOTSPOT 0
#define USERHOSTNAME "veijo"

#define V_REF 1100
#define WDT_TIMEOUT 60    
#define EEPROM_SIZE 120  // This is 1-Byte  
#define NFETCH 4  // Mosfet's channel
#define NFETPIN 32 // Mosfets pin! --> RTC_GPIO9

// Moving average variables for ADC readings on battery voltage
#define MOVING_AVG_SIZE 5

// Quickpid Variables
bool  printOrPlotter = 0;   // on(1) monitor, off(0) plotter
float Setpoint;
float Input;
float Output;
float Kp = 12; 
float Ki = 0.003; 
float Kd = 0;  
float lampotila;
int batteryInSeries;

int tempest;

// LED's
#define greenpin 4
#define redpin 17
#define yellowpin 18

// time comparator variables. --> 
// going to deprectate these & move to taskscheduler?
unsigned long lampoMillis = 0; // Heat condifiton checking loop millis()
unsigned long mittausmillit = 0;  // battery voltage and heat reading millis()
unsigned long full_millis = 0; // Eco-or-Full battery millis counter
unsigned long reset_timer = 0;  // Watchdog reset timer, so nothing will get stuck.
unsigned long btserialmillis = 0;
unsigned long ledmillis = 0;
unsigned long currentMillis = 0;
unsigned long timedMillis = 0;  
unsigned long timerMillis = 0;


//Function Prototypes
void connectWifi();
void setUpUI();
void textCallback(Control *sender, int type);
void randomString(char *buf, int len);
void paramCallback(Control* sender, int type, int param);


void ecoVoltCallback(Control *sender, int value);
void boostVoltCallback(Control *sender, int value);
void generalCallback(Control *sender, int type);
void ecomodeCallback(Control *sender, int type);
void tempBoostCallback(Control *sender, int type);

//UI handles
uint16_t wifi_ssid_text, wifi_pass_text;
volatile bool updates = false;

// tabs in ESPUI
uint16_t tab1, tab2, tab3;

// Styles for ESPUI 
String clearLabelStyle, switcherLabelStyle;

uint16_t mainTime, battInfo, permInfo;

uint16_t labelId, APSwitcher, ipLabel, ipText;

// HTTPUSERACCESS
uint16_t httpUserAccess, httpUsername, httpPassword;

//ESPUI main page integers:
uint16_t mainLabel, voltLabel, tempLabel;
uint16_t tempBoostSwitcher, voltBoostSwitcher; 
uint16_t wifitab, maintab, ndtab;

//ESPUI Configs page integers:
uint16_t mainSwitcher, styleSlider2, styleSlider3, batteryName, mainSelector, permSwitcher;
uint16_t ecoTempNum, ecoVoltNum;
uint16_t boostTemp, boostVolts, nameLabel, hostnameLabel, ecoVoltLabel, ecoVoltUnitLabel, boostVoltUnitLabel, boostVoltLabel;

Battery batt;


void setUpUI() {

  //Turn off verbose debugging
  ESPUI.setVerbosity(Verbosity::VerboseJSON);

  Serial.println(WiFi.localIP());
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
  switcherLabelStyle = "width: 80px; margin-left: .6rem; margin-right: .6rem; background-color: unset;";

  ESPUI.addControl(Separator, "Quick controls", "", None);
  auto vertgroupswitcher = ESPUI.addControl(Label, "Boost", "Boost", Peterriver);
  ESPUI.setElementStyle(vertgroupswitcher, "background-color: unset;");
  ESPUI.setVertical(vertgroupswitcher); 
  ESPUI.setElementStyle( tempLabel = ESPUI.addControl(Label, "", "0", None, vertgroupswitcher), switcherLabelStyle);
  ESPUI.setElementStyle( voltLabel = ESPUI.addControl(Label, "", "0", None, vertgroupswitcher), switcherLabelStyle);
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, vertgroupswitcher), clearLabelStyle);  // NewlineLabel
  ESPUI.setVertical(ESPUI.addControl(Switcher, "", "0", None, vertgroupswitcher, generalCallback));
  ESPUI.setVertical(ESPUI.addControl(Switcher, "", "0", None, vertgroupswitcher, generalCallback));
  ESPUI.setElementStyle(ESPUI.addControl(Label, "", "", None, vertgroupswitcher), clearLabelStyle);  // NewlineLabel
  ESPUI.addControl(Separator, "", "", None);
 
  
 /*
  * First tab
  *_____________________________________________________________________________________________________________*/

  /* auto configs = ESPUI.addControl(Tab, "", "Configuration"); */

  auto infoLabel = ESPUI.addControl(Label, "How To Use", "Battery Assistant<br><br>How To:<br><br>For initial setup, the most important thing is to define your battery configuration. Is your battery a 7S or 16S defines how your charger is controlled.<ul><li>7S is 24V</li><li>10S is 36V</li><li>13S is 48V</li><li>16S is 60V</li><li>20S is 72V</li></ul><p>Get to know how to use this even more from the link below</p>", Peterriver, tab1, textCallback);
  ESPUI.setElementStyle(infoLabel, "font-family: serif; background-color: unset; width: 100%; text-align: left;");
  labelId = ESPUI.addControl(Label, "Uptime:", "", None, tab1);
  ESPUI.setElementStyle(labelId, "font-family: serif; background-color: unset; width: 100%; text-align: left;");
  ipText = ESPUI.addControl(Label, "IP", "ipAddress", ControlColor::None, tab1);
  /*

       Tab2, Basic required setup configurations.

   */
  auto nameLabel =  ESPUI.addControl(Text, "Hostname ", USERHOSTNAME, Alizarin, tab2, generalCallback);
                    ESPUI.setElementStyle(ESPUI.addControl(Label, "veijo.local", "URL:  veijo.local ", Alizarin, nameLabel), "background-color: unset; width: 100%; text-align: left;");
                    ESPUI.setElementStyle(ESPUI.addControl(Label, "ip", "IP: ", Alizarin, nameLabel), "background-color: unset; width: 50px; text-align: left;");
                    ESPUI.setElementStyle( ipLabel = ESPUI.addControl(Label, "", "IP", None, nameLabel), "background-color: unset; width: 50%; text-align: left;");
                                  
                    ESPUI.setElementStyle(nameLabel, "font-size: x-large; font-family: serif; width: 100%; text-align: center;");
                
                    static String optionValues[] {"0", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21"};
                    mainSelector = ESPUI.addControl(Select, "Battery Series Configuration", "Selectorin", Wetasphalt, tab2, selectBattery);
                    for(auto const& v : optionValues) {
                    ESPUI.addControl(Option, v.c_str(), v, None, mainSelector); }

                    ESPUI.setElementStyle(battInfo = ESPUI.addControl(Label, "Is your battery within these sepcifications?", "", None, mainSelector), "background-color: unset; width: 100%; text-align: left;");

  permSwitcher = ESPUI.addControl(Switcher, "Permanent heating disable", "", None, tab2, generalCallback);
                 ESPUI.setElementStyle(permInfo = ESPUI.addControl(Label, "Battery has a built in safety protocol to cut off heating when the battery is below 30%.", "", None, permSwitcher), "background-color: unset; width: 100%; text-align: left;");
            
  ESPUI.addControl(Separator, "Economy -mode", "", None, tab2);

                  ecoTempNum = ESPUI.addControl(Number, "Economy Temperature", "20", Turquoise, tab2, generalCallback);
                                    ESPUI.addControl(Min, "", "1", None, ecoTempNum);
                                    ESPUI.addControl(Max, "", "35", None, ecoTempNum);
                                    ESPUI.setElementStyle(ESPUI.addControl(Label, "C", "℃", Emerald, ecoTempNum), "font-family: serif; width: 60px; text-align: center; background-color: unset;");               

                  ecoVoltNum = ESPUI.addControl(Number, "Economy Charging", "70", Turquoise, tab2, ecoVoltCallback);
                                    ESPUI.addControl(Min, "", "50", None, ecoVoltNum);
                                    ESPUI.addControl(Max, "", "100", None, ecoVoltNum); 
                                    ESPUI.setElementStyle(ecoVoltUnitLabel = ESPUI.addControl(Label, "precent", "%   =", Emerald, ecoVoltNum), "font-family: serif; width: 50px; background-color: unset;");
                                    ESPUI.setElementStyle(ecoVoltLabel = ESPUI.addControl(Label, "Volts", "0", Emerald, ecoVoltNum), " text-align: right; font-size: large; font-family: serif; width: 50px; background-color: unset;"); 
                                    ESPUI.setElementStyle(ESPUI.addControl(Label, "", "V", None, ecoVoltNum), "font-family: serif; width: 30px; background-color: unset;");                                 
  ESPUI.addControl(Separator, "One-shot Economy override", "", None, tab2);

                  boostTemp = ESPUI.addControl(Number, "Boost Temperature", "20", Peterriver, tab2, generalCallback);
                                    ESPUI.addControl(Min, "", "1", None, boostTemp);
                                    ESPUI.addControl(Max, "", "35", None, boostTemp);
                                    ESPUI.setElementStyle(ESPUI.addControl(Label, "C", "℃", Emerald, boostTemp), "font-family: serif; width: 60px; text-align: center; background-color: unset;");                                       

                  boostVolts = ESPUI.addControl(Number, "Boost Charging", "90", Peterriver, tab2, boostVoltCallback);
                                    ESPUI.addControl(Min, "", "60", None, boostVolts);
                                    ESPUI.addControl(Max, "", "100", None, boostVolts);
                                    ESPUI.setElementStyle(boostVoltUnitLabel = ESPUI.addControl(Label, "precent", "%   =", Emerald, boostVolts), "font-family: serif; width: 50px; background-color: unset;");
                                    ESPUI.setElementStyle(boostVoltLabel = ESPUI.addControl(Label, "Volts", "0", Emerald, boostVolts), " text-align: right; font-size: large; font-family: serif; width: 50px; background-color: unset;");                                    
                                    ESPUI.setElementStyle(ESPUI.addControl(Label, "", "V", None, boostVolts), "font-family: serif; width: 30px; background-color: unset;");
  // Save to EEPROM
  ESPUI.addControl(Separator, "Mem", "Memory", None, tab2);
  ESPUI.addControl(Button, "Memory", "Save", None, tab2, generalCallback);

  APSwitcher = ESPUI.addControl(Switcher, "Run only as Access Point", "", None, tab3, generalCallback);

  httpUserAccess =  ESPUI.addControl(Label, "User Access for LAN", "HttpAccess", Peterriver, tab3, generalCallback);
  httpUsername =    ESPUI.addControl(Text, "Username", "", Dark, httpUserAccess, textCallback);
  httpPassword =    ESPUI.addControl(Text, "Password", "", Dark, httpUserAccess, textCallback);
  ESPUI.setElementStyle(httpUserAccess, "font-family: serif; background-color: unset;");

  /*
   
   
    Tab: WiFi Credentials
    You use this tab to enter the SSID and password of a wifi network to autoconnect to.
   
   
   *-----------------------------------------------------------------------------------------------------------*/
  auto wifitab = ESPUI.addControl(Tab, "", "WiFi");
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

  //Finally, start up the UI.
  //This should only be called once we are connected to WiFi.
  ESPUI.begin("  Battery Assistant: Veijo ", "lassi", "passu123");
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

void boostVoltCallback(Control* sender, int type) {

  int boostValue = 85;

  // Define the usable voltage range per cell
  const float min_voltage_per_cell = 3.0;  // 3V at 0%
  const float max_voltage_per_cell = 4.2;  // 4.2V at 100%

  // Calculate the voltage for one cell based on the percentage
  float percentage = boostValue;
  float cell_voltage = min_voltage_per_cell + ((percentage / 100) * (max_voltage_per_cell - min_voltage_per_cell));

  // Assume 'battery_string' is the number of cells in series, defined earlier in another function
  extern int batteryInSeries;
  Serial.println(batteryInSeries);
  // Calculate the total voltage for the whole battery string
  float total_voltage = cell_voltage * batteryInSeries;

  // Now you can use total_voltage in your application (e.g., to display or regulate power)
  // For example, print the total voltage (this could be sending it to a display, etc.)
  Serial.print("Voltage: ");
  Serial.println(total_voltage, 2);  // Print with 2 decimal places


  ESPUI.updateLabel(boostVoltLabel, String(total_voltage, 1));

}

void ecoVoltCallback(Control* sender, int value) {

  int boostValue = 65;

  // Define the usable voltage range per cell
  const float min_voltage_per_cell = 3.0;  // 3V at 0%
  const float max_voltage_per_cell = 4.2;  // 4.2V at 100%

  // Calculate the voltage for one cell based on the percentage
  float percentage = boostValue;
  float cell_voltage = min_voltage_per_cell + ((percentage / 100) * (max_voltage_per_cell - min_voltage_per_cell));

  // Assume 'battery_string' is the number of cells in series, defined earlier in another function
  extern int batteryInSeries;
  Serial.println(batteryInSeries);
  // Calculate the total voltage for the whole battery string
  float total_voltage = cell_voltage * batteryInSeries;

  // Now you can use total_voltage in your application (e.g., to display or regulate power)
  // For example, print the total voltage (this could be sending it to a display, etc.)
  Serial.print("Voltage: ");
  Serial.println(total_voltage, 2);  // Print with 2 decimal places

  ESPUI.updateLabel(ecoVoltLabel, String(total_voltage, 1));
}

void selectBattery(Control* sender, int value)
{
    switch (value)
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
    }

    String battEmpty = "<br><br>      Empty             0%   =   " + String((sender->value.toInt() * (float)3 ), 1);
    String battNom   = "    <br>      Nominal        50%   =   " + String((sender->value.toInt() * (float)3.7 ), 1);
    String battFull =  "    <br>      Full              100%   =  " +  String((sender->value.toInt() * (float)4.2 ), 1);      
    String allBatt = battEmpty + battNom + battFull;
    ESPUI.updateLabel(battInfo, allBatt);



    int battValue = sender->value.toInt();

    if(battValue > 6 && battValue < 22)
      {
        batteryInSeries = sender->value.toInt();
      }

     Serial.print("BatteryValue: ");
     Serial.print(battValue);
     Serial.print("  ");
     Serial.print(sender->value);
     Serial.print("  ");
     Serial.print(sender->value.toInt());
}

void ecomodeCallback(Control *sender, int type) {

    switch (type)
    {
    case S_ACTIVE:
        // batt.setEcoMode(true);
        Serial.print("Ecomode Active:");
        //Serial.println(batt.getEcoMode());
        break;

    case S_INACTIVE:
        // batt.setEcoMode(false);
        Serial.println("Ecomode Inactive");
        // Serial.println(batt.getEcoMode());
        break;
    }

    Serial.print(" ");
    Serial.println(sender->id);

}

void tempBoostCallback(Control *sender, int type) {

    switch (type)
    {
    case S_ACTIVE:
        // batt.setEcoMode(true);
        Serial.print("TempBoost Active:");
        // Serial.println(batt.getEcoMode());
        break;

    case S_INACTIVE:
        // batt.setEcoMode(false);
        Serial.println("TempBoost Inactive");
        // Serial.println(batt.getEcoMode());
        break;
    }

    Serial.print(" ");
    Serial.println(sender->id);

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

void setup()
{

  // ESPUI.prepareFileSystem();
  
  batt.begin();

 //  esp_task_wdt_init(WDT_TIMEOUT, true);  // enable panic so ESP32 restarts
  // esp_task_wdt_add(NULL);                           // add current thread to WDT watch


  randomSeed(0);
  Serial.begin(115200);
  while(!Serial);
  if(SLOW_BOOT) delay(1000); //Delay booting to give time to connect a serial monitor
  connectWifi();
  #if defined(ESP32)
    WiFi.setSleep(true); //For the ESP32: turn off sleeping to increase UI responsivness (at the cost of power use)
  #endif
  setUpUI();

// End of setup
}

void loop()
{

/*
  // Watchdog reset timer, so nothing will get stuck.
  if(millis() - reset_timer >= 1000) 
    {
      reset_timer = millis();
      esp_task_wdt_reset();
    }
*/

  if(millis() - mittausmillit >= 2000)
    {
      mittausmillit = millis();

      Serial.print("Temp: ");
      Serial.println(lampotila);
      batt.Temperature = lampotila;
      batt.readTemp();
    
      Input = batt.getTemp();
      Setpoint = batt.getHeatpoint();

      tempest = (int)batt.getTemp();  // tempest will now hold 23 (decimal part truncated)
      float gettemps = batt.getTemp();
      float getvolts = batt.getVoltage();

      String wlanIpAddress;
      String tempsValue;
      String voltsValue;
      String minutemen;
      float uptime;
      
      voltsValue = String(getvolts, 2) + " V ";
      tempsValue = String(gettemps, 1) + " ℃"; 



      Serial.println(WiFi.localIP());
      wlanIpAddress = WiFi.localIP().toString();

      ESPUI.updateLabel(ipText, wlanIpAddress);
      
      
           
      Serial.println(voltsValue);

      ESPUI.updateLabel(voltLabel, voltsValue);
      ESPUI.updateLabel(tempLabel, tempsValue);

      ESPUI.updateLabel(ipLabel, wlanIpAddress);

      uptime = millis() / 60000;
      minutemen = String(uptime, 1); 
      ESPUI.updateLabel(labelId, minutemen);

      
    }

    

   //  Quick heatprotection with some margin from isHeatable(), so the trend is downwards.
  if( batt.getTemp() < 0 || batt.getTemp() >  40 )
    { 
      digitalWrite(redpin, LOW);   // Lights up the red led. -> inverted. 
      batt.setCharger(false);
    }
  
  // Charging comparator -> future revision: make states! 
  if(batt.isChargeable())
      {
        if(batt.getVoltage() < batt.getEcoModeVoltage())  // Voltage is below ecomodeVoltage
          {
              digitalWrite(redpin, HIGH); // red led pin inverted.
              batt.setCharger(true);
              y_led.stop();
              g_led.setDelay(1000, 0);
          }

        if((batt.getVoltage() > (batt.getEcoModeVoltage() + 0.25)) && batt.getEcoMode() && (millis() - full_millis >= 60000))  // 1 min hysteresis
          {
            full_millis = millis(); 
            batt.setCharger(false);
            y_led.start();
            y_led.setDelay(1000, 0);
          }

        if(( batt.getVoltage() < (batt.getNominalS() * 4.25)) && !batt.getEcoMode())
          {
            batt.setCharger(true);
            g_led.setDelay(1000, 0);
            y_led.start();
            y_led.setDelay(1000, 0);
          }       
      }

// Battery drain protection, so heater wont kill the battery by draining it completely.
 if(batt.getVoltage() <= batt.getCutOffVoltage() || batt.getWarnings())
 {
    // ledcWrite(NFETCH, 0);  // lets write 0 to the heater. 

    g_led.setDelay(950, 50);
    y_led.setDelay(950, 50);
 }
 else if (batt.isHeatable() || batt.isHeatingMode())
 {
   // ledcWrite(NFETCH, Output);  // Channel 0 -> 4

   g_led.setDelay(1000, 0);
   y_led.setDelay(1000, 0);
 }

myPID.Compute();  // QuickPID looper   
batt.update();
dallas.update();
g_led.blink();
y_led.blink();

}
// End of Main Loop

//Utilities
//
//If you are here just to see examples of how to use ESPUI, you can ignore the following functions
//------------------------------------------------------------------------------------------------
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
  WiFi.setHostname(USERHOSTNAME);
#else
  WiFi.hostname(HOSTNAME);
#endif
  Serial.println("Begin wifi...");

  //Load credentials from EEPROM
  if(!(FORCE_USE_HOTSPOT)) {
    WiFi.setHostname(USERHOSTNAME);
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
    connect_timeout = 50; //7 seconds
    while (WiFi.status() != WL_CONNECTED && connect_timeout > 0) {
      delay(250);
      Serial.print(".");
      connect_timeout--;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(WiFi.localIP());
    Serial.println("Wifi started");

    if (!MDNS.begin(USERHOSTNAME)) {
      Serial.println("Error setting up MDNS responder!");
       while(1) {
      delay(100);
    }
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

/*
void ipCallback(Control *sender, int type) {
  ESPUI.updateText(mainText, String(rndString2));
}
*/

void randomString(char *buf, int len) {
  for(auto i = 0; i < len-1; i++)
    buf[i] = random(0, 26) + 'A';
  buf[len-1] = '\0';
}
