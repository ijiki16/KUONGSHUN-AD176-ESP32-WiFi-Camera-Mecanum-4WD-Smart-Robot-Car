//www.kuongshun.com

#include    <Servo.h>

#define MOTOR_PIN           9
#define PWM1_PIN            5
#define PWM2_PIN            6
#define SHCP_PIN            2                  // The displacement of the clock
#define EN_PIN              7                  // Can make control
#define DATA_PIN            8                  // Serial data
#define STCP_PIN            4                  // Memory register clock            
#define Trig_PIN            12
#define Echo_PIN            13
#define LEFT_LINE_TRACJING      A2
#define CENTER_LINE_TRACJING    A1
#define right_LINE_TRACJING     A0

Servo MOTORservo;

const int Forward       = 163;                 // forward
const int Backward      = 92;                  // back
const int Turn_Left     = 106;                 // left translation
const int Turn_Right    = 149;                 // Right translation
const int Top_Left      = 34;                  // Upper left mobile
const int Bottom_Left   = 72;                  // Lower left mobile
const int Top_Right     = 129;                 // Upper right mobile
const int Bottom_Right  = 20;                  // The lower right move
const int Stop          = 0;                   // stop
const int Contrarotate  = 83;                  // Counterclockwise rotation
const int Clockwise     = 172;                 // Rotate clockwise
const int Model1        = 3;                  // model1
const int Model2        = 6;                  // model2
const int Model3        = 7;                  // model3
const int Model4        = 4;                  // model4
const int Model1_Ctrl   = 1;

const int Speed         = 13;
const int Servo         = 2;
const int Motor_Move    = 12;                 // servo turn left
const int LED           = 5;                 // servo turn right

byte dataLen, index_a = 0;
char buffer[52];
unsigned char prevc=0;
bool isStart = false;
bool ED_client = true;
bool WA_en = false;

int Left_Tra_Value;
int Center_Tra_Value;
int Right_Tra_Value;
int Black_Line = 400;
int speeds = 250;
int leftDistance = 0;
int middleDistance = 0;
int rightDistance = 0;
int action = 0;



byte RX_package[17] = {0};
uint16_t angle = 90;
byte Model = Stop;
byte val = 0;
int model_var = 0;
char device = 0;
int UT_distance = 0;
int OA_mark=0;
void setup()
{
  Serial.setTimeout(10);
  Serial.begin(115200);

  MOTORservo.attach(MOTOR_PIN);

  pinMode(SHCP_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);
  pinMode(STCP_PIN, OUTPUT);
  pinMode(PWM1_PIN, OUTPUT);
  pinMode(PWM2_PIN, OUTPUT);

  pinMode(Trig_PIN, OUTPUT);
  pinMode(Echo_PIN, INPUT);

  pinMode(LEFT_LINE_TRACJING, INPUT);
  pinMode(CENTER_LINE_TRACJING, INPUT);
  pinMode(right_LINE_TRACJING, INPUT);

  MOTORservo.write(angle);

  Motor(Stop, 0);
}

unsigned char readBuffer(int index_r)
{
  return buffer[index_r]; 
}

void writeBuffer(int index_w,unsigned char c)
{
  buffer[index_w]=c;
}

void loop()
{
  RXpack_func();
  switch (model_var)
  {
    case 0:
      model1_func(device,val);
      break;
    case 1:
      model2_func();      // OA model
      break;
    case 2:
      model3_func();      // follow model
      break;
    case 3:
      model4_func();      // Tracking model
      break;
  }

}



void model1_func(byte device, byte values) 
{
  switch (device) {
    case Motor_Move:
      switch (values) {
        case 0x00:
          Motor(Stop, 0);
          break;
        case 0x01:
          Motor(Forward, speeds);
          break;
        case 0x02:
          Motor(Backward, speeds);
          break;
        case 0x03:
          Motor(Turn_Left, speeds);
          break;
        case 0x04:
          Motor(Turn_Right, speeds);
          break;
        case 0x05:
          Motor(Top_Left, speeds);
          break;
        case 0x07:
          Motor(Top_Right, speeds);
          break;
        case 0x06:
          Motor(Bottom_Left, speeds);
          break;
        case 0x08:
          Motor(Bottom_Right, speeds);
          break;
        case 0x0A:
          Motor(Clockwise, speeds);
          break;
        case 0x09:
          Motor(Contrarotate, speeds);
          break;
      }
      break;
    case Servo:
      angle=values;
      if (angle >= 180) angle = 180;
      if (angle <= 1) angle = 1;
      MOTORservo.write(angle);
      //delay(5);
      break;
    case LED:
      //Serial.write(RX_package, 17);
      break;
    case Speed:
      speeds=values;
      break;
  }
}

void model2_func()      // OA
{
  while (model_var == 1) {
    RXpack_func();
    middleDistance = SR04(Trig_PIN, Echo_PIN);
    if (middleDistance <= 15)
    {
      Motor(Stop, 0);
      delay(100);
      Motor(Backward, speeds-70);
      delay(100);
      Motor(Clockwise, speeds);
      delay(200);
      Motor(Stop, 0);
      delay(200);
      rightDistance = SR04(Trig_PIN, Echo_PIN);//SR04();
      delay(200);
      if (rightDistance > 15)
      {
        continue;
      }
      else if(OA_mark==0)
      {
        OA_mark++;
        continue;
      }
      Motor(Contrarotate, speeds);
      delay(600);
      Motor(Stop, 0);
      delay(200);
      leftDistance = SR04(Trig_PIN, Echo_PIN);//SR04();
      delay(200);

      if (rightDistance > leftDistance) {
        Motor(Backward, speeds-70);
        delay(500);
        Motor(Clockwise, speeds);
        delay(500);
        OA_mark=0;
        continue;
      }
      else if (rightDistance < leftDistance) {
        while (leftDistance < 25) {
          Motor(Contrarotate, speeds);
          delay(200);
          Motor(Stop, 0);
          delay(200);
          leftDistance = SR04(Trig_PIN, Echo_PIN);//SR04();
          delay(200);
          
        }
        Motor(Contrarotate, speeds);
        delay(100);
        OA_mark=0;
        
        continue;
      }
    }
    else
    {
      Motor(Forward, speeds);
    }
  }
}

void model3_func()      // follow model
{
  MOTORservo.write(90);
  UT_distance = SR04(Trig_PIN, Echo_PIN);
  Serial.println(UT_distance);
  if (UT_distance < 15)
  {
    Motor(Backward, speeds-50);
  }
  else if (15 <= UT_distance && UT_distance <= 20)
  {
    Motor(Stop, 0);
  }
  else if (20 <= UT_distance && UT_distance <= 25)
  {
    Motor(Forward, speeds-70);
  }
  else if (25 <= UT_distance && UT_distance <= 50)
  {
    Motor(Forward, speeds-30);
  }
  else
  {
    Motor(Stop, 0);
  }
}

void model4_func()      // tracking model
{
  MOTORservo.write(90);
  Left_Tra_Value = analogRead(LEFT_LINE_TRACJING);
  Center_Tra_Value = analogRead(CENTER_LINE_TRACJING);
  Right_Tra_Value = analogRead(right_LINE_TRACJING);
  if (Left_Tra_Value > Black_Line && Center_Tra_Value <= Black_Line && Right_Tra_Value > Black_Line) 
  {
    Motor(Forward, 180);
  } 
  else if (Left_Tra_Value < Black_Line && Center_Tra_Value < Black_Line && Right_Tra_Value < Black_Line) 
  {
    Motor(Forward, 130);
  } 
  else if (Left_Tra_Value >= Black_Line && Center_Tra_Value >= Black_Line && Right_Tra_Value < Black_Line) 
  {
    Motor(Clockwise, 220);
  } 
  else if (Left_Tra_Value >= Black_Line && Center_Tra_Value < Black_Line && Right_Tra_Value < Black_Line) 
  {
    Motor(Clockwise, 160);
  } 
  else if (Left_Tra_Value < Black_Line && Center_Tra_Value < Black_Line && Right_Tra_Value >= Black_Line) 
  {
    Motor(Contrarotate, 160);
  } 
  else if (Left_Tra_Value < Black_Line && Center_Tra_Value >= Black_Line && Right_Tra_Value >= Black_Line) 
  {
    Motor(Contrarotate, 220);
  } 
  else if (Left_Tra_Value >= Black_Line && Center_Tra_Value >= Black_Line && Right_Tra_Value >= Black_Line) 
  {
    Motor(Forward, 130);
  }
}



void Motor(int Dir, int Speed)      // motor drive
{
  digitalWrite(EN_PIN, LOW);
  analogWrite(PWM1_PIN, Speed);
  analogWrite(PWM2_PIN, Speed);

  digitalWrite(STCP_PIN, LOW);
  shiftOut(DATA_PIN, SHCP_PIN, MSBFIRST, Dir);
  digitalWrite(STCP_PIN, HIGH);
}

float SR04(int Trig, int Echo)      // ultrasonic measured distance
{
  digitalWrite(Trig, LOW);
  delayMicroseconds(2);
  digitalWrite(Trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(Trig, LOW);
  float distance = pulseIn(Echo, HIGH) / 58.00;
  delay(10);

  return distance;
}

void parseData()
{ 
    isStart = false;
    action = readBuffer(9);
    device = readBuffer(10);
    val = readBuffer(12);
    switch (action)
    {
        case Model1_Ctrl:
            //callOK_Len01();
            model_var = 0;
            break;
        case Model1:
            //callOK_Len01();
            model_var = 0;
            Motor(Stop, 0);
            break;
        case Model2:
            //callOK_Len01();
            model_var = 1;
            break;
        case Model3:
            //callOK_Len01();
            model_var = 2;
            break;
        case Model4:
            //callOK_Len01();
            model_var = 3;
            break;
        default:break;
    }
}

void RXpack_func()  //Receive data
{
    if(Serial.available() > 0)
    {
      unsigned char c = Serial.read()&0xff;
      //Serial.write(c);
      if(c==0x55&&isStart==false)
        {
          if(prevc==0xff)
          {
            index_a=1;
            isStart = true;
          }
        }
        else
        {
          prevc = c;
          if(isStart)
          {
            if(index_a==2)
            {
            dataLen = c; 
            }
            else if(index_a>2)
            {
              dataLen--;
            }
            writeBuffer(index_a,c);
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
}
