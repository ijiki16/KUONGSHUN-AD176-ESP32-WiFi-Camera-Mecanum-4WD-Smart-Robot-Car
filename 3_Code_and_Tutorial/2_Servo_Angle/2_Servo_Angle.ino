//www.kuongshun.com

#include    <Servo.h>
#define servo_PIN    9
Servo myservo;
void setup()
{
    myservo.attach(servo_PIN);
    myservo.write(90);
    delay(1000);
}

void loop()
{
  for(int i=0;i<180;i++){
    myservo.write(i);
    delay(10);
  }
  delay(1000);
  for(int i=180;i>0;i--){
    myservo.write(i);
    delay(10);
  }
  delay(1000);
}
