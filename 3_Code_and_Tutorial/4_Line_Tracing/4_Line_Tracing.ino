//www.kuongshun.com

// PWM control pin
#define PWM1_PIN            5
#define PWM2_PIN            6      
// 74HCT595N chip pin
#define SHCP_PIN            2                               // The displacement of the clock
#define EN_PIN              7                               // Can make control
#define DATA_PIN            8                               // Serial data
#define STCP_PIN            4                               // Memory register clock          

#define LEFT_LINE_TRACJING          A0
#define CENTER_LINE_TRACJING        A1
#define right_LINE_TRACJING         A2

const int Forward       = 163;                               // forward
const int Backward      = 92;                              // back
const int Turn_Left     = 106;                              // left translation
const int Turn_Right    = 149;                              // Right translation 
const int Top_Left      = 34;                               // Upper left mobile
const int Bottom_Left   = 72;                              // Lower left mobile
const int Top_Right     = 129;                               // Upper right mobile
const int Bottom_Right  = 20;                               // The lower right move
const int Stop          = 0;                                // stop
const int Contrarotate  = 83;                              // Counterclockwise rotation
const int Clockwise     = 172;                               // Rotate clockwise

int Left_Tra_Value;
int Center_Tra_Value;
int Right_Tra_Value;
int Black_Line = 500;//small is black


void setup()
{
    pinMode(SHCP_PIN, OUTPUT);
    pinMode(EN_PIN, OUTPUT);
    pinMode(DATA_PIN, OUTPUT);
    pinMode(STCP_PIN, OUTPUT);
    pinMode(PWM1_PIN, OUTPUT);
    pinMode(PWM2_PIN, OUTPUT);

    pinMode(LEFT_LINE_TRACJING, INPUT);
    pinMode(CENTER_LINE_TRACJING, INPUT);
    pinMode(right_LINE_TRACJING, INPUT);
    Serial.begin(9600);
}

void loop()
{
    Left_Tra_Value = analogRead(LEFT_LINE_TRACJING);
    Center_Tra_Value = analogRead(CENTER_LINE_TRACJING);
    Right_Tra_Value = analogRead(right_LINE_TRACJING);    
    Serial.println("l,c ,r:");
    Serial.println(analogRead(LEFT_LINE_TRACJING));
    Serial.println(analogRead(CENTER_LINE_TRACJING));
    Serial.println(analogRead(right_LINE_TRACJING));
    if (Left_Tra_Value > Black_Line && Center_Tra_Value <= Black_Line && Right_Tra_Value > Black_Line)
    {
        Motor(Forward, 180);
    }
    else if (Left_Tra_Value < Black_Line && Center_Tra_Value < Black_Line && Right_Tra_Value < Black_Line)
    {
        Motor(Forward, 120);
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
        Motor(Forward, 120);
    }
}

void Motor(int Dir, int Speed)
{
    digitalWrite(EN_PIN, LOW);
    analogWrite(PWM1_PIN, Speed);
    analogWrite(PWM2_PIN, Speed);

    digitalWrite(STCP_PIN, LOW);
    shiftOut(DATA_PIN, SHCP_PIN, MSBFIRST, Dir);
    digitalWrite(STCP_PIN, HIGH);
}
