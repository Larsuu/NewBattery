#ifndef TELEGRAMBOT_H
#define TELEGRAMBOT_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <AsyncTelegram2.h>
#include <functional>
#include <vector>
#include <Arduino.h>
#include "Battery.h"


// typedef void (*MenuAction)();

class Battery;

struct MenuNode {
    String name;
    std::vector<MenuNode*> children;
    MenuNode* parent;
    std::function<String()> header;
    std::function<void()> callback;

    MenuNode(const String& nodeName, MenuNode* parentNode = nullptr)
        : name(nodeName), parent(parentNode) {}
};



class TelegramBot : public AsyncTelegram2 {
public:
    TelegramBot(WiFiClientSecure& client, const String& token, Battery& battery);
    void begin();
    void handleBot();
    void processMessage(const TBMessage& msg);
    void displayMenu(const TBMessage& msg);
    void handleCallback(const TBMessage& msg);
    void sendMessage(const TBMessage& msg, const char* text, const InlineKeyboard& keyboard);
    void setCallback(std::function<void(const TBMessage&)> callback);
    MenuNode* createMenuTree(); // Create the menu tree
    
    // MenuNode* createMenuTree();

private:
    Battery& device;
    WiFiClientSecure client;
    bool initialized; // Flag to check if the bot is initialized
    MenuNode* rootMenu;
    MenuNode* currentNode;

    static const uint32_t bufferSize = BUFFER_BIG; // Define bufferSize

};

#endif // TELEGRAMBOT_H