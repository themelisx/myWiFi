#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"

#include "MyDebug.h"
#include "MyWiFi.h"

MyWiFi::MyWiFi() {
    myDebug = new MyDebug();
    myDebug->start(115200, DEBUG_LEVEL_DEBUG);
    myDebug->println(DEBUG_LEVEL_DEBUG, "[WiFi]");  
    
    initDone = false;
    tmp_buf = (char*)malloc(128);   

    semaphoreData = xSemaphoreCreateMutex();
    xSemaphoreGive(semaphoreData);
}

void MyWiFi::init(wifi_mode_t mode, char *ssid, char *password) {
    myDebug->println(DEBUG_LEVEL_DEBUG, "Initializing WiFi");  
    
    savedSSID = (char*)malloc(strlen(ssid) + 1);
    strcpy(savedSSID, ssid);
    
    savedPassword = (char*)malloc(strlen(password) + 1);
    strcpy(savedPassword, password);

    sprintf(tmp_buf, "SSID: '%s', password:'%s'", savedSSID, savedPassword);
    myDebug->println(DEBUG_LEVEL_DEBUG2, tmp_buf);

    WiFi.mode(mode);
    myDebug->print(DEBUG_LEVEL_INFO, "MAC Address: ");
    myDebug->println(DEBUG_LEVEL_INFO, WiFi.macAddress());
    initDone = true;
}

void MyWiFi::stop() {
    if (!initDone) {
        notInitialized();
        return;        
    }
    disconnect();
    initDone = false;
    free(savedSSID);
    free(savedPassword);
}

void MyWiFi::notInitialized() {
    myDebug->print(DEBUG_LEVEL_ERROR, "WiFi not initialized");
}

void MyWiFi::connect() {    
    myDebug->println(DEBUG_LEVEL_INFO, "WiFi disconnected, reconnecting...");

    if (!initDone) {
        notInitialized();
        return;        
    }

    xSemaphoreTake(semaphoreData, portMAX_DELAY);
    WiFi.disconnect();
    WiFi.begin(savedSSID, savedPassword);
    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
        myDebug->print(DEBUG_LEVEL_DEBUG, ".");
        delay(500);
    }

    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        myDebug->println(DEBUG_LEVEL_DEBUG, "WiFi connected");
        WiFiChannel = ap_info.primary;      
        myDebug->print(DEBUG_LEVEL_DEBUG, "WiFi channel: ");
        myDebug->println(DEBUG_LEVEL_DEBUG, WiFiChannel); 
    } else {
        WiFiChannel = 0;
        myDebug->println(DEBUG_LEVEL_DEBUG, "Not connected to any WiFi network.");    
    }
    xSemaphoreGive(semaphoreData);
}

void MyWiFi::ensureConnection() {
    if (!initDone) {
        notInitialized();
        return;        
    }
    if (!isConnected()) {
        connect();
    }
}

bool MyWiFi::isConnected() {
    if (!initDone) {
        notInitialized();
        return false;        
    }
    bool ret;
    xSemaphoreTake(semaphoreData, portMAX_DELAY);
    ret = WiFi.status() == WL_CONNECTED;
    xSemaphoreGive(semaphoreData);
    return ret;
}

void MyWiFi::disconnect() {
    myDebug->print(DEBUG_LEVEL_DEBUG, "WiFi disconnecting...");
    if (!initDone) {
        notInitialized();
        return;        
    }
    xSemaphoreTake(semaphoreData, portMAX_DELAY);
    WiFi.disconnect();
    xSemaphoreGive(semaphoreData);
}

wl_status_t MyWiFi::getStatus() {
    if (!initDone) {
        notInitialized();
        return WL_DISCONNECTED;        
    }
    wl_status_t ret;
    xSemaphoreTake(semaphoreData, portMAX_DELAY);
    ret = WiFi.status();
    xSemaphoreGive(semaphoreData);
    return ret;
}

uint8_t MyWiFi::getChannel() {
    if (!initDone) {
        notInitialized();
        return 0;        
    }
    uint8_t ret;
    xSemaphoreTake(semaphoreData, portMAX_DELAY);
    ret = WiFiChannel;
    xSemaphoreGive(semaphoreData);
    return ret;
}
