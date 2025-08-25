#include <AirNgin.h>

#define KEY_OF_CENTER "AIRN" // IT'S PRODUCER CENTER CODE

// -------- SETTING PIN --------
#define ETH_CS 10
#define SD_CS 4

#define LED_PIN    LED_BUILTIN    // روی این برد = PA5 (LD2)
#define Pushbotton PC13 // PIN FOR GO TO Config Panel and AP MODE


EthernetClient ethClient;
AirNginClient airnginClient(ethClient);

String _SerialNo="";

unsigned long _TimerSecCurrent, _TimerSecOld = 0;
byte _TimerSecDef = 0;

byte _TimerKeyPush = 0;

// بررسی وضعیت شبکه در هر ثانیه
static unsigned long lastCheck = 0;


unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 10000; // هر 10 ثانیه یک بار تلاش مجدد

int maxRetries = 10;                 // تعداد تلاش‌ها


void MqttSend(String topic, String data){

  airnginClient.Mqtt_Send(topic, data);

}


// تست اتصال اینترنت (پورت DNS گوگل)
bool checkInternet() {

  if (ethClient.connect(IPAddress(8, 8, 8, 8), 53)) {
    ethClient.stop();
    return true;
  }
  return false;
}



void setup() {
  bool isDebugEnabled = true;
  delay(20);
  Serial.begin(9600);
  while (!Serial) {}
  Tools__SerialBarcodeReload();

  pinMode(SD_CS, OUTPUT);
  delay(20);
  pinMode(Pushbotton, INPUT_PULLUP);
  delay(20);
  pinMode(LED_PIN, OUTPUT);
  delay(100);
  digitalWrite(SD_CS, HIGH);
  delay(20);
  airnginClient.begin(_SerialNo, isDebugEnabled);
  delay(20);
  airnginClient.GenerateMAC(airnginClient.mac);
  airnginClient.PrintMAC(airnginClient.mac);
  Ethernet.init(ETH_CS);
  delay(200);

  Serial.println("HW: " + String(Ethernet.hardwareStatus()));
  Serial.println("Link: " + String(Ethernet.linkStatus()));

  const unsigned long dhcpTimeout = 10000; // ۱۰ ثانیه
  const unsigned long retryDelay  = 3000;  // ۳ ثانیه
  
  int maxRetries = 5;

  int attempt = 0;
  bool success = false;

  while (attempt < maxRetries && !success) {
    Serial.print("ETH: Trying to connect (attempt ");
    Serial.print(attempt + 1);
    Serial.println(")...");

    unsigned long start = millis();
    int result = Ethernet.begin(airnginClient.mac);

    while (Ethernet.localIP() == INADDR_NONE && (millis() - start) < dhcpTimeout) {
      delay(100);
    }

    if (Ethernet.localIP() != INADDR_NONE) {
      Serial.print("✅ ETH: Connected! IP = ");
      Serial.println(Ethernet.localIP());
      success = true;
    } else {
      Serial.println("❌ ETH: Failed to get IP, retrying...");
      delay(retryDelay);
    }
    attempt++;
  }

  if (!success) {
    Serial.println("⚠️ DHCP failed, using fallback IP");
    IPAddress fallbackIP(192, 168, 0, 200);
    IPAddress fallbackDNS(8, 8, 8, 8);
    IPAddress fallbackGW(192, 168, 0, 1);
    IPAddress fallbackSN(255, 255, 255, 0);
    Ethernet.begin(airnginClient.mac, fallbackIP, fallbackDNS, fallbackGW, fallbackSN);
  }

  airnginClient.printNetInfo();
  delay(3000);
  airnginClient.setOnMessageCallback(myMqttCallback);
}



void loop() {
    delay(10);    
    airnginClient.loop();
    TimerSec_Refresh();             //read Time
    if(digitalRead(Pushbotton)==LOW){
      _TimerKeyPush += _TimerSecDef;
    }
    else
      _TimerKeyPush = 0;
    if(_TimerKeyPush>=5){   //
      _TimerKeyPush=0;

      if(airnginClient.isConfigMode)
      {
        digitalWrite(LED_PIN, LOW);
        airnginClient.isConfigMode=false;
        airnginClient._Mqtt_TryConnecting = false;
        airnginClient.stopBroadcast = false;


      }else{
        digitalWrite(LED_PIN, HIGH);
        airnginClient.isConfigMode=true;  
        airnginClient._Mqtt_TryConnecting = true;
        airnginClient.stopBroadcast = false;
      } 
    }

    checkEthernetConnection();  // بررسی وضعیت شبکه

  // هندل دکمه
  if (digitalRead(Pushbotton) == LOW) {
    _TimerKeyPush += _TimerSecDef;
  } else {
    _TimerKeyPush = 0;
  }
  if (_TimerKeyPush >= 5) {
    _TimerKeyPush = 0;

    if (airnginClient.isConfigMode) {
      digitalWrite(LED_PIN, LOW);
      airnginClient.isConfigMode = false;
      airnginClient._Mqtt_TryConnecting = false;
      airnginClient.stopBroadcast = false;
    } else {
      digitalWrite(LED_PIN, HIGH);
      airnginClient.isConfigMode = true;
      airnginClient._Mqtt_TryConnecting = true;
      airnginClient.stopBroadcast = false;
    }
  }
}


void Tools__SerialBarcodeReload()
{

    String chip = (String(airnginClient.Tools__GetChipID()) + "0000000").substring(0, 7);
    if (chip == "0000000")
      chip = (String(airnginClient.Tools__Random(1000000, 9999998)) + "0000000").substring(0, 7);
    _SerialNo = KEY_OF_CENTER + ("000" + chip).substring(0, 10);
  

  Serial.println("_SerialNo : " + _SerialNo);
}


void TimerSec_Refresh()
{
  //-------------------------------------------- TIMER
    _TimerSecCurrent = millis();
    _TimerSecDef = ((_TimerSecCurrent - _TimerSecOld) / 1000) & 0xFF;
    if (_TimerSecDef < 0)
      _TimerSecDef = 1;
    if (_TimerSecDef >= 1)
      _TimerSecOld = _TimerSecCurrent;
  
}



void myMqttCallback(char *topic, uint8_t *payload, unsigned int length) {

  Serial.println("**************** myMqttCallback ********************************");
  String projectTopic = String(topic);

  int p = projectTopic.indexOf('/');
  if (projectTopic.substring(0, p) != airnginClient._ProjectCode)
    return;
  projectTopic = projectTopic.substring(p + 1);


  Serial.print(F("MQTT Payload: "));
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();


  //   // StaticJsonDocument<512> doc;  // فرض می‌کنیم اندازه 512 بایت کافی است
  //   // DeserializationError error = deserializeJson(doc, payload, length);
  //   // if (error) {
  //   //     Serial.println("JSON parse failed!");
  //   //     return;
  //   // }

  //   if (projectTopic == "DeviceSetting") {
  //       String opr = doc["operationName"].as<String>();
  //       if (opr == "save_scenario" || opr == "delete_scenario") {
  //           // کد مربوط به ذخیره یا حذف سناریو
  //       } else if (opr == "save_setting") {
  //           if (doc["deviceSerial"].as<String>() == airnginClient._SerialCloud) {
  //               String cmd, d = doc["value"].as<String>();
  //               if (d != "") {
  //                   deserializeJson(doc, d);
  //                   if (doc["request"]["commandName"] && doc["request"]["commandData"]) {
  //                       cmd = doc["request"]["commandName"].as<String>();
  //                       if (cmd == "saveScenarioOperation") {
  //                           JsonVariant inp = doc["scenarioOperation"].as<JsonVariant>();
  //                           // پردازش سناریو
  //                       }
  //                       // سایر دستورات...
  //                   }
  //               }
  //           }
  //       }
  //   }
  // سایر موضوعات (topics) مشابه...
}


void checkEthernetConnection() {

// بررسی وضعیت شبکه هر 5 ثانیه
if (millis() - lastCheck > 5000) {
  lastCheck = millis();

  auto hw = Ethernet.hardwareStatus();
  auto link = Ethernet.linkStatus();
  Serial.println("HW: " + String(hw));
  Serial.println("Link: " + String(link));

  if (hw == EthernetNoHardware) {
    Serial.println("❌ No Ethernet hardware detected!");
    return;
  }

  if (link == LinkOFF) {
    Serial.println("⚠️ Cable unplugged!");
    return;
  }

  // فقط وقتی IP نداره دوباره DHCP بگیر
  if (Ethernet.localIP() == INADDR_NONE) {
    Serial.println("❌ No IP, retrying DHCP...");
    if (Ethernet.begin(airnginClient.mac) == 0) {
      Serial.println("DHCP failed, using fallback IP");
      IPAddress fallbackIP(192, 168, 0, 200);
      IPAddress fallbackDNS(8, 8, 8, 8);
      IPAddress fallbackGW(192, 168, 0, 1);
      IPAddress fallbackSN(255, 255, 255, 0);
      Ethernet.begin(airnginClient.mac, fallbackIP, fallbackDNS, fallbackGW, fallbackSN);
    } else {
      Serial.println("✅ DHCP success after reconnect!");
      airnginClient.printNetInfo();
    }
  } else {
    airnginClient._Mqtt_TryConnecting = true; // اجازه می‌دهیم MQTT تلاش کند وصل شود
    Serial.println("✅ Ethernet OK, IP: " + Ethernet.localIP().toString());
  }
}

}