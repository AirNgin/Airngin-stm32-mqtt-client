#include "AirNgin.h"
#include "Arduino.h"


bool _MqttCon_IsConnected = false;
String _MqttBroker = "";
bool debugOn=false;

AirNginClient* AirNginClient::instance = nullptr;


AirNginClient::AirNginClient(Client& netClient)
    : _MqttObj(netClient), _net(&netClient), _Server(80)
{
    instance = this;   // اگر قبلاً برای singleton استفاده کردی
}


void AirNginClient::begin(String _serial_of_device , bool is_debug_enabled) {

  instance = this;
  instance->isDebugEnabled = is_debug_enabled;
 
  //-------------------------------------------------------- Preparing EEPROM & File
  Tools__SettingRead();
  Tools__SettingShowInfo();
  _MqttObj.setBufferSize(4096);

  if(_serial_of_device != ""){
    _SerialCloud = _serial_of_device;
  }else{
    instance->Debug_ConsolePrintln("Serial Can't Empty");
    return;
  }

  instance->Debug_ConsolePrintln("_SerialCloud :: >> " + _SerialCloud);
  instance->Debug_ConsolePrintln(".... IOT Setup START ...");
  //............................................ Mqtt Settings
  _MqttCon_IsConnected = false;
  instance->_MqttCon_Steps = 0;
}


void AirNginClient::PrintMAC(byte mac[6]) {
    char macStr[18]; // "AA:BB:CC:DD:EE:FF" + null terminator
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2],
            mac[3], mac[4], mac[5]);

    instance->Debug_ConsolePrint("MAc : : >> ");
    instance->Debug_ConsolePrintln(macStr);
}


void AirNginClient::GenerateMAC(byte mac[6]) {
    instance->Debug_ConsolePrintln("GenerateMAC ::");
    uint32_t id1 = HAL_GetUIDw0();
    uint32_t id2 = HAL_GetUIDw1();
    uint32_t id3 = HAL_GetUIDw2();
    mac[0] = (byte)(id1 & 0xFF);
    mac[1] = (byte)((id2 >> 8) & 0xFF);
    mac[2] = (byte)(id2 & 0xFF);
    mac[3] = (byte)((id3 >> 16) & 0xFF);
    mac[4] = (byte)((id3 >> 8) & 0xFF);
    mac[5] = (byte)(id3 & 0xFF);
}


void AirNginClient::loop() {
    updateMQTT();
    handleUdp();
}

void AirNginClient::handleUdp() {
  if (!instance->isConfigMode) return; // در حالت پیکربندی، فقط MQTT را به‌روزرسانی می‌کنیم
  
  if (!instance->stopBroadcast && instance->isConfigMode) {
      instance->Debug_ConsolePrintln("Config Mode Active");
      broadcastInfo();
  }

  int packetSize = Udp.parsePacket();

  if (packetSize) {
    char buf[1024];
    int len = Udp.read(buf, sizeof(buf) - 1);
    if (len > 0) buf[len] = 0;

    Debug_ConsolePrint("UDP Received 1 : ");
    Debug_ConsolePrintln(buf);

    
     if (strcmp(buf, "STOP") == 0) {
     stopBroadcast = true;
     return;
     } 
     else if (strcmp(buf, "START") == 0) {
       stopBroadcast = false;
     }else if(strcmp(buf, "FINISH") == 0) {
       isConfigMode = false;
       stopBroadcast = false;
     }
       Debug_ConsolePrint("UDP Received 3 : ");
    

    // ---- پارس JSON ----
    StaticJsonDocument<4096> doc;
    DeserializationError err = deserializeJson(doc, buf);
    if (err) {
      Serial.print(F("JSON parse failed: "));
      Serial.println(err.f_str());
      return;
    }


    // مقادیر
    String projectCode   = doc["projectCode"]   | "";
    String clientId      = doc["clientId"]      | "";
    String mqttUser      = doc["mqttUser"]      | "";
    String mqttPass      = doc["mqttPass"]      | "";
    String mqttBroker    = doc["mqttBroker"]    | "";
    String encryptionKey    = doc["encryptionKey"]    | "";
    String encryptionKeyProject    = doc["encryptionKeyProject"]    | "";
    bool isUseEncrypted    = doc["isUseEncrypted"]  | false;
    bool closeConfig     = doc["closeConfig"]   | false;

    Tools__Memory_StrSet("_ProjectCode", projectCode);
    Tools__Memory_StrSet("_CloudClientId", clientId);
    Tools__Memory_StrSet("_MqttUser", mqttUser);
    Tools__Memory_StrSet("_MqttPass", mqttPass);
    Tools__Memory_StrSet("_MqttBroker", mqttBroker);
    Tools__Memory_StrSet("_EncryptionKey", encryptionKey);
    Tools__Memory_StrSet("_EncryptionKeyProject", encryptionKeyProject);
    Tools__Memory_StrSet("_SerialNo", _SerialCloud);
    instance->Debug_ConsolePrintln("After Save Serial");
    instance->config.isUseEncrypted = isUseEncrypted;
    instance->Debug_ConsolePrintln("After Save isUseEncrypted");
    saveConfig();
    Tools__SettingRead();
    isConfigMode = false;
    stopBroadcast = false;
    // اگر دستور خاصی دریافت شد
  
  }
}


void AirNginClient::broadcastInfo() {
  instance->Debug_ConsolePrintln("Config Mode Active 1");
  // ساخت JSON ساده
  String json = "{";
  json += "\"name\":\"DVC-" + instance->_SerialCloud + "-key\",";
  json += "\"ip\":\"" + Ethernet.localIP().toString() + "\",";
  json += "\"gateway\":\"" + Ethernet.gatewayIP().toString() + "\",";
  json += "\"subnet\":\"" + Ethernet.subnetMask().toString() + "\",";
  json += "\"dns\":\"" + Ethernet.dnsServerIP().toString() + "\",";
  json += "\"deviceSerial\":\"" + instance->_SerialCloud + "\"";
  json += "}";
  instance->Debug_ConsolePrintln("Config Mode Active 2");

  instance->Udp.begin(instance->UDP_PORT);

  instance->Debug_ConsolePrintln("Config Mode Active 4");

  IPAddress broadcast(255, 255, 255, 255);
  instance->Debug_ConsolePrintln("Config Mode Active 5");

  // ارسال Broadcast
  instance->Udp.beginPacket(broadcast, instance->UDP_PORT);
  instance->Udp.write((const uint8_t*)json.c_str(), json.length());
  instance->Udp.endPacket();
  instance->Debug_ConsolePrintln("Config Mode Active 6");


  instance->Debug_ConsolePrint("Broadcast: ");
  instance->Debug_ConsolePrintln(json);
}


void AirNginClient::printNetInfo() {
  instance->Debug_ConsolePrint(F("---- Ethernet Info ----"));
  IPAddress ip = Ethernet.localIP();

  instance->Debug_ConsolePrint(F("Local IP   : ")); instance->Debug_ConsolePrintln(ip.toString());
  IPAddress gatewayIP = Ethernet.gatewayIP();

  instance->Debug_ConsolePrint(F("Gateway    : ")); instance->Debug_ConsolePrintln(gatewayIP.toString());
  IPAddress subnetMask = Ethernet.gatewayIP();

  instance->Debug_ConsolePrint(F("Subnet     : ")); instance->Debug_ConsolePrintln(subnetMask.toString());
  IPAddress dnsServerIP = Ethernet.dnsServerIP();

  instance->Debug_ConsolePrint(F("DNS        : ")); instance->Debug_ConsolePrintln(dnsServerIP.toString());
  instance->Debug_ConsolePrint(F("-----------------------"));
}


// Internet Ping Check
bool AirNginClient::checkInternet() {

    if (_net->connect(IPAddress(8,8,8,8), 53)) { // تست اتصال به DNS گوگل
        _net->stop();
        return true;
    }
    return false;
}

// MQTT Handler
void AirNginClient::updateMQTT() {
    if (instance->isConfigMode) return; // در حالت پیکربندی، فقط MQTT را به‌روزرسانی می‌کنیم
    if (!EthernetConnected() || !_Mqtt_TryConnecting) return;

    switch (_MqttCon_Steps) {
        case 0:
            instance->Debug_ConsolePrintln("Step 0: Preparing MQTT connection...");
            _MqttCon_Steps = 2;
            break;

        case 2:
            instance->Debug_ConsolePrintln("Step 2: Configuring MQTT client...");
            _MqttObj.setCallback(Mqtt__OnRecieve);
            _MqttObj.setServer(_MqttBroker.c_str(), 1883);
            _MqttObj.setBufferSize(2048);
            _MqttObj.setSocketTimeout(15);
            _MqttObj.setKeepAlive(15);
            _MqttCon_Steps = 3;
            break;

        case 3:
            if (!_MqttObj.connected()) {
                attemptMQTTConnection();
            } else {
                _MqttObj.loop();
            }
            break;

        default:
            instance->Debug_ConsolePrintln("Step " + String(_MqttCon_Steps) + ": No action");
            break;
    }
}

bool AirNginClient::EthernetConnected() {
    return (Ethernet.linkStatus() != LinkOFF);
}

void AirNginClient::attemptMQTTConnection() {
    static unsigned long lastAttempt = 0;
    if (millis() - lastAttempt < 5000) return;
    lastAttempt = millis();

    instance->Debug_ConsolePrintln("Attempting MQTT Connection...");
    Tools__SettingShowInfo();
    if (_MqttObj.connect(_CloudClientId.c_str(), _MqttUser.c_str(), _MqttPass.c_str())) {
        instance->Debug_ConsolePrintln("MQTT Connected");
        mqttConnected = true;
        _MqttObj.subscribe((_ProjectCode + "/DeviceToDevice").c_str());
        _MqttObj.subscribe((_ProjectCode + "/ServerToDevice").c_str());
        _MqttObj.subscribe((_ProjectCode + "/DeviceSetting").c_str());
        _MqttObj.subscribe((_ProjectCode + "/Share").c_str());
        _MqttObj.subscribe("Time/Tehran");
    } else {
        instance->Debug_ConsolePrintln("MQTT Connect Failed, State: " + String(_MqttObj.state()));
        mqttConnected = false;
    }
}


void AirNginClient::Mqtt__OnRecieve(char *topic, uint8_t *payload, unsigned int length)
{

  instance->onMessageCallback(topic, payload, length);  // فراخوانی callback ثبت‌شده توسط کاربر
  instance->_MqttRcv_IsWorking = false;

}



void AirNginClient::Tools__Reboot(String Reason = "")
{
  instance->Debug_ConsolePrintln( "\r\n\r\n!!!!!!!! Rquested To REBOOT !!!!!!!!\r\n      Due to : " + Reason + "\r\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n\r\n" );
  NVIC_SystemReset();
}



void AirNginClient::MemoClean(int p_start, int p_end, bool cleanFiles) {

  instance->Debug_ConsolePrintln( "MemoClean: " + String(p_start) + " To " + String(p_end) + (cleanFiles ? "  And Clean Files" : " "));
  

    for (int i = p_start; i <= p_end; i++)
    {
      EEPROM.write(i, 0xFF);
      if (i % 100 == 0)
      {
        delay(1);
      }
    }
    delay(100);
 
}


void AirNginClient::MemoCheck()
{
  int sum = 0;
 
    sum = (MemoReadByte(EP_MEMORYPOINT1H) * 256) + MemoReadByte(EP_MEMORYPOINT1L);

  if (sum != EP_MEMORYPOINT1L)
  {
    instance->MemoClean(EP_MEMORYSTART1, EP_MEMORYEND1, true);
    instance->MemoWriteByte(EP_MEMORYPOINT1H, (EP_MEMORYPOINT1L & 0xFF00) >> 8);
    instance->MemoWriteByte(EP_MEMORYPOINT1L, EP_MEMORYPOINT1L & 0xFF);
  }
  //................................................................................
  sum = 0;

    sum = (MemoReadByte(EP_MEMORYPOINT2H) * 256) + MemoReadByte(EP_MEMORYPOINT2L);
 
  if (sum != EP_MEMORYPOINT2L)
  {
    instance->MemoClean(EP_MEMORYSTART2, EP_MEMORYEND2, true);
    instance->MemoWriteByte(EP_MEMORYPOINT2H, (EP_MEMORYPOINT2L & 0xFF00) >> 8);
    instance->MemoWriteByte(EP_MEMORYPOINT2L, EP_MEMORYPOINT2L & 0xFF);
  }
}



long AirNginClient::Tools__Random(long a, long b)
{
  long d;
  if (a > b)
  {
    d = a;
    a = b;
    b = d;
  }
  d = (b - a) + 1;
  return a + (rand() % d);
}



uint32_t AirNginClient::Tools__GetChipID()
{
    uint32_t chipId = 0;

    uint32_t *uid = (uint32_t*)0x1FFF7A10;  // آدرس UID (برای STM32F1/F4/F7 – بسته به سری فرق دارد)
    chipId = uid[0] ^ uid[1] ^ uid[2];      // ترکیب 96 بیت → یک شناسه 32 بیتی


    return chipId;
}

void AirNginClient::Network_Reset()
{
  instance->Debug_ConsolePrintln("\r\n************* Network_Reset (Ethernet) ***************\r\n");

  // جلوگیری از اجرای IOT_Checker هنگام ریست
  instance->_MqttCon_Steps = 99;
  _IOT_PingTimeout = MIN_15;

  
  Ethernet.begin(mac);
  delay(1000);

  _MqttCon_IsConnected = false;
  instance->_MqttCon_Steps = 0;
}



bool AirNginClient::Network_HelthCheck()
{
  // اگر MQTT تا 15 دقیقه وصل نبود یک بار Ethernet ریست شود
  instance->Debug_ConsolePrintln("Network_HelthCheck (after " + String(MIN_15 - _IOT_PingTimeout) + " sec)");

  if (MIN_15 - _IOT_PingTimeout < SEC_30) return true;   // اگر کمتر از 30 ثانیه گذشته نیازی به تست نیست

  //................................................................
  // روی Ethernet به جای WiFi.gatewayIP از Ethernet.localIP یا gateway مشخص‌شده استفاده می‌کنیم
  IPAddress gateway = Ethernet.gatewayIP();   // تابع موجود در Ethernet
  if (gateway && Network_Ping(gateway.toString()))
  {
    instance->Debug_ConsolePrintln("\r\n************* SUCCESS PING FROM GATEWAY !! ***************\r\n");

    if (Network_Ping(_MqttBroker))
    {
      instance->Debug_ConsolePrintln("\r\n************* SUCCESS PING FROM BROKER !! ***************\r\n");

      // اگر به بروکر وصل بود، اجازه بدیم reconnect بشه
      _IOT_MqttTimeout = TIMER_NEED_RUN;
      return true;
    }
    else
    {
      // ممکنه اینترنت قطع باشه، یا فقط اینترانت فعال باشه
      if (MIN_15 - _IOT_PingTimeout > MIN_10)
      {
        instance->Debug_ConsolePrintln("\r\n************* AFTER 10 MINUTES, TRY OTHER BROKER DOMAIN !! ***************\r\n");
        if (_MqttBroker == "mqtt.airngin.ir")
        {
          if (Network_Ping("mqtt.airngin.com"))
          {
            _MqttBroker = "mqtt.airngin.com";
            instance->Debug_ConsolePrintln("\r\n************* SWITCHED TO MQTT.AIRNGIN.COM !! ***************\r\n");
            return true;
          }
        }
        else if (Network_Ping("mqtt.airngin.ir"))
        {
          _MqttBroker = "mqtt.airngin.ir";
          instance->Debug_ConsolePrintln("\r\n************* SWITCHED TO MQTT.AIRNGIN.IR !! ***************\r\n");
          return true;
        }
        return false;
      }
      else
      {
        return true; // کمتر از 10 دقیقه → فقط صبر کن
      }
    }
  }
  else
  {
    instance->Debug_ConsolePrintln("\r\n************* NO PING FROM GATEWAY !! ***************\r\n");
    // اگر gateway پینگ نشد → باید Ethernet ریست کنیم
    Network_Reset();
    return false;
  }
}





void AirNginClient::Send_RebootAndStatus()
{
  if (_Reboot)
  {
    //............................................ CLOUD BROKER
      if (_MqttCon_IsConnected)
      {
        _Reboot = false;
        MessageCloud__ViaMqtt_NotifyTo_User(10);
      }   
  }
}



bool AirNginClient::Network_Ping(String remote_host)
{
    instance->Debug_ConsolePrintln("Ping To " + remote_host);
    EthernetClient client;
    if (client.connect(remote_host.c_str(), 80)) {  // پورت 80 فقط برای تست باز بودن مسیر
        instance->Debug_ConsolePrintln("Ping (Ethernet) Success!!");
        client.stop();
        return true;
    } else {
        instance->Debug_ConsolePrintln("Ping (Ethernet) Failed!");
        return false;
    }
}

void AirNginClient::Mqtt_Send(String topic, String data, bool offlineSupport)
{

    //................................. Reject If Mesh Not Connected
    if (data == "")  return;
    if(data[0]=='{'){
      if( data.indexOf('"')<0 && data.indexOf('\'')>=0 ) data.replace('\'', '"');
    }
    topic = _ProjectCode + "/" + topic;
  
    //----------------------------------------------------------------------------- Cloud MqttBroker

    if (_MqttObj.connected())
      _MqttObj.publish(topic.c_str(), data.c_str());
    
}


//===========================================================================================
//                                         Cloud Message
//===========================================================================================

//..................................................................... Mqtt Send Notfication To User
void AirNginClient::MessageCloud__ViaMqtt_NotifyTo_User(int messageCode)
{
  Mqtt_Send("SendNotificationToUser", "{\"code\":\"" + String(messageCode) + "\",\"type\":\"30\"}", true); // Reboot Popup-Notify To User of Project
}
void AirNginClient::MessageCloud__ViaMqtt_NotifyTo_User(String& messageCode)
{
  Mqtt_Send("SendNotificationToUser", "{\"code\":\"" + messageCode + "\",\"type\":\"30\"}", true); // Reboot Popup-Notify To User of Project
}


//..................................................................... Mqtt Send SMS To User
void AirNginClient::MessageCloud__ViaMqtt_SMSTo_User(int messageCode)
{
  Mqtt_Send("SendNotificationToUser", "{\"code\":\"" +  String(messageCode) + "\",\"type\":\"20\"}", true); // Reboot Popup-Notify To User of Project
}
void AirNginClient::MessageCloud__ViaMqtt_SMSTo_User(String& messageCode)
{
  Mqtt_Send("SendNotificationToUser", "{\"code\":\"" +  messageCode + "\",\"type\":\"20\"}", true); // Reboot Popup-Notify To User of Project
}


//..................................................................... Mqtt Send SMS To Center
void AirNginClient::MessageCloud__ViaMqtt_SMSTo_Center(int messageCode)
{
  Mqtt_Send("SendNotificationToCenter", "{\"code\":\"" +  String(messageCode) + "\",\"type\":\"20\"}", true); // Reboot Popup-Notify To User of Project
}
void AirNginClient::MessageCloud__ViaMqtt_SMSTo_Center(String& messageCode)
{
  Mqtt_Send("SendNotificationToCenter", "{\"code\":\"" +  messageCode + "\",\"type\":\"20\"}", true); // Reboot Popup-Notify To User of Project
}


//===========================================================================================
//                                          End
//===========================================================================================

void AirNginClient::MemoWriteByte(int p_start, byte val)
{
  EEPROM.write(p_start, val);
  delay(1);
}


byte AirNginClient::MemoReadByte(int p_start)
{
  byte r = 0;
    r = 0 + EEPROM.read(p_start);
  return r;
}



bool AirNginClient::Tools__Memory_StrSet(String key, String val) {
  instance->Debug_ConsolePrintln("Tools__Memory_StrSet > key : " + key + " > val : " + val);


  if (key == "_SerialNo") {
    val.toUpperCase();
    strncpy(instance->config.SerialNo, val.c_str(), sizeof(instance->config.SerialNo));
  }
  else if (key == "_CloudClientId") {
    strncpy(instance->config.ClientId, val.c_str(), sizeof(instance->config.ClientId));
  }
  else if (key == "_ProjectCode") {
    val.toUpperCase();
    strncpy(instance->config.ProjectCode, val.c_str(), sizeof(instance->config.ProjectCode));
  }
  else if (key == "_EncryptionKey") {
    strncpy(instance->config.EncryptionKey, val.c_str(), sizeof(instance->config.EncryptionKey));
  }
  else if (key == "_EncryptionKeyProject") {
    strncpy(instance->config.EncryptionKeyProject, val.c_str(), sizeof(instance->config.EncryptionKeyProject));
  }
  else if (key == "_MqttUser") {
    strncpy(instance->config.MqttUser, val.c_str(), sizeof(instance->config.MqttUser));
  }
  else if (key == "_MqttPass") {
    strncpy(instance->config.MqttPass, val.c_str(), sizeof(instance->config.MqttPass));
  }
  else if (key == "_MqttBroker") {
    strncpy(instance->config.MqttBroker, val.c_str(), sizeof(instance->config.MqttBroker));
  }
  else if (key == "_IsUseEncrypted") {
    instance->_IsUsedEncryptionKey = instance->config.isUseEncrypted ? "true" : "false";
  }

  return true;
}




String AirNginClient::Tools__Memory_StrGet(String key)
{


  bool checkNull = false;
  String res = "";
  int v = 0;

  
   
    if (key == "_SerialNo")
    {
      res = MemoReadString(EP_SERIAL_S, EP_SERIAL_E);
      if (res == "" || res[0] == char(255))
        res = "0000000000";
    }
    else if (key == "_CloudClientId")
    {
      res = MemoReadString(EP_CLIENTID_S, EP_CLIENTID_E);
      if (res[0] == char(255))
        res = "";
    }
    else if (key == "_ProjectCode")
    {
      res = MemoReadString(EP_PROJECTCODE_S, EP_PROJECTCODE_E);
      // res.toUpperCase();
      checkNull = true;
    }
    else if (key == "_EncryptionKey")
    {
      res = MemoReadString(EP_ENCRYPTIONKEY_S, EP_ENCRYPTIONKEY_E);
      checkNull = true;
    }
    else if (key == "_EncryptionKeyProject")
    {
      res = MemoReadString(EP_ENCRYPTIONKEY_PROJECT_S, EP_ENCRYPTIONKEY_PROJECT_E);
      checkNull = true;
    }
    else if (key == "_MqttUser")
    {
      res = MemoReadString(EP_MQTTUSER_S, EP_MQTTUSER_E);
      checkNull = true;
    }
    else if (key == "_MqttPass")
    {
      res = MemoReadString(EP_MQTTPASS_S, EP_MQTTPASS_E);
      checkNull = true;
    }
    else if (key == "_MqttBroker")
    {
      res = MemoReadString(EP_MQTTBROKER_S, EP_MQTTBROKER_E);
      checkNull = true;
    }
    

  if (checkNull && res.charAt(0) == char(255))
    res = "";

  instance->Debug_ConsolePrintln("Tools__Memory_StrGet > key : " + key + " > val : " + res);

  return res;
}

void AirNginClient::saveConfig() {
  instance->Debug_ConsolePrintln("Start Config Saved to EEPROM");
  EEPROM.put(0, config);
  instance->Debug_ConsolePrintln("End Config Saved to EEPROM");
}

void AirNginClient::loadConfig() {
  EEPROM.get(0, config);
  instance->Debug_ConsolePrintln("Config Loaded from EEPROM");
}


void AirNginClient::MemoWriteString(int p_start, int p_end, String val)
{
  instance->Debug_ConsolePrintln("StrSet 4");

  int maxLen = p_end - p_start + 1; // فضای مجاز
  int strLen = val.length();
  if (strLen > maxLen) strLen = maxLen; // برش اگر رشته بزرگتر بود

  // نوشتن فقط کاراکترهای تغییر کرده
  for (int i = 0; i < maxLen; i++) {
    byte b = (i < strLen) ? val[i] : 0; // صفرگذاری بعد از انتهای رشته
    EEPROM.update(p_start + i, b);
  }

  instance->Debug_ConsolePrintln("StrSet 5");

}

String AirNginClient::MemoReadString(int p_start, int p_end)
{
  String val = "";
  int i, empty = 0;
  char v, cFF = char(0xFF);
  for (i = p_start; i <= p_end; i++)
  {

      v = EEPROM.read(i);
      if (v == '\0')
        break;
      if (v == cFF)
        empty++;
      val += ("" + String(v));

  }
  if (val.length() == empty)
    val = "";
    
  return val;
}




byte AirNginClient::Tools__StringToByte(String inp)
{
  byte r = 0;

    r = (byte)((inp).toInt() & 0xFF);

  return r;
}


int AirNginClient::Tools__StringToInt(String inp)
{
  int r = 0;

    inp.trim();
    r = (inp).toInt();

  return r;
}

void AirNginClient::Tools__Memory_ClearAll() {
  instance->Debug_ConsolePrintln("⚠️ Clearing all EEPROM ...");

  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.update(i, 0);   // 👈 می‌تونی به جای 0 بزنی 0xFF
  }

  instance->Debug_ConsolePrintln("✅ EEPROM Cleared!");
}


// void AirNginClient::Tools__SettingRead()
// {
//   instance->Debug_ConsolePrintln("Setting Read... ");

//   // 👇 کل struct رو از EEPROM بخون
//   EEPROM.get(0, instance->config);

//   //...................................... Load Important Data
//   instance->_Configured = instance->config.Configured;
//   instance->_StartMode  = String(instance->config.StartMode);

//   //...................................... Read Other Settings
//   instance->_CloudClientId       = String(instance->config.ClientId);
//   instance->_ProjectCode         = String(instance->config.ProjectCode);
//   instance->_EncryptionKey       = String(instance->config.EncryptionKey);
//   instance->_EncryptionKeyProject= String(instance->config.EncryptionKeyProject);
//   instance->_MqttBroker          = String(instance->config.MqttBroker);
//   instance->_MqttUser            = String(instance->config.MqttUser);
//   instance->_MqttPass            = String(instance->config.MqttPass);

//   // پاکسازی فاصله‌ها
//   instance->_CloudClientId.trim();
//   instance->_ProjectCode.trim();
//   instance->_MqttUser.trim();
//   instance->_MqttPass.trim();
//   instance->_MqttBroker.trim();
//   instance->_EncryptionKey.trim();
//   instance->_EncryptionKeyProject.trim();

//   instance->Debug_ConsolePrintln("Config loaded into variables.");
  
// }

String safeString(const char *src, int maxLen) {
  // اگه کلش 0xFF یا صفر بود، خروجی خالی بده
  bool empty = true;
  for (int i = 0; i < maxLen; i++) {
    if (src[i] != (char)0xFF && src[i] != 0) {
      empty = false;
      break;
    }
  }
  if (empty) return "";

  // رشته رو درست بساز
  return String(src);
}


void AirNginClient::Tools__SettingRead()
{
  instance->Debug_ConsolePrintln("Setting Read... ");

  // کل struct رو بخون
  EEPROM.get(0, instance->config);

  //...................................... Safe Read Other Settings
  instance->_CloudClientId        = safeString(instance->config.ClientId, sizeof(instance->config.ClientId));
  instance->_ProjectCode          = safeString(instance->config.ProjectCode, sizeof(instance->config.ProjectCode));
  instance->_EncryptionKey        = safeString(instance->config.EncryptionKey, sizeof(instance->config.EncryptionKey));
  instance->_EncryptionKeyProject = safeString(instance->config.EncryptionKeyProject, sizeof(instance->config.EncryptionKeyProject));
  instance->_MqttBroker           = safeString(instance->config.MqttBroker, sizeof(instance->config.MqttBroker));
  instance->_MqttUser             = safeString(instance->config.MqttUser, sizeof(instance->config.MqttUser));
  instance->_MqttPass             = safeString(instance->config.MqttPass, sizeof(instance->config.MqttPass));
  instance->_IsUsedEncryptionKey = instance->config.isUseEncrypted ? "true" : "false";

  // پاکسازی فاصله‌های اضافه
  instance->_CloudClientId.trim();
  instance->_ProjectCode.trim();
  instance->_MqttUser.trim();
  instance->_MqttPass.trim();
  instance->_MqttBroker.trim();
  instance->_EncryptionKey.trim();
  instance->_EncryptionKeyProject.trim();

  instance->Debug_ConsolePrintln("Config loaded into variables.");
}




AirNginClient::~AirNginClient() {
    // Clean-up code if needed
}


//===========================================================================================
//                                      Config (AP Mode)
//===========================================================================================

void AirNginClient::Tools__SettingDefault()
{
  instance->Debug_ConsolePrintln("Setting To Dafault >>> ");
  //..................
  _ProjectCode = "";
  _EncryptionKey = "";
  _EncryptionKeyProject = "";
  _MqttBroker = "";
  _MqttUser = "";
  _MqttPass = "";

}


void AirNginClient::Tools__SettingSave()
{
  instance->Debug_ConsolePrintln("Setting Save... ");
 
    instance->_CloudClientId.trim();
    Tools__Memory_StrSet("_CloudClientId", instance->_CloudClientId);
    Tools__Memory_StrSet("_ProjectCode", instance->_ProjectCode);
    Tools__Memory_StrSet("_EncryptionKey", instance->_EncryptionKey);
    Tools__Memory_StrSet("_EncryptionKeyProject", instance->_EncryptionKeyProject);
    Tools__Memory_StrSet("_MqttBroker", instance->_MqttBroker);
    Tools__Memory_StrSet("_MqttUser", instance->_MqttUser);
    Tools__Memory_StrSet("_MqttPass", instance->_MqttPass);

}



void AirNginClient::Tools__SettingShowInfo()
{
    if(!instance->isDebugEnabled) return;

    instance->Debug_ConsolePrintln("\r\n -----------------------\r\nSetting Show Info\r\n-----------------------");
    instance->Debug_ConsolePrintln("_SerialNo: " + instance->_SerialCloud);
    instance->Debug_ConsolePrintln("_ProjectCode: " + instance->_ProjectCode);
    instance->Debug_ConsolePrintln("_EncryptionKey: " + instance->_EncryptionKey);
    instance->Debug_ConsolePrintln("_EncryptionKeyProkect: " + instance->_EncryptionKeyProject);
    instance->Debug_ConsolePrintln("\r\n _MqttBroker: " + instance->_MqttBroker);
    instance->Debug_ConsolePrintln("_MqttUser: " + instance->_MqttUser);
    instance->Debug_ConsolePrintln("_MqttPass: " +instance-> _MqttPass);
    instance->Debug_ConsolePrintln("_CloudClientId: " + instance->_CloudClientId);
    instance->Debug_ConsolePrintln("_IsUsedEncryptionKey: " + instance->_IsUsedEncryptionKey);
    instance->Debug_ConsolePrintln("");

    printNetInfo();
  
}



void AirNginClient::SetClock(String UTC) {
  static time_t currentEpoch = 0; // ذخیره‌ی ساعت فعلی داخل کلاس (به‌جاش می‌تونی متغیر عضو بذاری)

  if (UTC.length() > 0) {
    instance->Debug_ConsolePrintln("SetClock : Try to sync the time from API...");

    struct tm timeinfo = {};
    const char* dateTimeStr = UTC.c_str();

    if (sscanf(dateTimeStr, "%d-%d-%dT%d:%d:%d",
               &timeinfo.tm_year, &timeinfo.tm_mon, &timeinfo.tm_mday,
               &timeinfo.tm_hour, &timeinfo.tm_min, &timeinfo.tm_sec) == 6) {

      timeinfo.tm_year -= 1900;
      timeinfo.tm_mon  -= 1;

      time_t t = mktime(&timeinfo);
      if (t != -1) {
        currentEpoch = t; // ذخیره کردن ساعت دریافتی
        instance->Debug_ConsolePrintln("SetClock : Time set from API : " + String(asctime(&timeinfo)));
        return;
      }
    } else {
      instance->Debug_ConsolePrintln("SetClock : Invalid date format. Use YYYY-MM-DDTHH:MM:SS");
    }
  }

  // اگر UTC از API نداشت → استفاده از NTP
  instance->Debug_ConsolePrintln("SetClock : Try to sync the time from NTP...");

  EthernetUDP Udp;
  const char* ntpServer = "pool.ntp.org";
  const int NTP_PACKET_SIZE = 48;
  byte packetBuffer[NTP_PACKET_SIZE];

  DNSClient dns;
  dns.begin(Ethernet.dnsServerIP());

  IPAddress ntpIP;
  if (dns.getHostByName(ntpServer, ntpIP) == 1) {
    Udp.begin(2390);
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011;
    Udp.beginPacket(ntpIP, 123);
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();

    delay(1000);
    if (Udp.parsePacket()) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord  = word(packetBuffer[42], packetBuffer[43]);
      unsigned long secsSince1900 = (highWord << 16) | lowWord;
      const unsigned long seventyYears = 2208988800UL;
      currentEpoch = secsSince1900 - seventyYears;

      instance->Debug_ConsolePrintln("SetClock : NTP Time sync OK: " + String(ctime(&currentEpoch)));
    } else {
      instance->Debug_ConsolePrintln("SetClock : NTP response failed");
    }
  } else {
    instance->Debug_ConsolePrintln("SetClock : DNS lookup for NTP server failed");
  }
}




void AirNginClient::TimerSec_Refresh()
{
  //-------------------------------------------- TIMER
 
    _TimerSecCurrent = millis();
    _TimerSecDef = ((_TimerSecCurrent - _TimerSecOld) / 1000) & 0xFF;
    if (_TimerSecDef < 0)
      _TimerSecDef = 1;
    if (_TimerSecDef >= 1)
    {
      _TimerSecOld = _TimerSecCurrent;
      _TimerForActions += _TimerSecDef;
    
        if (_IOT_PingTimeout > 0)
        {
          _IOT_PingTimeout -= _TimerSecDef;
          if (_IOT_PingTimeout <= 0) _IOT_PingTimeout = TIMER_NEED_RUN;
        }
        if (_IOT_MqttTimeout > 0)
        {
          _IOT_MqttTimeout -= _TimerSecDef;
          if (_IOT_MqttTimeout <= 0) _IOT_MqttTimeout = TIMER_NEED_RUN;
        }
        if (_IOT_ModemTimeout > 0)
        {
          _IOT_ModemTimeout -= _TimerSecDef;
          if (_IOT_ModemTimeout <= 0) _IOT_ModemTimeout = TIMER_NEED_RUN;
        }

    }

}


void AirNginClient::DebugPrint(const char* fmt, ...) {
    if (!isDebugEnabled) return;
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.print(F("Debug: "));
    Serial.println(buf);
}


void AirNginClient::Debug_ConsolePrint(String p1)
{
    if(!instance->isDebugEnabled) return;
  
    p1.replace("\r\n", "\n");
    p1.replace('\r', '\n');
    Serial.println("Debug :: \t"+p1);
  
}

void AirNginClient::Debug_ConsolePrint(int p1) { Debug_ConsolePrint(String(p1)); }
void AirNginClient::Debug_ConsolePrint(long p1) { Debug_ConsolePrint(String(p1)); }
void AirNginClient::Debug_ConsolePrint(float p1) { Debug_ConsolePrint(String(p1)); }


void AirNginClient::Debug_ConsolePrintln(String msg)
{
    // اگر می‌خواهی فقط وقتی سریال وصل است چاپ شود، می‌توانی !Serial را هم چک کنی
    if (!isDebugEnabled) return;

    // نرمال‌سازی CRLF/CR -> LF
    msg.replace("\r\n", "\n");
    msg.replace('\r', '\n');

    Serial.print(F("Debug ::\t"));
    Serial.println(msg);
    // Serial.flush(); // معمولاً لازم نیست
}


void AirNginClient::Debug_ConsolePrintln(int p1) { Debug_ConsolePrint(String(p1)); }
void AirNginClient::Debug_ConsolePrintln(long p1) { Debug_ConsolePrint(String(p1)); }
void AirNginClient::Debug_ConsolePrintln(float p1) { Debug_ConsolePrint(String(p1)); }



// // ---------- Add Data ----------
// const String serverURL = "https://app.airngin.com/api/v1.0/DeviceCloud/";
// // تابع عمومی برای درخواست‌های HTTP
// String AirNginClient::HttpRequest(const char* method, const char* path, const char* body) {
//     EthernetClient client;
//     String response = "";

//     if (client.connect(server, port)) {
//         // --- ساختن Header
//         client.print(method);
//         client.print(" ");
//         client.print(path);
//         client.println(F(" HTTP/1.1"));
//         client.print(F("Host: "));
//         client.println(server);
//         client.println(F("User-Agent: STM32Ethernet/1.0"));
//         client.println(F("Connection: close"));

//         if (body && strlen(body) > 0) {
//             client.println(F("Content-Type: application/json"));
//             client.print(F("Content-Length: "));
//             client.println(strlen(body));
//             client.println();
//             client.println(body);
//         } else {
//             client.println();
//         }

//         // --- خواندن پاسخ
//         unsigned long timeout = millis();
//         while (client.connected() && millis() - timeout < 5000) {
//             while (client.available()) {
//                 char c = client.read();
//                 response += c;
//                 timeout = millis();
//             }
//         }
//         client.stop();
//     }
//     return response;
// }

// //--------------------------------------
// // حالا می‌تونیم توابع اصلی رو ساده کنیم
// //--------------------------------------

// bool AirNginClient::addDataToCloud(const char* key, const char* value) {
//     char body[128];
//     snprintf(body, sizeof(body), "{\"projectCode\":\"%s\",\"clientId\":\"%s\",\"key\":\"%s\",\"value\":\"%s\"}", 
//              _ProjectCode, _CloudClientId, key, value);

//     String resp = HttpRequest("POST", "/api/Cloud/AddData", body);

//     if (resp.indexOf("\"isSuccess\":true") > 0) {
//         DebugPrint(F("Data added successfully"), true);
//     } else {
//         DebugPrint(F("Failed to add data"), true);
//     }

//     return true;
// }

// String AirNginClient::getDataFromCloud(const char* key) {
//     char body[128];
//     snprintf(body, sizeof(body), "{\"projectCode\":\"%s\",\"clientId\":\"%s\",\"key\":\"%s\"}", 
//              _ProjectCode, _CloudClientId, key);

//     String resp = HttpRequest("POST", "/api/Cloud/GetData", body);

//     int pos = resp.indexOf("\"value\":\"");
//     if (pos > 0) {
//         pos += 9;
//         int end = resp.indexOf("\"", pos);
//         return resp.substring(pos, end);
//     }
//     return "";
// }

// bool AirNginClient::deleteDataFromCloud(const char* key) {
//     char body[128];
//     snprintf(body, sizeof(body), "{\"projectCode\":\"%s\",\"clientId\":\"%s\",\"key\":\"%s\"}", 
//              _ProjectCode, _CloudClientId, key);

//     String resp = HttpRequest("POST", "/api/Cloud/DeleteData", body);

//     if (resp.indexOf("\"isSuccess\":true") > 0) {
//         DebugPrint(F("Data deleted successfully"), true);
//     } else {
//         DebugPrint(F("Failed to delete data"), true);
//     }

//     return true;

// }
