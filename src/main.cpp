#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h> 

#define R_Led_pin 16
#define G_Led_pin 2
#define B_Led_pin 14
#define DET_Allow 0
#define DET_Check 12
#define DET_Ign 13

#define ID 11 // Sender ID 10 for TX 11 for RX1 12 for RX2
int IGN_Status=0; // 0 - disconected 1-no igniter 2-Ready unarmed 3-Ready ARMED
int IGN_FIRE=0;


unsigned long timer;
#define SEND_TIMEOUT 500


volatile boolean callbackCalled;

#define WIFI_CHANNEL 4
//uint8_t remoteMac_1[] = {0xA0, 0x20, 0xA6, 0x13, 0x60, 0xA8};
uint8_t remoteMac_1[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; 
uint8_t remoteMac_2[] = {0xA0, 0x20, 0xA6, 0x13, 0x60, 0xA8}; //TX MAC
char macBuf[18];

struct __attribute__((packed)) MESSAGE {
  int Sender; // Message ID 100 for TX>ALL 101 TX>RX1 102 TX>RX2 200 for RX1 300 for RX2
  int IGN_Status;
  int IGN_FIRE;
} messageData;




void formatMacAddress(const uint8_t *macAddr, char *buffer, int maxLength)
{
  snprintf(buffer, maxLength, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}

void LED(int R, int G, int B) {
  analogWrite(R_Led_pin, R);
  analogWrite(G_Led_pin, G);
  analogWrite(B_Led_pin, B);
}

void FIRE() {
  if(IGN_Status==3&&IGN_FIRE==1&&digitalRead(DET_Allow)){
Serial.println("Kaboom!!!");
digitalWrite(DET_Ign, true);
  }
}

  


void initEspNow() {
  if (esp_now_init()!=0) {
    Serial.println("*** ESP_Now init failed");
    ESP.restart();
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_add_peer(remoteMac_1, ESP_NOW_ROLE_COMBO, WIFI_CHANNEL, NULL, 0);
  esp_now_add_peer(remoteMac_2, ESP_NOW_ROLE_COMBO, WIFI_CHANNEL, NULL, 0);
  
  esp_now_register_recv_cb([](uint8_t *mac, uint8_t *data, uint8_t len) {

    char macStr[18];
    formatMacAddress(mac, macStr, 18);
    memcpy(&messageData, data, sizeof(messageData));

    Serial.print("recv_cb, msg from device: "); 
    Serial.println(macStr);

      if(messageData.Sender==201) {
        Serial.print("Nadajnik do RX1: ");
        Serial.println(messageData.IGN_FIRE);
        IGN_FIRE=messageData.IGN_FIRE;
        FIRE();
      }
      if(messageData.Sender==100) {
        Serial.print("Nadajnik do wszystkich: ");
        Serial.print(messageData.IGN_Status);
        Serial.print(messageData.IGN_FIRE);
      }
      if(!(messageData.Sender==201) && !(messageData.Sender==100)) {
        Serial.print("Niepoprawny nadawca ");
        Serial.println(messageData.Sender);
      }

    
  });

  esp_now_register_send_cb([](uint8_t* mac, uint8_t sendStatus) {
    Serial.printf("send_cb, send done, status = %i\n", sendStatus);
    callbackCalled = true;
  });
}


void sendAll() {
  messageData.Sender = 200;// Message ID 100 for TX>ALL 101 TX>RX1 102 TX>RX2 200>ALL 201 for RX1>TX etc.
uint8_t bs[sizeof(messageData)];
  memcpy(bs, &messageData, sizeof(messageData));
  esp_now_send(NULL, bs, sizeof(messageData)); // NULL means send to all peers

} 

void sendTo(uint8_t* mac) {
  messageData.Sender = 201;// Message ID 100 for TX>ALL 101 TX>RX1 102 TX>RX2 200 for RX1 300 for RX2
uint8_t bs[sizeof(messageData)];
  memcpy(bs, &messageData, sizeof(messageData));
  esp_now_send(mac, bs, sizeof(messageData)); 

} 


void setup() {
  delay(100);
  
  //PIN init
  pinMode(DET_Allow, INPUT_PULLUP);
  pinMode(DET_Check, INPUT_PULLUP);
  pinMode(DET_Ign, OUTPUT);
  pinMode(R_Led_pin, OUTPUT);
  pinMode(G_Led_pin, OUTPUT);
  pinMode(B_Led_pin, OUTPUT);
  LED(0,0,0);
   Serial.begin(115200);

  //ESP-NOW Init

  WiFi.mode(WIFI_STA); // Station mode for esp-now sensor node
  WiFi.disconnect();

  Serial.printf("This mac: %s, \n", WiFi.macAddress().c_str()); 
  Serial.printf("target mac 1: %02x:%02x:%02x:%02x:%02x:%02x\n", remoteMac_1[0], remoteMac_1[1], remoteMac_1[2], remoteMac_1[3], remoteMac_1[4], remoteMac_1[5]); 
  Serial.printf("target mac 2: %02x:%02x:%02x:%02x:%02x:%02x\n", remoteMac_2[0], remoteMac_2[1], remoteMac_2[2], remoteMac_2[3], remoteMac_2[4], remoteMac_2[5]); 
  Serial.printf(", channel: %i\n", WIFI_CHANNEL); 
  formatMacAddress(remoteMac_1, macBuf, 18);
  Serial.println(macBuf);
  initEspNow();

  
}






void loop() {
  if(millis()>timer+SEND_TIMEOUT){
    timer=millis();
    if(digitalRead(DET_Check)){
      LED(0,255,255);
      IGN_Status = 1;
    }  
    if(!digitalRead(DET_Check)) {
     LED(255,0,0);
      IGN_Status = 2;
    }
    if(!digitalRead(DET_Check)&&!digitalRead(DET_Allow)) {
    IGN_Status = 3;
    LED(255,0,255);
    }

  
  messageData.IGN_Status = IGN_Status;
  sendTo(remoteMac_2);
  }
}


