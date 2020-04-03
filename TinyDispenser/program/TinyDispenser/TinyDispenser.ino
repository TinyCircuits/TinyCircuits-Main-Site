#include <Wire.h>
#include "VL53L0X.h"    // For interfacing with the Time-of-Flight Distance sensor
#include <ServoDriver.h>

// ** SERVO STUFF **

ServoDriver servo(NO_R_REMOVED);//this value affects the I2C address, which can be changed by
                                //removing resistors R1-R3. Then the corresponding R1_REMOVED,
                                //R2_REMOVED, R1_R2_REMOVED, R1_R4_REMOVED and so on can be set.
                                //Default is NO_R_REMOVED

// Flag used to check if door is in closed position
bool setClosed = false;


// ** BUTTON STUFF **

#define buttonPin A1 // corresponds to Port 0. Similarly, Port 1 = A1, Port 2 = A2, Port 3 = A3.

// Used to track the state of the momentary button
int buttonState = 0;
int lastButtonState = buttonState;


// ** TOF DISTANCE SENSOR STUFF **

VL53L0X distanceSensor; // Name of sensor 
const int tofPort = 0;  // Port # of sensor (Found on Whisker Adapter Board)

const int averageCount = 1;
int average[averageCount];
int averagePos = 0;



#if defined (ARDUINO_ARCH_AVR)
#define SerialMonitorInterface Serial
#elif defined(ARDUINO_ARCH_SAMD)
#define SerialMonitorInterface SerialUSB
#endif


void setup() {
  // Begin Serial Communication and set Baud Rate
  SerialMonitorInterface.begin(115200);
  Wire.begin();
  //while(!SerialMonitorInterface)//This will block until the Serial Monitor is opened on TinyScreen+/TinyZero platform!

  // Use this pin as an input to read the button state
  pinMode(buttonPin, INPUT);

  //Set the period to 20000us or 20ms, correct for driving most servos
  if(servo.begin(20000)){
    SerialUSB.println("Motor/Servo driver not detected!");
  }else{

    delay(250);

    //The failsafe turns off the PWM output if a command is not sent in a certain amount of time.
    //Failsafe is set in milliseconds- comment or set to 0 to disable
    servo.setFailsafe(0);
  }

  // Select the port of the distance sensor (this number corresponds 
  // with port #'s on the Whisker adapter board)
  selectPort(tofPort);

  // Initialize the distance sensor and set a timeout
  distanceSensor.init();
  distanceSensor.setTimeout(500);

  // Start continuous back-to-back mode (take readings as
  // fast as possible).  To use continuous timed mode
  // instead, provide a desired inter-measurement period in
  // ms (e.g. sensor.startContinuous(100))
  distanceSensor.setMeasurementTimingBudget(200000);
  distanceSensor.startContinuous();

  delay(50);

  SerialMonitorInterface.println("Setup done!");
}


void loop() {

  // Grab the current button state for checking after getting the next button state in a few lines
  lastButtonState = buttonState;

  // Get the button state
  buttonState = digitalRead(buttonPin);

  // When the last button state is not the same as the just grabbed state, dispense food/increase servo angle
  if (lastButtonState != buttonState){
    if (buttonState == LOW){
      SerialMonitorInterface.println("Button pressed! Give food!");
      servo.setServo(1, 900);
      servo.setServo(2, 900);
      servo.setServo(3, 900);
      servo.setServo(4, 900);
      delay(400);
      servo.setServo(1, 1700);
      servo.setServo(2, 1700);
      servo.setServo(3, 1700);
      servo.setServo(4, 1700);
    }
  }

  unsigned long dist = getTOFReading();


  // Distance > 300mm will mean door is closed, but 100mm < x < 300mm is open will 100 being all the way
  // When configured correctly, 1000 in setServo is closed and 1800 is open
  // TESTING: Closed is 1100 and open is 1800
  if(dist >= 100 && dist <= 300){
    // Map 'to' range in reverse for servo orientation
    float servoValue = map(dist, 100, 300, 900, 1700);
    servo.setServo(1, servoValue);
    servo.setServo(2, servoValue);
    servo.setServo(3, servoValue);
    servo.setServo(4, servoValue);
    setClosed = false;
  }else if(!setClosed && dist > 300){
    servo.setServo(1, 1700);
    servo.setServo(2, 1700);
    servo.setServo(3, 1700);
    servo.setServo(4, 1700);
    setClosed = true;
  }
}


unsigned long getTOFReading(){
  // Calculate the average position of the distance sensor
  unsigned long averageRead = 0;
  average[averagePos] = distanceSensor.readRangeContinuousMillimeters();
  averagePos++;
  if (averagePos >= averageCount)averagePos = 0;
  for (int i = 0; i < averageCount; i++) {
    averageRead += (unsigned long)average[i];
  }
  averageRead /= (unsigned long)averageCount;

  return averageRead;
}


// **This function is necessary for all Whisker boards attached through an Adapter board**
// Selects the correct address of the port being used in the Adapter board
void selectPort(int port) {
  Wire.beginTransmission(0x70);
  Wire.write(0x04 + port);
  Wire.endTransmission(0x70);
}
