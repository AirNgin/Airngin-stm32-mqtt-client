#pragma once
/*
 airngin.h - A simple client for Connect To AirNgin Broker.
  AirNgin VERSION : 0.1.11 Beta
  https://airngin.com
*/
#ifndef airngin_h
#define airngin_h

#define SOFTWARE_VERSION "1.0.0"

#define DVC_DEFAULTPASS "00000000"  // DONT CHANGE THIS
#define SOFTWARE_DATE "1404.04.31"



#include <Arduino.h>
#include <Ethernet.h>
#include <EEPROM.h>
// #include <WString.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Dns.h>



#define TASK_PRIORITY_HIGH 1       // 5
#define TASK_PRIORITY_MID 2        // 3
#define TASK_PRIORITY_LOW 3        // 2
#define TASK_STACKSIZE_SMALL 2048  // 1024
#define TASK_STACKSIZE_MEDIUM 4096 // 4096
#define TASK_STACKSIZE_LARGE 8192  // 8192
#define TASK_CORE_0 0
#define TASK_CORE_1 1
#define TASK_LOOP_DELAY 20


#define BYTE_1 1
#define BYTE_2 2
#define BYTE_3 3
#define BYTE_10 10
#define BYTE_11 11
#define BYTE_12 12
#define BYTE_14 14
#define BYTE_15 15
#define BYTE_16 16
#define BYTE_17 17
#define BYTE_20 20
#define BYTE_32 32
#define BYTE_37 37
#define BYTE_38 38
#define BYTE_45 45
#define BYTE_100 100
#define BYTE_400 400

#define MIN_20 1200
#define MIN_15 900
#define MIN_10 600
#define MIN_5 300
#define MIN_4 240
#define MIN_3 180
#define MIN_2 120
#define MIN_1 60
#define SEC_90 90
#define SEC_30 30
#define SEC_20 20
#define SEC_10 10
#define SEC_5 5
#define SEC_4 4
#define SEC_3 3
#define SEC_2 2
#define SEC_1 1
#define TIMER_NEED_RUN 0
#define TIMER_JOB_DONE -1
#define TIMER_EXPIRED_FOREVER -2


//===========================================================================================
//                                           EPPROM
//===========================================================================================
#define EEPROM_SIZE 4000

#define EP_MEMORY_NOT_CLEANABLE 128

#define EP_TESTMODE (EP_MEMORY_NOT_CLEANABLE + 1)// (129)
#define EP_CONFIGURED (EP_TESTMODE + 1)            // (130)
#define EP_STARTMODE (EP_CONFIGURED + 1)         // (131)
#define EP_SERIAL_S (EP_STARTMODE + 1)           // (132-141)
#define EP_SERIAL_E (EP_SERIAL_S + BYTE_10 - 1)  // (132-141)
#define EP_MEMORYSTART1 142                      // MEMORY POINT 1 BEGIN :  Cleanable Settings, Clear Begin From Here To End Of EPROM

#define EP_PROJECTCODE_S (EP_MEMORYSTART1 + 1)   //........................................................... NETWORK
#define EP_PROJECTCODE_E (EP_PROJECTCODE_S + BYTE_20 - 1)
#define EP_CLIENTID_S (EP_PROJECTCODE_E + 1)
#define EP_CLIENTID_E (EP_CLIENTID_S + BYTE_38 - 1)
#define EP_ENCRYPTIONKEY_S (EP_CLIENTID_E + 1)   // For Own Encryption Key
#define EP_ENCRYPTIONKEY_E (EP_ENCRYPTIONKEY_S + BYTE_45)
#define EP_ENCRYPTIONKEY_PROJECT_S (EP_ENCRYPTIONKEY_E + 1) //........................................................ NETWORK MESH
#define EP_ENCRYPTIONKEY_PROJECT_E (EP_ENCRYPTIONKEY_PROJECT_S + BYTE_45)
#define EP_MQTTUSER_S (EP_ENCRYPTIONKEY_PROJECT_E + 1) //....................................................... NETWORK MQTT
#define EP_MQTTUSER_E (EP_MQTTUSER_S + BYTE_16 - 1)
#define EP_MQTTPASS_S (EP_MQTTUSER_E + 1)
#define EP_MQTTPASS_E (EP_MQTTPASS_S + BYTE_16 - 1)
#define EP_MQTTBROKER_S (EP_MQTTPASS_E + 1)
#define EP_MQTTBROKER_E (EP_MQTTBROKER_S + BYTE_100 - 1)
#define EP_MODEMSSID_S (EP_MQTTBROKER_E + 1)
#define EP_MODEMSSID_E (EP_MODEMSSID_S + BYTE_32 - 1)
#define EP_MODEMPASS_S (EP_MODEMSSID_E + 1)
#define EP_MODEMPASS_E (EP_MODEMPASS_S + BYTE_20 - 1)

#define EP_RESERVE1_S (EP_MODEMPASS_E + 1)           // RESERVE S 100
#define EP_RESERVE1_E (EP_RESERVE1_S + BYTE_1 - 1) // RESERVE E 100
#define EP_MEMORYEND1 (EP_RESERVE1_E)              // MEMORY POINT 1 END
#define EP_MEMORYPOINT1H (EP_MEMORYEND1 + 1)       // MEMORY POINT 1 SAVE : to save high-byte of current EProm-Address (to detect if upper EProm-Addresses changed)
#define EP_MEMORYPOINT1L (EP_MEMORYPOINT1H + 1)    // MEMORY POINT 1 SAVE : to save low-byte of current EProm-Address (to detect if upper EProm-Addresses changed)
#define EP_MEMORYSTART2 (EP_MEMORYPOINT1L + 1)     // MEMORY POINT 2 BEGIN : Cleanable Settings, Clear Begin From Here To End Of EPROM


//................................................................................................. END
#define EP_MEMORYEND2 (EP_MEMORYSTART2)  // MEMORY POINT 2 END : Cleanable Settings, ClearFrom Start To Here (End Of EPROM)
#define EP_MEMORYPOINT2H (EP_MEMORYEND2 + 1)    // MEMORY POINT 2 SAVE : To save high-byte of current EProm-Address (to detect if upper EProm-Addresses changed)
#define EP_MEMORYPOINT2L (EP_MEMORYPOINT2H + 1) // MEMORY POINT 2 SAVE : To save low-byte of current EProm-Address (to detect if upper EProm-Addresses changed)


//.......................................... Scenario
#define MAX_SCENARIO 20         // تعداد سناریوها

//===========================================================================================
//                                      PREFERENCES MEMORY
//===========================================================================================
    struct AirNginConfig {
      char ProjectCode[20];
      char ClientId[44];
      char MqttUser[32];
      char MqttPass[32];
      char MqttBroker[32];
      char EncryptionKey[32];
      char EncryptionKeyProject[32];
      char SerialNo[32];
      bool isUseEncrypted; // 0 = not encrypted, 1 = encrypted
    };


class AirNginClient{

private:

    int UDP_PORT = 45454;
    EthernetUDP Udp;
    byte _MqttCon_Steps;
    int _IOT_ModemTimeout = MIN_3;
    int _IOT_MqttTimeout = TIMER_NEED_RUN;
    int _IOT_PingTimeout = MIN_15;
    String _EncryptionKey;
    String _EncryptionKeyProject;
    String _MqttBroker;
    String _MqttUser;
    String _MqttPass;    
    String _CloudClientId;
    PubSubClient _MqttObj;
    unsigned long _TimerMqttLoopPrev;
    const unsigned long _MttLoopInterval = 5000;
  
    bool _IsSerialStarted = false;
    bool _Reboot;

    static AirNginClient *instance;

    unsigned long _TimerSecCurrent, _TimerSecOld = 0;
    int _TimerSecExtra = 0;
    byte _TimerSecDef = 0;
    int _TimerForActions = 0;

    void TimerSec_Refresh(); // timer 
    void Tools__Memory_ClearAll();
    void Debug_ConsolePrint(String p1);
    void Debug_ConsolePrint(int p1);   // اضافه کردن نسخه int
    void Debug_ConsolePrint(long p1);  // اضافه کردن نسخه long
    void Debug_ConsolePrint(float p1); // اضافه کردن نسخه float
    void Debug_ConsolePrintln(String p1);
    void Debug_ConsolePrintln(int p1);   // اضافه کردن نسخه int
    void Debug_ConsolePrintln(long p1);  // اضافه کردن نسخه long
    void Debug_ConsolePrintln(float p1); // اضافه کردن نسخه float
    void Send_RebootAndStatus();
    void Network_Reset();
    bool Network_HelthCheck();
    bool Network_Ping();
    bool Network_Ping(String remote_host);
    void handleUdp();
    // static void IOT__WiFiEvent(WiFiEvent_t event);
    void Tools__Reboot(String Reason);
    void MemoClean(int p_start = EP_MEMORYSTART2, int p_end = EP_MEMORYEND2, bool cleanFiles = true);
    static void Mqtt__OnRecieve(char *topic, uint8_t *payload, unsigned int length);
    void MemoWriteByte(int p_start, byte val);
    byte MemoReadByte(int p_start);
    String Tools__Memory_StrGet(String key);
    bool Tools__Memory_StrSet(String key, String val);
    void MemoWriteString(int p_start, int p_end, String val);
    String MemoReadString(int p_start, int p_end);
    int Tools__StringToInt(String inp);
    byte Tools__StringToByte(String inp);
    void Tools__SettingShowInfo();
    void (*onMessageCallback)(char *topic, uint8_t *payload, unsigned int length);  // اشاره‌گر به تابع callback

    // Preferences _MemoFile;
    void Tools__SettingRead();

    //===========================================================================================
    //                                       Config / Update
    //===========================================================================================
    String HttpRequest(const char* method, const char* path, const char* body);
    void Tools__SettingSave();
    void Tools__SettingDefault();
    bool mqttConnected = false;

    
    bool _Update_Trying = false;
    static const String rootCACertificate;
    bool _MqttRcv_IsWorking = false;
    Client* _net;
    EthernetServer _Server;
    bool _ConfigPanelIsOpen = false;  
    bool _NeedReboot = false;  
    // void SendHttpResponse(int code, String contentType, String body);
    void attemptMQTTConnection();
    bool EthernetConnected();
    void updateMQTT();
    bool checkInternet();
    void updateEthernet();
    void DebugPrint(const char* fmt, ...);
    void MemoCheck();
    void broadcastInfo();


public:



    bool _IsUsedEncryptionKey = false;
    bool _Mqtt_TryConnecting  = false;
    String _SerialCloud;
    String _ProjectCode;
    void loop();
    AirNginConfig config;   // کل تنظیمات
    void saveConfig();
    void loadConfig();

    AirNginClient(Client& netClient);
    // bool addDataToCloud(const char* key, const char* value);
    // String getDataFromCloud(const char* key);
    // bool deleteDataFromCloud(const char* key);
    void begin(String serial_of_device="",bool isDebugEnabled=false);
    void Mqtt_Send(String topic, String data, bool offlineSupport=false);
    void MessageCloud__ViaMqtt_NotifyTo_User(int messageCode);
    void MessageCloud__ViaMqtt_NotifyTo_User(String &messageCode);
    void MessageCloud__ViaMqtt_SMSTo_User(int messageCode);
    void MessageCloud__ViaMqtt_SMSTo_User(String &messageCode);
    void MessageCloud__ViaMqtt_SMSTo_Center(int messageCode);
    void MessageCloud__ViaMqtt_SMSTo_Center(String &messageCode);            
    uint32_t Tools__GetChipID();
    long Tools__Random(long a, long b);
    // void IOT__FirmwareUpdate(String DownloadLink, String Certificate = "", String UTC="");
    
    void SetClock(String UTC="");
    ~AirNginClient();
    
    void setOnMessageCallback(void (*callback)(char *topic, uint8_t *payload, unsigned int length)) {
        onMessageCallback = callback;
    }

    bool isConfigMode = false;
    bool stopBroadcast = false; // برای جلوگیری از ارسال اطلاعات در حالت پیکربندی
    bool _Ethernet_IsConnected = false;
    bool _WiFi_ConnectWorking;
    bool _MqttCon_IsConnected;
    bool isDebugEnabled = false;
    byte mac[6] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
    static void GenerateMAC(byte mac[6]);       
    void PrintMAC(byte mac[6]);
    void printNetInfo();     

};




#endif //airngin_h