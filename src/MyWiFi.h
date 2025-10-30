#ifndef WIFI_h
#define WIFI_h

#include <Arduino.h>
#include "esp_wifi.h"
#include <esp_now.h>

extern MyDebug *myDebug;
#ifdef USE_MODULE_SWITCHES
extern MySwitches *mySwitches;
#endif

typedef struct s_espNow {
  uint8_t type;
  uint8_t id;
  int16_t value;
} s_espNow;

class MyWiFi {
private:
    // functions
    void notInitialized();    

    //Vars
    uint8_t WiFiChannel;
    bool initDone;

    esp_now_peer_info_t peerInfo;
    s_espNow espNowPacket;

    SemaphoreHandle_t semaphoreData;
    
    char macStr[18];
    char *tmp_buf;
    char *savedSSID;
    char *savedPassword;
public:
    MyWiFi();
    // functions
    void init(wifi_mode_t mode, char *ssid, char *password);
    void stop();
    bool initESPNow(int channel, bool encrypted);
    void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
    void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len);
    void addEspNowPeer(uint8_t address[6]);    
    esp_err_t sendEspNow(const uint8_t *peer_addr, uint8_t type, uint8_t id, uint16_t value);
    void connect();
    void disconnect();
    void ensureConnection();
    bool isConnected();
    wl_status_t getStatus();
    uint8_t getChannel();
};
  
#endif