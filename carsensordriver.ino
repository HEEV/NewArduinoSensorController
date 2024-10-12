#include <Servo.h>
#include "cppQueue.h"

#define Q_IMPLEMENTATION FIFO  // First In, First Out

unsigned int counter = 0;

//throttle constants
Servo myservo;  //Creates instance of a servo. - DO NOT CHANGE
int pos = 0;    //Initializes variable to store servo position at any given point in time (both values). - DO NOT CHANGE
int potPos;     //Initializes variable to store the potentiometer position. - DO NOT CHANGE

//Airspeed sensor variables
float airOffset;  //Varibale to hold the 0-speed pressure
float airDiff;    //Difference between current air pressure and offset

//wheel speed constants
int wheelSpeedSensorPin = 2;
int numMagnets = 3;
float wheelRadius = 10.0f;  //wheel radius in inches
int debounceTime = 10;      //debounce time in ms
unsigned long magnetTimes[2] = { 0 };
float circumference = 0;     //DO NOT MODIFY!!! calculated from radius
float magRadius = 4.1875;    // inches
float magCircumference = 0;  // inches

float currSpeed = 0;

// Rolling Average Variables
// Initialize teh queue to hold our rolling values
const int Q_SIZE = 12;  // Size of our queue, deciding our rolling avg
int currAvgCount = 7;
cppQueue qRollingVals(sizeof(float), Q_SIZE, Q_IMPLEMENTATION);



struct __attribute__((packed)) DataPacket {
  int sensor;
  float data;
  int64_t time;
};

void setup() {
  // Initialize queue with 5 values
  float baseRec = 0.0f;
  for (int i = 0; i < Q_SIZE; i++) {
    qRollingVals.push(&baseRec);
  }

  Serial.begin(9600);  //Sets frequency value. - DO NOT CHANGE
  myservo.attach(9);   //Attaches the pin 9 input (physical servo) to the servo object. - DO NOT CHANGE
  pinMode(A0, INPUT);  //Attaches the A0 input (physical potentiometer) to the board. - DO NOT CHANGE
  pinMode(wheelSpeedSensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), handleMagnet, FALLING);
  circumference = 2 * wheelRadius * PI;   // inches
  magCircumference = 2 * magRadius * PI;  // inches
  pinMode(A7, INPUT);
  pinMode(12, OUTPUT);
  pinMode(6, INPUT);
  pinMode(A2, INPUT);  //Attach the A2 pin on the arduino to the OUT pin on the airspeed module
  airOffset = 0;       //Assign to 0 for when it resets
  for (int i = 0; i < 50; i++) {
    airOffset += analogRead(A2);  //Average 50 readings to smooth out the data
  }
  airOffset /= 50;  //Divide by 50 to make it the average
}

bool isOn = false;
void loop() {
  currSpeed = getSpeed();
  // firstLen = !firstLen;
  Serial.println(String(batteryLevel()) + ", " + String(currSpeed));  // return global speed instead of calculating here

  // set averaging size
  if (currSpeed > 10.0f) {
    currAvgCount = 10;
  }
  else if (currSpeed > 20.0f) {
    currAvgCount = 12;
  }
  else { 
    currAvgCount = 7;
  }

  delay(50);
  digitalWrite(LED_BUILTIN, isOn ? HIGH : LOW);
  isOn = !isOn;
}

void handleMagnet() {
  unsigned long curTime = millis();
  if (curTime - magnetTimes[0] > debounceTime) {
    magnetTimes[1] = magnetTimes[0];
    magnetTimes[0] = curTime;
  }

  //currSpeed = getSpeed();  // Handle the speed calc on the pulse instead
}

float getSpeed() {
  float mph = 0.0f;
  if (millis() - magnetTimes[0] < 600 && magnetTimes[0] != 0) {
    // Calculating our speed based on the magnet timings
    // TODO: fix this equation for current conditions of the magnets
    /*
   current magnet setup (X is a magnet)
      ***********
     *           * 
     *     X     *
     *     O     *
     *   X   X   *
     *           *
      ***********
    */
    //(((float)circumference / numMagnets) / (magnetTimes[0] - magnetTimes[1])) * 1000.0f
    // (((float)arcLengths[firstLen] / numMagnets) / (magnetTimes[0] - magnetTimes[1])) * 1000.0f
    float inps = (((float)circumference / numMagnets) / (magnetTimes[0] - magnetTimes[1])) * 1000.0f;
    // convert the speed we calculated from Inches/Sec to Miles/Hr
    mph = ((inps / 12.0f) / 5280.0f) * 3600.0f;
  }

  // Push the newest velocity, remove the oldest value
  float rec;
  qRollingVals.pop(&rec);
  qRollingVals.push(&mph);

  // Get our rolling average
  mph = calculateRollingAvg();

  return mph;
}

float calculateRollingAvg() {
  float rec = 0.0f;
  float sum = 0.0f;

  // Get all of our values and sum them
  for (int i = 0; i < currAvgCount; i++) {
    // Index i + (Q_SIZE - CurrAvgCount), moves the index by the offset into the queue we are looking.
    qRollingVals.peekIdx(&rec, i + (Q_SIZE - currAvgCount));
    sum += rec;
  }

  // Return the average of our rolling values
  return sum / currAvgCount;
}


float batteryLevel() {
  float voltageRead = analogRead(7);
  // Vbat = Vread * R2 / (R1 + R2)
  return voltageRead * 680.0f / (1000.0f + 680.0f);
}