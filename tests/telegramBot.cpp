// src/TelegramBot.cpp
#include "TelegramBot.h"

// Constructor
TelegramBot::TelegramBot() {
    bot = new AsyncTelegram2(asiakas);
    configTzTime("EET-2EEST,M3.5.0/3,M10.5.0/4", "time.google.com", "time.windows.com", "pool.ntp.org");
    if(WiFi.status() == WL_CONNECTED) { 
        bot->setUpdateTime(1000);
        bot->setTelegramToken(token);
        Serial.print("\nTest Telegram connection... ");
        bot->begin() ? Serial.println("OK") : Serial.println("NOK");
        char welcome_msg[128];
        snprintf(welcome_msg, 128, "BOT @%s online\n/help all commands avalaible.", bot->getBotName());
        bot->sendTo(userid, welcome_msg);
    }
    else {
        Serial.print("not connected");
    }
}

// Method to set battery actions
void TelegramBot::setBatteryActions(std::function<float()> getTemperature,
                                     std::function<int()> getVoltage,
                                     std::function<void(bool)> activateVoltageBoost,
                                     std::function<void(bool)> activateTemperatureBoost) {
    this->getTemperature = getTemperature;
    this->getVoltage = getVoltage;
    this->activateVoltageBoost = activateVoltageBoost;
    this->activateTemperatureBoost = activateTemperatureBoost;
}

// Method to handle incoming commands
void TelegramBot::handleCommand(const String& command, int64_t chatId) {
    if (command == "get_temp") {
        float temp = getTemperature(); // Call the function to get temperature
        sendMessage(chatId, "Current temperature: " + String(temp));
    } else if (command == "get_voltage") {
        int voltage = getVoltage(); // Call the function to get voltage
        sendMessage(chatId, "Current voltage: " + String(voltage) + "%");
    } else if (command == "enable_voltage_boost") {
        activateVoltageBoost(true); // Enable voltage boost
        sendMessage(chatId, "Voltage boost enabled.");
    } else if (command == "disable_voltage_boost") {
        activateVoltageBoost(false); // Disable voltage boost
        sendMessage(chatId, "Voltage boost disabled.");
    } else if (command == "enable_temp_boost") {
        activateTemperatureBoost(true); // Enable temperature boost
        sendMessage(chatId, "Temperature boost enabled.");
    } else if (command == "disable_temp_boost") {
        activateTemperatureBoost(false); // Disable temperature boost
        sendMessage(chatId, "Temperature boost disabled.");
    } else {
        sendMessage(chatId, "Unknown command.");
    }
}

// Method to send a message
void TelegramBot::sendMessage(int64_t chatId, const String& message) {
    TBMessage msg; // Create a TBMessage object
    msg.chatId = chatId; // Set the chat ID
    msg.text = message.c_str(); // Convert String to const char*
    bot->sendMessage(msg, msg.text); 
}

// Method to handle incoming messages from the bot
void TelegramBot::onMessageReceived(const TBMessage& message) {
    handleCommand(message.text, message.chatId); // Pass chat ID to handleCommand
}

// Method to check for new messages
void TelegramBot::checkForMessages() {
    TBMessage msg;
    if (bot->getNewMessage(msg)) { // Check for new messages
        onMessageReceived(msg); // Process the received message
    }
}