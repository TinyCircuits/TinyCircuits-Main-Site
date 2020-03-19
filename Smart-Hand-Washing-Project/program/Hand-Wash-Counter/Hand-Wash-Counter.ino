/************************************************************************
 * Smart Hand Washing
 * This program uses a moisture sensor, buzzer, and screen to keep track 
 * of how long you are washing hands, and alarming you if you have not 
 * washed your hands for at least 20 seconds.
 * 
 * Hardware by: TinyCircuits
 * Written by: Laveréna Wienclaw for TinyCircuits
 *
 * Initiated: Mar 2020 
 * Updated: Mar 2020
 ************************************************************************/

#include <Wire.h>
#include <SPI.h>
#include <Wireling.h>
#include <TinierScreen.h>  
#include <GraphicsBuffer.h> 

// Make compatible with all TinyCircuits processors
#if defined(ARDUINO_ARCH_AVR)
#define SerialMonitorInterface Serial
#elif defined(ARDUINO_ARCH_SAMD)
#define SerialMonitorInterface SerialUSB
#endif

/* * * * * * MOISTURE SENSOR * * * * * * */
#define MOISTURE_PORT 0 
#define MIN_CAP_READ 710 /* Toggle this to raw minimum value */
#define MAX_CAP_READ 975 /* Toggle this to raw maximum value */

#define ANALOG_READ_MAX 1023
#define THERMISTOR_NOMINAL 10000
#define TEMPERATURE_NOMINAL 25
#define B_COEFFICIENT 3380
#define SERIES_RESISTOR 35000
int moisture;

/* * * * * * * * * * 0.69" OLED * * * * * * * * * */
int DISPLAY_PORT = 3;
int RESET_PIN = A0+DISPLAY_PORT;
TinierScreen display = TinierScreen(TinierScreen069);
GraphicsBuffer screenBuffer = GraphicsBuffer(96, 16, colorDepth1BPP);
int xMax, yMax, x, y;

/* * * * * * * * * * BUZZER * * * * * * * * * */
int BUZZER_PIN = A2;

void setup() {
  Wire.begin();
  Wireling.begin();
  delay(10);

  /* * * * * * * * * Moisture Sensor init * * * * * * * */
  Wireling.selectPort(MOISTURE_PORT);
  SerialMonitorInterface.begin(9600);

  /* * * * * * * * * 0.69" Screen init * * * * * * * */
  Wireling.selectPort(DISPLAY_PORT);
  display.begin(RESET_PIN);
  if (screenBuffer.begin()) {
    //memory allocation error- buffer too big!
  }
  screenBuffer.setFont(thinPixel7_10ptFontInfo);

  /* * * * * * * * * Buzzer init * * * * * * * */
  pinMode(BUZZER_PIN, OUTPUT);
  
}

// Hand washing thresholds
int WASH_TIME = 5000; // Wash hands for at least this amount of time (At least 20 seconds)
int FAUCET_ON = 50;   // Minmum moisture reading when washing hands
int FAUCET_OFF = 20;  // Max moisture reading when faucet is off and not washing hands

// Timing variables
int startWash = 0;
int currentTime;
int currentWashTime;
int increment = 0;

// Alarm/buzzer variables
int buzzerBegin;
int buzzerCurrent;
bool stopFlag = false;
int stoppedWashing;
int currentStoppedWashing = 0;

void loop() {
  Wireling.selectPort(MOISTURE_PORT);
  SerialMonitorInterface.print("M: ");
  moisture = readMoisture();
  SerialMonitorInterface.println(readMoisture());

  if(startWash == 0) // Faucet off 
  {
    Wireling.selectPort(DISPLAY_PORT);
    screenBuffer.clear(); // keep screen off while hands aren't being washed
    display.writeBuffer(screenBuffer.getBuffer(), screenBuffer.getBufferSize());

    if (moisture > FAUCET_ON){
      startWash = millis(); // Faucet has turned on
    }
  }
  else{ // Faucet on
    currentTime = millis();
    currentWashTime = currentTime - startWash - currentStoppedWashing;
    SerialUSB.print("current wash time:"); SerialUSB.println(currentWashTime);
    
    if(WASH_TIME > currentWashTime){ // Haven't been washing hands for long enough
      Wireling.selectPort(DISPLAY_PORT);
      washingMsg(); 

      Wireling.selectPort(MOISTURE_PORT);
      moisture = readMoisture(); 
      
      if(moisture < FAUCET_OFF) // Stopped washing hands before time's up - trigger alarm!
      {
        if (!stopFlag){
          stoppedWashing = millis();
          stopFlag = true;
        }
        
        tone(BUZZER_PIN, 1000);
        delay(500);
        noTone(BUZZER_PIN);
        
        currentStoppedWashing = millis() - stoppedWashing;
      }
    }
    else // hands have been washed for long enough
    {
      // Display happy - all done - message
      Wireling.selectPort(DISPLAY_PORT);
        x = 10;
        y = 4;
      screenBuffer.clear();
      screenBuffer.setCursor(x, y);
      screenBuffer.print("All done! <3");
      display.writeBuffer(screenBuffer.getBuffer(), screenBuffer.getBufferSize());
      delay(1500);

      startWash = 0;
      stopFlag = false;
      currentStoppedWashing = 0;
    }
  }
  
  

  delay(50);
}

void washingMsg(){
  xMax = screenBuffer.width + 20 - screenBuffer.getPrintWidth("KEEP WASHING!");
  yMax = screenBuffer.height + 8 - screenBuffer.getFontHeight();
  x = increment % xMax; if ((increment / xMax) & 1) x = xMax - x;
  y = increment % yMax; if ((increment / yMax) & 1) y = yMax - y;
  x -= 10;
  y -= 4;

  Wireling.selectPort(DISPLAY_PORT);
  screenBuffer.clear();
  screenBuffer.setCursor(x, y);
  screenBuffer.print("KEEP WASHING!");
  Wire.setClock(1000000);
  display.writeBuffer(screenBuffer.getBuffer(), screenBuffer.getBufferSize());
  Wire.setClock(50000);

  increment++;
  delay(10);
}

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
