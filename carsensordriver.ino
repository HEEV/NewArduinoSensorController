#include <Servo.h>

unsigned int counter = 0;

//throttle constants
Servo myservo;  //Creates instance of a servo. - DO NOT CHANGE
int pos = 0;    //Initializes variable to store servo position at any given point in time (both values). - DO NOT CHANGE
int potPos;     //Initializes variable to store the potentiometer position. - DO NOT CHANGE

//Airspeed sensor variables
float airOffset; //Varibale to hold the 0-speed pressure
float airDiff;   //Difference between current air pressure and offset

//wheel speed constants
int wheelSpeedSensorPin = 2;
int numMagnets = 2;
float wheelRadius = 10.0f; //wheel radius in inches
int debounceTime = 10; //debounce time in ms
unsigned long magnetTimes[2] = { 0 };
float circumference = 0; //DO NOT MODIFY!!! calculated from radius

struct __attribute__ ((packed)) DataPacket
{
  int sensor;
  float data;
  int64_t time;
};

void setup() {
  Serial.begin(9600); //Sets frequency value. - DO NOT CHANGE
  myservo.attach(9);    //Attaches the pin 9 input (physical servo) to the servo object. - DO NOT CHANGE
  pinMode(A0, INPUT);   //Attaches the A0 input (physical potentiometer) to the board. - DO NOT CHANGE
  pinMode(wheelSpeedSensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), handleMagnet, FALLING);
  circumference = 2 * wheelRadius * PI;
  pinMode(A7, INPUT);
  pinMode(12, OUTPUT);
  pinMode(6, INPUT);
  pinMode(A2, INPUT);   //Attach the A2 pin on the arduino to the OUT pin on the airspeed module
  airOffset = 0;        //Assign to 0 for when it resets
  for (int i = 0; i < 50; i++) {  
     airOffset += analogRead(A2); //Average 50 readings to smooth out the data
  }
  airOffset /= 50;  //Divide by 50 to make it the average
}

bool isOn = false;
void loop() {
  Serial.println(String(batteryLevel()) + ", " + String(getSpeed()));
  delay(50);
  digitalWrite(LED_BUILTIN, isOn ? HIGH : LOW);
  isOn = !isOn;
}

void handleMagnet (){
  unsigned long curTime = millis();
  if(curTime - magnetTimes[0] > debounceTime)
  {
    magnetTimes[1] = magnetTimes[0];
    magnetTimes[0] = curTime;
  }
}

float getSpeed()
{
  float mph = 0.0f;
  if(millis() - magnetTimes[0] < 400 && magnetTimes[0] != 0)
  {
    float inps = (((float)circumference / numMagnets) / (magnetTimes[0] - magnetTimes[1])) * 1000.0f;
    mph = ((inps / 12.0f) / 5280.0f) * 3600.0f;
  }
  return mph;
}

float batteryLevel() {
  float voltageRead = analogRead(7);
  // Vbat = Vread * R2 / (R1 + R2)
  return voltageRead * 680.0f / (1000.0f + 680.0f);
}