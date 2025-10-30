#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"

#include "MyDebug.h"
#include "MyWiFi.h"

#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <esp_now.h>

MyWiFi* g_myWiFiInstance = nullptr;

MyWiFi::MyWiFi() {
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

////////////
// ESPNow //
////////////
void MyWiFi::addEspNowPeer(uint8_t address[6]) {
    memcpy(peerInfo.peer_addr, address, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        myDebug->println(DEBUG_LEVEL_ERROR, "Failed to add peer");
    } else {
        myDebug->println(DEBUG_LEVEL_INFO, "ESPNow peer added");
    }
}

void OnDataRecvWrapper(const uint8_t *mac_addr, const uint8_t *data, int len) {
    if (g_myWiFiInstance) {
        g_myWiFiInstance->OnDataRecv(mac_addr, data, len);
    }
}

#ifndef MODE_RELEASE
void OnDataSentWrapper(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (g_myWiFiInstance) {
        g_myWiFiInstance->OnDataSent(mac_addr, status);
    }
}

// Callback when data is sent
void MyWiFi::OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {  
    myDebug->print(DEBUG_LEVEL_DEBUG2, "Packet to: ");
    // Copies the sender mac address to a string
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    myDebug->print(DEBUG_LEVEL_DEBUG2, macStr);
    myDebug->print(DEBUG_LEVEL_DEBUG2, " send status:\t");
    myDebug->println(DEBUG_LEVEL_DEBUG2, status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
#endif

// Callback when data is received
void MyWiFi::OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {

  #ifndef MODE_RELEASE
    myDebug->print(DEBUG_LEVEL_DEBUG2, "Packet from: ");
    // Copies the sender mac address to a string
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    myDebug->println(DEBUG_LEVEL_DEBUG2, macStr);
    myDebug->print(DEBUG_LEVEL_DEBUG2, "Bytes received: ");
    myDebug->println(DEBUG_LEVEL_DEBUG2, len);
  #endif

  memcpy(&espNowPacket, incomingData, sizeof(espNowPacket));
  if (espNowPacket.type == 2) { // Buttons
    #ifdef USE_MODULE_SWITCHES
        mySwitches->espNow(espNowPacket.id, espNowPacket.value);
    #endif
  }
}

bool MyWiFi::initESPNow(int channel, bool encrypted) {

    g_myWiFiInstance = this;

    myDebug->println(DEBUG_LEVEL_INFO, "Initializing ESP-NOW...");
    
    WiFi.mode(WIFI_STA);
    
    myDebug->print(DEBUG_LEVEL_INFO, "MAC Address: ");
    myDebug->println(DEBUG_LEVEL_INFO, WiFi.macAddress());

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        myDebug->println(DEBUG_LEVEL_ERROR, "Error initializing ESP-NOW");
        return false;
    }

    // Once ESPNow is successfully Init, we will register for Send CB to
    // get the status of Trasnmitted packet
    #ifndef MODE_RELEASE
        esp_now_register_send_cb(OnDataSentWrapper);
    #endif

    // preper register peer
    peerInfo.channel = channel;  
    peerInfo.encrypt = encrypted;

    // Register for a callback function that will be called when data is received
    esp_now_register_recv_cb(OnDataRecvWrapper);
}

esp_err_t MyWiFi::sendEspNow(const uint8_t *peer_addr, uint8_t type, uint8_t id, uint16_t value) {
    espNowPacket.type = type;
    espNowPacket.id = id;
    espNowPacket.value = value;
    return esp_now_send(peer_addr, (uint8_t *) &espNowPacket, sizeof(s_espNow));
}

