#include "Battery.h"
#include <Arduino.h>
#include <ESPUI.h>
#include <EEPROM.h>
// #include <PubSubClient.h>
// #include <AsyncTelegram.h>


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

Battery battery;

//Settings
#define SLOW_BOOT 0
#define HOSTNAME "BattAssistant"
#define FORCE_USE_HOTSPOT 0


int tempest;
int batteryInSeries;
unsigned int mittausmillit = 0;  // battery voltage and heat reading millis()


uint16_t tab1, tab2, tab3;
String clearLabelStyle, switcherLabelStyle;
uint16_t battInfo, permInfo;
uint16_t labelId, APSwitcher, ipLabel, ipText;
uint16_t httpUserAccess, httpUsername, httpPassword;
uint16_t voltLabel, tempLabel;
uint16_t tempBoostSwitcher, voltBoostSwitcher; 
uint16_t wifitab;

//UI handles
uint16_t wifi_ssid_text, wifi_pass_text;
uint16_t graph;
volatile bool updates = false;

//ESPUI Configs page integers:
uint16_t mainSelector, permSwitcher;
uint16_t ecoTempNum, ecoVoltNum;
uint16_t boostTemp, boostVolts, nameLabel, hostnameLabel, ecoVoltLabel, ecoVoltUnitLabel, boostVoltUnitLabel, boostVoltLabel;

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
  ESPUI.setVertical(ESPUI.addControl(Switcher, "", "0", None, vertgroupswitcher, boostVoltageSwitcherCallback));
  ESPUI.setVertical(ESPUI.addControl(Switcher, "", "0", None, vertgroupswitcher, boostTempSwitcherCallback));
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
  auto nameLabel =  ESPUI.addControl(Text, "Hostname ", "myNameHere", Alizarin, tab2, generalCallback);
                    ESPUI.setElementStyle(ESPUI.addControl(Label, "veijo.local", "URL:  veijo.local ", Alizarin, nameLabel), "background-color: unset; width: 100%; text-align: left;");
                    ESPUI.setElementStyle(ESPUI.addControl(Label, "ip", "IP: ", Alizarin, nameLabel), "background-color: unset; width: 100%; text-align: left;");
                    ESPUI.setElementStyle( ipLabel = ESPUI.addControl(Label, "", "IP", None, nameLabel), "background-color: unset; width: 50%; text-align: left;");
                                  
                    ESPUI.setElementStyle(nameLabel, "font-size: x-large; font-family: serif; width: 100%; text-align: center;");
                
                    static String optionValues[] {"0", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21"};
                    mainSelector = ESPUI.addControl(Select, "Battery Series Configuration", "Selectorin", Wetasphalt, tab2, selectBattery);
                    for(auto const& v : optionValues) {
                    ESPUI.addControl(Option, v.c_str(), v, None, mainSelector); }

                    ESPUI.setElementStyle(battInfo = ESPUI.addControl(Label, "Is your battery within these sepcifications?", "", None, mainSelector), "background-color: unset; width: 100%; text-align: left;");
/*


    Permanen Heater disable switcher
    ################################

  permSwitcher = ESPUI.addControl(Switcher, "Permanent heating disable", "", None, tab2, generalCallback);
                 ESPUI.setElementStyle(permInfo = ESPUI.addControl(Label, "Battery has a built in safety protocol to cut off heating when the battery is below 30%.", "", None, permSwitcher), "background-color: unset; width: 100%; text-align: left;");
*/      

/*


  Tab2, Economy and Boost mode configurations
  ###########################################



*/
  ESPUI.addControl(Separator, "Economy -mode", "", None, tab2);

                  ecoTempNum = ESPUI.addControl(Number, "Economy Temperature", "20", Turquoise, tab2, ecoTempCallback);
                                    ESPUI.addControl(Min, "", "1", None, ecoTempNum);
                                    ESPUI.addControl(Max, "", "35", None, ecoTempNum);
                                    ESPUI.setElementStyle(ESPUI.addControl(Label, "C", "℃", Emerald, ecoTempNum), "font-family: serif; width: 60px; text-align: center; background-color: unset;");               

                  ecoVoltNum = ESPUI.addControl(Number, "Economy Charging", "70", Turquoise, tab2, ecoVoltCallback);
                                    ESPUI.addControl(Min, "", "50", None, ecoVoltNum);
                                    ESPUI.addControl(Max, "", "100", None, ecoVoltNum); 
                                    ESPUI.setElementStyle(ecoVoltUnitLabel = ESPUI.addControl(Label, "precent", "%   =", Emerald, ecoVoltNum), "font-family: serif; width: 70px; background-color: unset;");
                                    ESPUI.setElementStyle(ecoVoltLabel = ESPUI.addControl(Label, "Volts", "0", Emerald, ecoVoltNum), " text-align: right; font-size: large; font-family: serif; width: 70px; background-color: unset;"); 
                                    ESPUI.setElementStyle(ESPUI.addControl(Label, "", "V", None, ecoVoltNum), "font-family: serif; width: 20px; background-color: unset;");                                 
  ESPUI.addControl(Separator, "One-shot Economy override", "", None, tab2);

                  boostTemp = ESPUI.addControl(Number, "Boost Temperature", "20", Peterriver, tab2, boostTempCallback);
                                    ESPUI.addControl(Min, "", "1", None, boostTemp);
                                    ESPUI.addControl(Max, "", "35", None, boostTemp);
                                    ESPUI.setElementStyle(ESPUI.addControl(Label, "C", "℃", Emerald, boostTemp), "font-family: serif; width: 60px; text-align: center; background-color: unset;");                                       

                  boostVolts = ESPUI.addControl(Number, "Boost Charging", "90", Peterriver, tab2, boostVoltCallback);
                                    ESPUI.addControl(Min, "", "60", None, boostVolts);
                                    ESPUI.addControl(Max, "", "100", None, boostVolts);
                                    ESPUI.setElementStyle(boostVoltUnitLabel = ESPUI.addControl(Label, "precent", "%   =", Emerald, boostVolts), "font-family: serif; width: 50px; background-color: unset;");
                                    ESPUI.setElementStyle(boostVoltLabel = ESPUI.addControl(Label, "Volts", "0", Emerald, boostVolts), " text-align: right; font-size: large; font-family: serif; width: 70px; background-color: unset;");                                    
                                    ESPUI.setElementStyle(ESPUI.addControl(Label, "", "V", None, boostVolts), "font-family: serif; width: 20px; background-color: unset;");
  // Save to EEPROM
  ESPUI.addControl(Separator, "Mem", "Memory", None, tab2);
  ESPUI.addControl(Button, "Memory", "Save", None, tab2, 

   [](Control *sender, int type) {
      if(type == B_UP) {
        Serial.println("Saving battery structs...");
		batt.setBoostPrecentVoltage(90);
		batt.setEcoPrecentVoltage(70);
		batt.setBoostTemp(33);
		batt.setEcoTemp(22);
      }});


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
/*







    EcoVoltage Iumber Field - update to Voltage
*/
void ecoVoltCallback(Control* sender, int type) {
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
          ESPUI.updateLabel(boostVoltLabel, String(batt.btryToVoltage(sender->value.toInt()), 2));
			   	Serial.print("Boost VoltagePRecent set to: ");
          Serial.print(batt.getBoostPrecentVoltage());
          Serial.println(" V");
        }
      else 
      {
          ESPUI.updateLabel(boostVoltLabel, String(batt.getBoostPrecentVoltage(), 2));
         	Serial.print("Boost VoltagePRecent set to: ");
          Serial.print(batt.getBoostPrecentVoltage());
          Serial.println(" V");}
		}
		break;

	case S_INACTIVE:
		Serial.println("Ecomode Inactive");
		break;
    }

  // ESPUI.updateLabel(boostVoltLabel, String(batt.btryToVoltage(sender->value.toInt()), 2));

}

/*








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

    battery.setup();
    randomSeed(0);
	Serial.begin(115200);
	while(!Serial);
	if(SLOW_BOOT) delay(5000); //Delay booting to give time to connect a serial monitor
	connectWifi();
	#if defined(ESP32)
		WiFi.setSleep(true); //For the ESP32: turn off sleeping to increase UI responsivness (at the cost of power use)
	#endif
	setUpUI();

}   

void loop() {

    battery.loop(); // Add battery.loop() here

  if(millis() - mittausmillit >= 1000)
    {
    mittausmillit = millis();
    // tempest =   23; //    (int)batt.getTemp();  // tempest will now hold 23 (decimal part truncated)
    // float gettemps = 25;
    // float getvolts = 65;
    String wlanIpAddress;
    String tempsValue;
    String voltsValue;
    String minutemen;
    float uptime;


    //  Serial.println(WiFi.localIP());
      wlanIpAddress = WiFi.localIP().toString();

      ESPUI.updateLabel(ipText, wlanIpAddress);


    voltsValue = String(batt.getBatteryDODprecent()) + " %";
    ESPUI.updateLabel(voltLabel, voltsValue);
	  Serial.println(voltsValue);
	  tempsValue = String(batt.getTemperature(), 1) + " ℃"; 
    ESPUI.updateLabel(tempLabel, tempsValue);
      
	  
	  ESPUI.updateLabel(ipLabel, wlanIpAddress);

      uptime = millis() / 60000;
      minutemen = String(uptime, 1); 
      ESPUI.updateLabel(labelId, minutemen);

	  // ESPUI.updateNumber(boostVolts, batt.getBoostPrecentVoltage());
	  // ESPUI.updateNumber(ecoVoltNum, batt.getEcoPrecentVoltage());
	  // ESPUI.updateNumber(boostTemp, batt.getBoostTemp());
	  // ESPUI.updateNumber(ecoTempNum, batt.getEcoTemp());

      // ecoVoltNum, ecoTempNum, boostTemp, boostVolts;
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