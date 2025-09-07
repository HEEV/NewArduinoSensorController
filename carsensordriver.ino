#include <Servo.h>
#include <DS18B20.h>


//wheel speed constants
#define wheelSpeedSensorPin 2
#define numMagnets 1
#define debounceTime 10 // in ms
#define wheelRadius 10.0 // in in
#define circumference (2 * wheelRadius * PI) // in in 
#define pulseDist (circumference / numMagnets) 

unsigned int counter = 0;

//throttle constants Not in use at the moment.
/*
Servo myservo;  //Creates instance of a servo. - DO NOT CHANGE
int pos = 0;    //Initializes variable to store servo position at any given point in time (both values). - DO NOT CHANGE
int potPos;     //Initializes variable to store the potentiometer position. - DO NOT CHANGE
*/

//Airspeed sensor variables
float airOffset = 0.0f;  //Varibale to hold the 0-speed pressure
//float airDiff;    //Difference between current air pressure and offset


// other wheelspeed variables
volatile unsigned long magnetTimes[2] = { 0 }; // volatile modifier due to write in interrupt
volatile unsigned long deltaTime = 0;
volatile unsigned long curTime = 0;
volatile float distTraveled = 0;
float currSpeed = 0;
float prevSpeed = 0;

// Temperature Sensor variables
DS18B20 ds(3);
const uint8_t engineTempAddr[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const uint8_t radTempAddr[8]    = { 0x28, 0xD0, 0xEB, 0x87, 0x00, 0xCA, 0x26, 0x82 };

// Index 0 is engine temp, index 1 is rad temp
const int cacheTTL[] = {50, 50};
int cacheLife[] = {0, 0};

// Honestly not sure wth this is

// struct __attribute__((packed)) DataPacket {
//   int sensor;
//   float data;
//   int64_t time;
// };

void setup() {
  Serial.begin(9600);  //Sets frequency value. - DO NOT CHANGE
  // not in use at the moment
  /*
  //myservo.attach(9);   //Attaches the pin 9 input (physical servo) to the servo object. - DO NOT CHANGE 
  pinMode(A0, INPUT);  //Attaches the A0 input (physical potentiometer) to the board. - DO NOT CHANGE
  */
  pinMode(wheelSpeedSensorPin, INPUT_PULLUP); // Wheelspeed, pin 2
  attachInterrupt(digitalPinToInterrupt(2), handleMagnet, FALLING); // wheelspeed interrupt
  pinMode(12, OUTPUT);
  pinMode(6, INPUT); 
  pinMode(4, INPUT_PULLUP);   // user input 1
  pinMode(5, INPUT_PULLUP);   // user input 2
  pinMode(7, INPUT);   // Karch Selection Pin
  pinMode(8, INPUT);   // Sting Selection Pin
  pinMode(10, OUTPUT); // Rad Fan Signal Out
  pinMode(11, OUTPUT); // Water Pump Signal Out
  pinMode(A7, INPUT);  // Battery Voltage
  pinMode(A2, INPUT);  //Attach the A2 pin on the arduino to the OUT pin on the airspeed module
  for (int i = 0; i < 50; i++) {
    airOffset += analogRead(A2);  //Average 50 readings to smooth out the data
  }
  airOffset /= 50;  //Divide by 50 to make it the average
}

//bool isOn = false;
void loop() {
  // Update speed values
  prevSpeed = currSpeed;
  currSpeed = getSpeed();

  // Update temperature cache values
  float engTemp = updateEngineTemp();
  float radTemp = updateRadiatorTemp();

  if ((currSpeed != prevSpeed && currSpeed > 0.25) || currSpeed == 0 ) {// 0.25 mph is error bandwidth for noise 
    Serial.println(String(batteryLevel()) + ", " + String(currSpeed) + ", " + String(distTraveled) + ", " + String(1) + ", " 
    + String(pollUserInput(1)) + ", " + String(pollUserInput(2)) + ", " + String(engTemp) + ", " + String(radTemp));
  }

  delay(50);
  // comment this out on deployment
  /*
  digitalWrite(LED_BUILTIN, isOn ? HIGH : LOW);
  isOn = !isOn;
  /**/
}

void handleMagnet() {
  curTime = millis();
  if (curTime - magnetTimes[0] > debounceTime) {
    magnetTimes[1] = magnetTimes[0];
    magnetTimes[0] = curTime;

    // use the retreived magnett timings to get delta t on the pulse
    deltaTime = magnetTimes[0] - magnetTimes[1];

    // use the known pulse distance travel to find total distance in feet
    distTraveled += pulseDist / 12;
  }
}

float getSpeed() {
  // Disable interrupts on reads of shared variables for atomicity
  // noInterrupts();

  if (millis() - magnetTimes[0] < 3800 && magnetTimes[0] != 0) {
    // Calculating our speed based on the magnet timings

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

    // Calculate speed in inches per second
    float inps = ((circumference / numMagnets) / deltaTime) * 1000.0f;

    // Done accessing shared variables, re-enable interrupts
    // interrupts();

    // convert the speed we calculated from Inches/Sec to Miles/Hr
    return ((inps / 12.0f) / 5280.0f) * 3600.0f;
  }

  return 0.0;
}


float batteryLevel() {
  float voltageRead = analogRead(7);
  // Vbat = Vread * R2 / (R1 + R2)
  return voltageRead * 680.0f / (1000.0f + 680.0f);
}

float updateEngineTemp() {
  if (ds.select(engineTempAddr)){
    if (cacheLife[0] > cacheTTL[0]) {
      cacheLife[0] = 0;
      return ds.getTempF();
    } else { 
      cacheLife[0]++; 
    }
  }
}

float updateRadiatorTemp() {
  if (ds.select(radTempAddr)){
    if (cacheLife[1] > cacheTTL[1]) {
      cacheLife[1] = 0;
      return ds.getTempF();
    }
    else { 
      cacheLife[1]++; 
    }
  }
}

bool pollUserInput(int index) {
  bool data;
  switch (index) {
    case 1:
      data = !(bool)digitalRead(4);
      break;
    case 2:
      data = !(bool)digitalRead(5);
      break;
    default:
      data = false;
  }

  return data;
}

int getCarId() {
  int karch = digitalRead(7);
  int sting = digitalRead(8);

  if (karch == 1) {
    return 1;
  }
  else if (sting == 1) {
    return 2;
  }
  else {
    return 1;
  }
}
