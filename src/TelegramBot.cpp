#include <TelegramBot.h>

// Constructor implementation
TelegramBot::TelegramBot(WiFiClientSecure& client, const String& token, Battery& battery)
    : AsyncTelegram2(client, bufferSize), 
      device(battery), 
      client(), 
      rootMenu(nullptr), 
      currentNode(nullptr), 
      initialized(false) 
      
{
    client.setInsecure();
}

// Method to initialize the bot
void TelegramBot::begin() {
    if (!initialized) {
        setTelegramToken(device.battery.telegram.token.c_str());
        rootMenu = createMenuTree();
        currentNode = rootMenu;
        AsyncTelegram2::begin();
        initialized = true;
    }
}

// Method to handle incoming messages
void TelegramBot::handleBot() {
    if (device.battery.telegram.enable && WiFi.isConnected()) {
        TBMessage msg;
        if (getNewMessage(msg)) {
            processMessage(msg);
        }
    } else {
        Serial.println("WiFi not connected or Telegram disabled. Retrying...");
        WiFi.begin(device.battery.wlan.ssid, device.battery.wlan.pass);
    }
}


// Method to process incoming messages
void TelegramBot::processMessage(const TBMessage& msg) {
    if (msg.messageType == MessageText && msg.text == "/start") {
        currentNode = rootMenu;
        displayMenu(msg);
    } else if (msg.messageType == MessageQuery) {
        handleCallback(msg);
        Serial.print("Navigating by button: ");
        Serial.println(msg.text);
    }
}


// Method to display the menu
void TelegramBot::displayMenu(const TBMessage& msg) {
    InlineKeyboard keyboard;

    // Set the header with battery data
    String headerText = String("<b>Voltage: ") + String(device.battery.milliVoltage) + " mV</b>\n" +
                        String("<b>Temperature: ") + String(device.battery.temperature) + " °C</b>\n";

    if (currentNode->name == "EcoV" || currentNode->name == "BoostV") {
        int buttonCount = 0;
        for (int i = 50; i <= 100; i += 5) {
            if (buttonCount > 0 && buttonCount % 6 == 0) {
                keyboard.addRow();
            }
            keyboard.addButton(String(i).c_str(), (String(currentNode->name) + "_" + String(i)).c_str(), KeyboardButtonQuery);
            buttonCount++;
        }
        keyboard.addRow();
        keyboard.addButton("Save", "save", KeyboardButtonQuery);
        keyboard.addRow();
        keyboard.addButton("Back", "back", KeyboardButtonQuery);
    } else {
        for (auto& child : currentNode->children) {
            keyboard.addButton(child->name.c_str(), child->name.c_str(), KeyboardButtonQuery);
        }
        if (currentNode != rootMenu) {
            keyboard.addRow();
            keyboard.addButton("Back", "back", KeyboardButtonQuery);
        }
    }

    String menuText = headerText + "\n" + currentNode->name + ": ";
    sendMessage(msg, menuText.c_str(), keyboard);
}


// Method to handle callbacks from buttons
void TelegramBot::handleCallback(const TBMessage& msg) {
    String callbackData = msg.callbackQueryData;

    if (callbackData.equals("back")) {
        if (currentNode != rootMenu && currentNode->parent != nullptr) {
            currentNode = currentNode->parent;
        }
    } else if (callbackData.equals("save")) {
        // Save the current settings to NVS or battery
        Serial.println("Saving selected value...");
        // Access battery data directly to save settings
        // device.battery.milliVoltage = newValue; // Example of setting a new value
    } else if (callbackData.startsWith("EcoV_") || callbackData.startsWith("BoostV_")) {
        int underscorePos = callbackData.indexOf("_");
        String menuName = callbackData.substring(0, underscorePos);
        int selectedValue = callbackData.substring(underscorePos + 1).toInt();
        Serial.println(menuName + " Selected Value: " + String(selectedValue));
        // Update the battery settings based on the selected value
    } else {
        for (auto& child : currentNode->children) {
            if (callbackData.equals(child->name.c_str())) {
                currentNode = child;
                break;
            }
        }
    }
    displayMenu(msg);
}


// Method to create the menu tree
MenuNode* TelegramBot::createMenuTree() {
    MenuNode* root = new MenuNode("Main Menu");
    root->header = [this]() -> String { return String("<b>") + String(device.battery.milliVoltage) + " mV</b>\n" +
                                          String("<b>") + String(device.battery.temperature) + " °C</b>\n"; };

    MenuNode* settings = new MenuNode("Settings", root);
    settings->header = [this]() -> String { return String("<b>") + String(device.battery.milliVoltage) + " mV</b>\n" +
                                              String("<b>") + String(device.battery.temperature) + " °C</b>\n"; };
    root->children.push_back(settings);

    // Add more menu nodes as needed
    // ...

    return root;
}









// Method to send a message
void TelegramBot::sendMessage(const TBMessage& msg, const char* text, const InlineKeyboard& keyboard) {
    if (initialized) {
        AsyncTelegram2::sendMessage(msg, text, keyboard); // Send the message using the bot
    }
}


// Method to set a callback for handling button presses
void TelegramBot::setCallback(std::function<void(const TBMessage&)> callback) {
    int buttonCallback = callback; // Set the callback function
}








/*
typedef void (*MenuAction)();
struct MenuNode {
    String name;
    MenuAction action;
    std::vector<MenuNode*> children;
    MenuNode* parent;
    std::function<String()> header;
    std::function<void()> callback;

    MenuNode(const String& nodeName, MenuNode* parentNode = nullptr, MenuAction nodeAction = nullptr)
        : name(nodeName), action(nodeAction), parent(parentNode) {}
};

class BotManager {
private:
    AsyncTelegram2 bot;
    MenuNode* rootMenu;
    MenuNode* currentNode;
    Battery& device;

public:
    BotManager(Battery& battery, WiFiClientSecure& client) 
        : bot(client), rootMenu(nullptr), currentNode(nullptr), device(battery) {
        rootMenu = createMenuTree();
        currentNode = rootMenu;
        client.setInsecure();
        bot.setTelegramToken(BOT_TOKEN);
        bot.begin();
    }

    void handleBot() {
        if (WiFi.isConnected()) {
            TBMessage msg;
            if (bot.getNewMessage(msg)) processMessage(msg);
        }   
        else {
        Serial.println("WiFi not connected. Retrying...");
        WiFi.begin(ssid, password);
        }
    }

    void processMessage(const TBMessage& msg) {
        if (msg.messageType == MessageText && msg.text == "/start") {
            currentNode = rootMenu;
            displayMenu(msg);

        } else if (msg.messageType == MessageQuery) {
            handleCallback(msg);
            Serial.print("Navigating by button: ");
            Serial.println(msg.text);            
        }
    }

    void displayMenu(const TBMessage& msg) {
        InlineKeyboard keyboard;

      if (currentNode->name == "EcoV" || currentNode->name == "BoostV") {
        int buttonCount = 0;
        for (int i = 50; i <= 100; i += 5) {
            if (buttonCount > 0 && buttonCount % 6 == 0) {
                keyboard.addRow();
            }
            keyboard.addButton(String(i).c_str(), (String(currentNode->name) + "_" + String(i)).c_str(), KeyboardButtonQuery);
            buttonCount++;
        }
        
        // Add Save button in a new row
        keyboard.addRow();
        keyboard.addButton("Save", "save", KeyboardButtonQuery);
        
        // Add Back button in a new row
        keyboard.addRow();
        keyboard.addButton("Back", "back", KeyboardButtonQuery);
      }

      else {
        for (auto& child : currentNode->children) {
            keyboard.addButton(child->name.c_str(), child->name.c_str(), KeyboardButtonQuery);
        }

        if (currentNode != rootMenu) {
            keyboard.addRow();
            keyboard.addButton("Back", "back", KeyboardButtonQuery);
        }
      }

        String headerText = currentNode->header ? currentNode->header() : "";
        String menuText = headerText + "\n" + currentNode->name + ": ";

        bot.sendMessage(msg, menuText.c_str(), keyboard);
  }




    void refreshMenu(const TBMessage& msg) {
    // Clear the previous inline keyboard
    InlineKeyboard clearKeyboard;
    // bot.editMessage(msg.chatId, msg.messageID.c_str(), clearKeyboard);

    // Display the updated menu
    displayMenu(msg);
}




void handleCallback(const TBMessage& msg) {
    String callbackData = msg.callbackQueryData;

    if (callbackData.equals("back")) {
        if (currentNode != rootMenu && currentNode->parent != nullptr) {
            currentNode = currentNode->parent;
        }
    } else if (callbackData.equals("save")) {
        // Add logic to save the selected value

        // save the latest data. from the class to the eeprom. 


        Serial.println("Saving selected value...");
    } else if (callbackData.startsWith("EcoV_") || callbackData.startsWith("BoostV_")) {
        int underscorePos = callbackData.indexOf("_");
        String menuName = callbackData.substring(0, underscorePos);
        int selectedValue = callbackData.substring(underscorePos + 1).toInt();
        Serial.println(menuName + " Selected Value: " + String(selectedValue));

        // push the value to the class. 
        // Add logic to set the actual value

    } else {
        for (auto& child : currentNode->children) {
            if (callbackData.equals(child->name.c_str())) {
                currentNode = child;
                break;
            }
        }
    }
    displayMenu(msg);
}


    MenuNode* createMenuTree() {

        MenuNode* root = new MenuNode("Main Menu");
                  root->header = [this]() -> String { return String("<b>") + String(device.getVoltage()) + " V   " + String(device.getTemperature()) + " °C" + "</b> \n"; };

        MenuNode* settings = new MenuNode("Settings", root);
                  settings->header = [this]() -> String { return String("<b>") + String(device.getVoltage()) + "V  " + String(device.getTemperature()) + " °C" + "</b> \n"; };
                  root->children.push_back(settings);

        MenuNode* ecotemp = new MenuNode("EcoT", settings);
                  ecotemp->header = [this]() -> String { return String("<b>") + String(device.getVoltage()) + "V  " + String(device.getTemperature()) + " °C" + "</b> \n"; };
                  settings->children.push_back(ecotemp);

        MenuNode* boosttemp = new MenuNode("BoostT", settings);
                  boosttemp->header = [this]() -> String { return String("<b>") + String(device.getVoltage()) + "V  " + String(device.getTemperature()) + " °C" + "</b> \n"; };
                  settings->children.push_back(boosttemp);       

        MenuNode* ecovolt = new MenuNode("EcoV", settings);
                  ecovolt->header = [this]() -> String { return String("<b>") + String(device.getVoltage()) + "V  " + String(device.getTemperature()) + " °C" + "</b> \n"; };
                  settings->children.push_back(ecovolt);

        MenuNode* boostvolt = new MenuNode("BoostV", settings);
                  boostvolt->header = [this]() -> String { return String("<b>") + String(device.getVoltage()) + "V  " + String(device.getTemperature()) + " °C" + "</b> \n"; };
                  settings->children.push_back(boostvolt);
        return root;
    }
};

Battery battery;
BotManager botManager(battery, client);

void setup() {
    Serial.begin(115200);
    Serial.println("Starting program...");

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");

    // client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
}

void loop() {
    botManager.handleBot();
    delay(100);
}

*/

