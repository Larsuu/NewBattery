// src/TelegramBot.h
#ifndef TELEGRAMBOT_H
#define TELEGRAMBOT_H

#include <Arduino.h>
#include <functional> // For std::function
#include <WiFiClientSecure.h>
#include <AsyncTelegram2.h>
#include <WiFiClient.h>
#include <WiFi.h>
#include <time.h>
    // Global variables for telegram

#define USE_CLIENTSSL false 

class TelegramBot {
public:
    // Constructor
    TelegramBot();

    

    //void setCredentials(const String& token, const String& chatId);

    // Method to set battery actions
    void setBatteryActions(std::function<float()> getTemperature,
                           std::function<int()> getVoltage,
                           std::function<void(bool)> activateVoltageBoost,
                           std::function<void(bool)> activateTemperatureBoost);

    // Method to handle incoming commands
    void handleCommand(const String& command, int64_t chatId);

    // Method to send a message (to be implemented)
    void sendMessage(int64_t chatId, const String& message); // Accept chatId

    // Method to handle incoming messages from the bot
    void onMessageReceived(const TBMessage& message);

    // Method to check for new messages
    void checkForMessages();


    const char* ssid  =  "Olohuone";     // SSID WiFi network
    const char* pass  =  "10209997";     // Password  WiFi network
    const char* token =  "7875426228:AAE5HQJSmiphhDAD-CynCpamfHmk65hkF1A";  // Telegram token
    int64_t userid = 922951523; 

private:
    // Function pointers to interact with the Battery class
    std::function<float()> getTemperature; // Function to get temperature
    std::function<int()> getVoltage; // Function to get voltage
    std::function<void(bool)> activateVoltageBoost; // Function to activate voltage boost
    std::function<void(bool)> activateTemperatureBoost; // Function to activate temperature boost


    String botToken;
    String chatId;

    AsyncTelegram2* bot;
};

#endif // TELEGRAMBOT_H



















/*
class TelegramBot : public AsyncTelegram2 {
public:
    TelegramBot(WiFiClientSecure& client, const String& token);
    void begin();
    void handleBot();
    void processMessage(const TBMessage& msg);
    void displayMenu(const TBMessage& msg);
    void handleCallback(const TBMessage& msg);
    void sendMessage(const TBMessage& msg, const char* text, InlineKeyboard& keyboard);
    void setCallback(std::function<void(const TBMessage&)> callback);
    MenuNode* createMenuTree(); // Create the menu tree
    

    // MenuNode* createMenuTree();

private:
    String token;
    WiFiClientSecure client;
    bool initialized; // Flag to check if the bot is initialized
    MenuNode* rootMenu;
    MenuNode* currentNode;

    std::function<void(const TBMessage&)> buttonCallback;

    static const uint32_t bufferSize = BUFFER_BIG; // Define bufferSize

};

*/

// #endif // TELEGRAMBOT_H