#include <DS18B20.h>

//wheel speed constants
#define wheelSpeedSensorPin 2
#define numMagnets 1
#define debounceTime 10 // in ms
#define wheelRadius 10.0 // in in
#define circumference (2 * wheelRadius * PI) // in in 
#define pulseDist (circumference / numMagnets) 

// other wheelspeed variables
volatile unsigned long magnetTimes[2] = { 0 }; // volatile modifier due to write in interrupt
volatile unsigned long deltaTime = 0;
volatile unsigned long curTime = 0;
volatile float distTraveled = 0;
float currSpeed = 0;
float prevSpeed = 0;

// Airspeed sensor variables
float airOffset = 0.0f;  // Variable to hold the 0-speed pressure
//float airDiff;    // Difference between current air pressure and offset

// Temperature Sensor variables
DS18B20 ds(3);
const uint8_t engineTempAddr[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const uint8_t radTempAddr[8]    = { 0x28, 0xD0, 0xEB, 0x87, 0x00, 0xCA, 0x26, 0x82 };

// Index 0 is engine temp, index 1 is rad temp
const int cacheTTL[] = {50, 50};
int cacheLife[] = {0, 0};

// This is a struct with all padding bytes removed, which is used for efficient sending of data over the serial bus.
// unsigned char = 1 byte
// unsigned int  = 2 bytes
// float         = 4 bytes

struct __attribute__((packed)) DataPacket {
  // Hardcoded sensors/values (common across vehicles)
  float         speed;
  float         distaceTraveled;
  float         airOffset;
  float         engineTemp;
  float         radTemp;
  // Digital Channels
  unsigned char channel0;
  unsigned char channel1;
  unsigned char channel2;
  unsigned char channel3;
  unsigned char channel4;
  // Analog Channels
  unsigned int  channelA0; // 10-bit ADC
};

void setup() {
  Serial.begin(9600);  //Sets frequency value. - DO NOT CHANGE

  pinMode(wheelSpeedSensorPin, INPUT_PULLUP); // Wheelspeed, pin 2
  attachInterrupt(digitalPinToInterrupt(2), handleMagnet, FALLING); // wheelspeed interrupt
  pinMode(4, INPUT_PULLUP);   // user input 1
  pinMode(5, INPUT_PULLUP);   // user input 2
  pinMode(6, INPUT);
  pinMode(7, INPUT);
  pinMode(8, INPUT);
  pinMode(10, OUTPUT); // Rad Fan Signal Out
  pinMode(11, OUTPUT); // Water Pump Signal Out
  pinMode(12, OUTPUT);
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

  struct DataPacket packet = {
    currSpeed, distTraveled, ((float)analogRead(2) - airOffset), engTemp, radTemp, 
    (unsigned char)digitalRead(4), (unsigned char)digitalRead(5), (unsigned char)digitalRead(6), 
    (unsigned char)digitalRead(7), (unsigned char)digitalRead(8), analogRead(7)
  };

  // Output all data channels to the bus, whether they are connected or not. Parsing happens RPi side.
  // The write call casts the packet to a pointer to a byte, which write then parses through.
  if ((currSpeed != prevSpeed && currSpeed > 0.25) || currSpeed == 0 ) {// 0.25 mph is error bandwidth for noise 
    Serial.write((uint8_t*)&packet, sizeof(packet));
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

    // use the retrieved magnet timings to get delta t on the pulse
    deltaTime = magnetTimes[0] - magnetTimes[1];

    // use the known pulse distance traveled to find total distance in feet
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
     *     X     * 
     *           *
     *     O     *
     *           *
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

// Get Temperatures, but only every so often because these sensors are slow.

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
