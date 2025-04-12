#include <WiFi.h>
#include "esp_camera.h"

WiFiServer server(100);

byte Stop[17]={0xff,0x55,0x0a,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x0C,0x00,0x00};
byte RX_package[17] = {0};
byte dataLen, index_a = 0;
char buffer[52];
unsigned char prevc=0;
bool isStart = false;
bool ED_client = true;
bool WA_en = false;
String sendBuff;
String Version = "Firmware Version is 0.12.21";
int action = 0;
char device = 0;
byte val = 0;
#define gpLED  4      // Light
void CameraWebServer_init();

void setup()
{
  Serial.begin(115200);
  //http://192.168.4.1/control?var=framesize&val=3
  //http://192.168.4.1/Test?var=
  CameraWebServer_init();
  server.begin();
  delay(100);
  pinMode(gpLED, OUTPUT); //Light
  ledcSetup(7, 5000, 8);
  ledcAttachPin(gpLED, 7);  //pin4 is LED

  for (int i = 0; i < 5; i++) 
  {
    ledcWrite(7, 20); // flash led
    delay(50);
    ledcWrite(7, 0);
    delay(50);
  }
}

unsigned char readBuffer(int index_r)
{
  return buffer[index_r]; 
}

void writeBuffer(int index_w,unsigned char c)
{
  buffer[index_w]=c;
}
void parseData()
{ 
    isStart = false;
    action = readBuffer(9);
    device = readBuffer(10);
    val = readBuffer(12);
    if (device==0x05)
    {
      if(val<20)val=0;
      ledcWrite(7, val);
      delay(1);
    }
}


void loop()
{
  WiFiClient client = server.available();
  if (client)
  {
    WA_en = true;
    ED_client = true;
    Serial.println("[Client connected]");

    while (client.connected())
    {
      if (client.available())
      {
        unsigned char clientBuff = client.read()&0xff;
        Serial.write(clientBuff);
        if(clientBuff==0x55&&isStart==false)
        {
          if(prevc==0xff)
          {
            index_a=1;
            isStart = true;
          }
        }
        else
        {
          prevc = clientBuff;
          if(isStart)
          {
            if(index_a==2)
            {
            dataLen = clientBuff; 
            }
            else if(index_a>2)
            {
              dataLen--;
            }
            writeBuffer(index_a,clientBuff);
          }
        }
        index_a++;
        if(index_a>120)
        {
          index_a=0; 
          isStart=false;
        }
        if(isStart&&dataLen==0&&index_a>3)
        { 
          isStart = false;
          parseData();
          index_a=0;
        }
      }
        
        //client.print(clientBuff);
            
      static unsigned long Test_time = 0;
      if (millis() - Test_time > 1000)
      {
        Test_time = millis();
        if (0 == (WiFi.softAPgetStationNum()))
        {
          Serial.write(Stop,17);
          
        }
      }
    }
    Serial.write(Stop,17);
    client.stop();
    Serial.println("[Client disconnected]");
  }
  else
  {
    if (ED_client == true)
    {
      ED_client = false;
      Serial.write(Stop,17);
    }
  }
}
