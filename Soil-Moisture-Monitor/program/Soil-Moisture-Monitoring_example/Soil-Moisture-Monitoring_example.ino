// Written by Jason Marcum for TinyCircuits
// Initiated: July 2019

#include <Wire.h>
#include <SPI.h>
#include <TinyScreen.h>
#include "TinyAnimation.h"
#include <Wireling.h>

//Library must be passed the board type
//TinyScreenDefault for TinyScreen shields
//TinyScreenAlternate for alternate address TinyScreen shields
//TinyScreenPlus for TinyScreen+
TinyScreen display = TinyScreen(TinyScreenPlus);

// ***Setup objects for displaying on the screen***

//This is an 8 x 5 pixel bitmap using TS library color definitions for raindrop/watering animation
unsigned char RainDrop[8*5]={
  TS_8b_Black, TS_8b_Black, TS_8b_Blue, TS_8b_Black, TS_8b_Black,
  TS_8b_Black, TS_8b_Blue, TS_8b_Blue,  TS_8b_Blue,  TS_8b_Black,
  TS_8b_Black, TS_8b_Blue, TS_8b_Blue,  TS_8b_Blue,  TS_8b_Black,
  TS_8b_Blue,  TS_8b_White, TS_8b_Blue,  TS_8b_Blue,  TS_8b_Blue,
  TS_8b_Blue,  TS_8b_Blue, TS_8b_White,  TS_8b_Blue,  TS_8b_Blue,
  TS_8b_Black, TS_8b_Blue, TS_8b_Blue,  TS_8b_Blue,  TS_8b_Black
};

PercentBarHorizontal bar(display, 2, 2, 94, 14, TS_8b_Gray, TS_8b_Blue, TS_8b_Green, 0, "dry", "wet", true);
IdleCircle idle(display, 75, 47, TS_8b_Gray, TS_8b_White, 9, 1, 3);
FallDownSprite animator(display, 70, 34, 5, 5, 5, 100, RainDrop);


// ***Setup variables for getting readings from the moisture sensor***

// These are close to correct values but calibration will make it more accurate
int offset = 700; // This is the value read off by the Moisture Sensor when completely dry
int range = 264; // The range og values from completely dry (700) to being submerged in water (964)

float moistness = 0.0;  // Variable that keeps raw reading from the sensor

// Used to smooth out readings from moisture sensor 
// (reduces bar jitter due to fast refresh rate/readings)
int moist_readings_counter = 0;
float moist_readings_sum = 0.0;

const int avg_moist_size = 8;
float avg_moistness = 0.0; // Actual avg moisture
float avg_moistness_last = 0.0; // Used to turn on animation

// 0 = no calibration
// 1 = display hold sensor in air
// 2 = take dry readings display hold sensor in water
// 3 = take wet readings end
byte calibration_flag = 0;
bool write_once_cali_flag = false;

bool animation_on = false; // Used to display certain animations at correct times

void setup(void) {
  Wire.begin(); //initialize I2C before we can initialize TinyScreen- not needed for TinyScreen+
  display.begin();
  SerialUSB.begin(9600);
  
  display.setBrightness(10);

  // Select the port the Wireling (moisture sensor) is connected to on the Adapter board
  Wireling.begin();
  Wireling.selectPort(0);
  delay(100);

  // Take sensor reading to get an initial value for displaying the moisture bar
  moistness = readMoisture();
  moistness = (moistness - offset) / range;

  // Apply the letters to the screen once so we don't have to every tick
  bar.updateBarEndInfo();

  // Show a message about what the plant is doing
  ShowIdleMessage();
}

void loop() {
  // Only show monitoring information if not currently in calibration mode
  if(calibration_flag == 0){
    // animation is on? Make raindrops!
    if(animation_on){
      // If got to the last frame then stop animation next time
      if(animator.tick()){
        animation_on = false;
        
        // Show a message about what the plant is doing
        ShowIdleMessage();
      }
    }
  
    // If no animation/not being watered show idle circle
    if(!animation_on){
      idle.tick();
    }
    
    // Get a moisture value
    moistness = readMoisture();
  
    // Offset the reading to put in range of 0 to 100
    moistness = (moistness - offset) / range;
  
    // Handle getting an average and updating the percent bar
    HandleMostureReading(moistness);
  
    // Press button by power switch to start sensor cailbration or enter c
    if(display.getButtons(TSButtonUpperRight) || (SerialUSB.available() > 0 && SerialUSB.read() == 'c')){
      SerialUSB.println("\nCalibration started!\n");
      calibration_flag++;
      write_once_cali_flag = false;
      
      // Delay so last button press doesn't register right away
      delay(800);
    }
  }

  // Handle the calibration steps
  if(calibration_flag >= 1){
    if(display.getButtons(TSButtonUpperRight|TSButtonUpperLeft|TSButtonLowerRight|TSButtonLowerLeft) || (SerialUSB.available() > 0 && SerialUSB.read() == 'n')){
      calibration_flag++; // Move on to next stwp of calibration
      write_once_cali_flag = false;
      
      // Delay so last button press doesn't register right away
      delay(800);
    }

    // Have to use flags since infinite loops don't allow new input from serial
    CalibrateMoisutreSensor();
  }
}

// Handles averaging and updating percent bar
void HandleMostureReading(float offset_moisture_reading){
  // Sum for avg's numerator
  moist_readings_sum += offset_moisture_reading;

  // Increment the tracker/counter
  moist_readings_counter += 1;

  // Calculate the average when the counter reaches the size
  if(moist_readings_counter == avg_moist_size){
    avg_moistness = moist_readings_sum/avg_moist_size;

    // Reset these for next average/reading
    moist_readings_counter = 0;
    moist_readings_sum = 0.0;
  }

  // Animate/update text for the percent bar
  bar.tick(avg_moistness);

  // Display plant status to the serial monitor
  if(!animation_on){
    SerialUSB.print("Plant is -> IDLE | Moisture level = ");
  }else{
    SerialUSB.print("Plant is -> HYDRATING | Moisture level = ");
  }

  // Display the moisture level to the serial monitor
  SerialUSB.println(avg_moistness);

  // Looks like a big jump, turn on animation and clear idle animation
  if(avg_moistness - avg_moistness_last > 0.1){
    animation_on = true;

    // Show a message about what the plant is doing
    ShowHydratingMessage();
    
    idle.erase();
  }

  avg_moistness_last = avg_moistness;
}

void ShowHydratingMessage(){
  display.setFont(thinPixel7_10ptFontInfo);
  display.fontColor(TS_8b_Green, TS_8b_Black);
  display.setCursor(0, 40);
  display.println("Plant is:");
  display.setCursor(0, 50);
  display.println("->");
  display.fontColor(TS_8b_Blue, TS_8b_Black);
  display.setCursor(11, 50);
  display.println("Hydrating");
}

void ShowIdleMessage(){
  display.setFont(thinPixel7_10ptFontInfo);
  display.fontColor(TS_8b_Green, TS_8b_Black);
  display.setCursor(0, 40);
  display.println("Plant is:");
  display.setCursor(0, 50);
  display.println("->");
  display.fontColor(TS_8b_Gray, TS_8b_Black);
  display.setCursor(11, 50);
  display.println("Idle     ");
}

// Used to calibrate the moisture sensor
void CalibrateMoisutreSensor(){

  // Only write stuff to the screen once, avoids flicker
  if(write_once_cali_flag)
    return;

  // Handle each flag for the calibration process
  switch(calibration_flag){
    case 1:
      display.clearScreen();
      display.setFont(thinPixel7_10ptFontInfo);
      display.fontColor(TS_8b_Green,TS_8b_Black);
      display.setCursor(0, 0);
      
      display.println("Hold sensor in the");
      display.setCursor(0, 10);
      display.fontColor(0x90, TS_8b_Black);
      display.println("AIR");
      display.fontColor(TS_8b_Green, TS_8b_Black);
      display.setCursor(22, 10);
      display.println("and press any");
      display.setCursor(0, 20);
      display.println("button");
      display.setCursor(0, 30);
      display.fontColor(TS_8b_Red, TS_8b_Black);
      display.println("(hold by wire only!)");
      display.fontColor(TS_8b_Green, TS_8b_Black);
      SerialUSB.println("Hold the sensor in the AIR and press any button or enter 'n' (hold by the wire only!)");
      write_once_cali_flag = true;
    break;
    case 2:
    
      display.clearScreen();
      display.setCursor(0, 0);
      display.println("Keep holding");
      SerialUSB.println("Keep holding the sensor");
      display.setCursor(0, 10);
      display.println("Taking readings...");
      SerialUSB.println("Taking readings...");
    
      // Take the dry readings
      moist_readings_sum = 0;
    
      for(int i=0; i<100; i++){
        
        moist_readings_sum += readMoisture();
    
        // Space readings apart
        delay(25);
      }
    
      // Dry readings are the offset (use average of 5 readings)
      offset = moist_readings_sum/100;
    
      display.clearScreen();
      display.setCursor(0, 0);
      display.println("Dry readings done");
      SerialUSB.println("Dry readings done");
      display.setCursor(0, 20);
      display.println("offset = " + String(offset));
      SerialUSB.println("offset = " + String(offset));
      delay(2000);
      write_once_cali_flag = true;
    
      display.setCursor(0, 0);
      display.clearScreen();
      display.println("Submerge sensor in");
      display.setCursor(0, 10);
      display.fontColor(TS_8b_Blue, TS_8b_Black);
      display.println("WATER");
      display.fontColor(TS_8b_Green, TS_8b_Black);
      display.setCursor(31, 10);
      display.println("and press any");
      display.setCursor(0, 20);
      display.println("button");
      SerialUSB.println("Submerge the sensor in WATER and press any button or enter 'n'");
      write_once_cali_flag = true;
    break;
    case 3:
      display.clearScreen();
      display.setCursor(0, 0);
      display.println("Keep holding");
      SerialUSB.println("Keep holding the sensor");
      display.setCursor(0, 10);
      display.println("Taking readings...");
      SerialUSB.println("Taking readings...");
    
      // Take the submerged readings
      moist_readings_sum = 0;
    
      for(int i=0; i<100; i++){
        
        moist_readings_sum += readMoisture();
    
        // Space readings apart
        delay(25);
      }
    
      range = (moist_readings_sum/100) - offset;
    
      // Range wasn't big enough, error!
      if(range <= 0){
        display.clearScreen();
        display.setCursor(0, 0);
        display.fontColor(TS_8b_Red, TS_8b_Black);
        display.println("Range not big");
        display.setCursor(0, 10);
        display.println("enough!");
        display.setCursor(0, 30);
        display.fontColor(TS_8b_Green, TS_8b_Black);
        display.println("Set to defaults...");
        SerialUSB.println("The range is not big enough! Setting offset and range back to defaults");
    
        // Give an extra second to read error
        delay(1000);
    
        offset = 0;
        range = 100;
      }else{
        display.clearScreen();
        display.setCursor(0, 0);
        display.println("Wet readings done");
        SerialUSB.println("Wet readings done");
        display.setCursor(0, 20);
        display.println("range = " + String(range));
        SerialUSB.println("range = " + String(range));
      }
    
      // Delay so last button press doesn't register right away and to give time to read
      delay(2000);
      display.clearScreen();
      delay(100);
      moist_readings_sum = 0;
      moist_readings_counter = 0;
      bar.updateBarEndInfo();
    
      // Show a message about what the plant is doing
      ShowIdleMessage();

      calibration_flag = 0;
    break;
  }
}

// Read moisture from Soil Moisture Wireling
//int readMoisture(){
//  Wire.beginTransmission(0x23);
//  Wire.write(1);
//  Wire.endTransmission();
//  delay(5);
//  int c=0;
//  Wire.requestFrom(0x23, 2);
//  if(Wire.available()==2)
//  { 
//    c = Wire.read();
//    c<<=8;
//    c|=Wire.read();
//  }
//  return c;
//}

/* * * * * * MOISTURE SENSOR * * * * * * */
#define MOISTURE_PORT 0 
#define MIN_CAP_READ 710 /* Toggle this to raw minimum value */
#define MAX_CAP_READ 975 /* Toggle this to raw maximum value */

#define ANALOG_READ_MAX 1023
#define THERMISTOR_NOMINAL 10000
#define TEMPERATURE_NOMINAL 25
#define B_COEFFICIENT 3380
#define SERIES_RESISTOR 35000

int readMoisture(){
  Wire.beginTransmission(0x30);
  Wire.write(1);
  Wire.endTransmission();
  delay(5);
  int c=0;
  Wire.requestFrom(0x30, 2);
  if(Wire.available()==2)
  { 
    c = Wire.read();
    c <<= 8;
    c |= Wire.read();
    c = constrain(c, MIN_CAP_READ, MAX_CAP_READ);
    c = map(c, MIN_CAP_READ, MAX_CAP_READ, 0, 100);
  }
  return c;
}

float readTemp() {
  Wire.beginTransmission(0x30);
  Wire.write(2);
  Wire.endTransmission();
  delay(5);
  int c = 0;
  Wire.requestFrom(0x30, 2);
  if (Wire.available() == 2)
  {
    c = Wire.read();
    c <<= 8;
    c |= Wire.read();
    //https://learn.adafruit.com/thermistor/using-a-thermistor thanks!
    uint32_t adcVal = ANALOG_READ_MAX - c;
    uint32_t resistance = (SERIES_RESISTOR * ANALOG_READ_MAX) / adcVal - SERIES_RESISTOR;
    float steinhart = (float)resistance / THERMISTOR_NOMINAL;     // (R/Ro)
    steinhart = log(steinhart);                  // ln(R/Ro)
    steinhart /= B_COEFFICIENT;                   // 1/B * ln(R/Ro)
    steinhart += 1.0 / (TEMPERATURE_NOMINAL + 273.15); // + (1/To)
    steinhart = 1.0 / steinhart;                 // Invert
    steinhart -= 273.15;                         // convert to C
    return steinhart;
  }
  return c;
}
