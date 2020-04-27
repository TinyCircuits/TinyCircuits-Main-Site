/*******************************************************************************
 * TinyCircuits TinyScreen+ & RTCZero Library
 * This example shows how to use the RTCZero with the TinyScreen+ for external
 * interrupts to wake from sleep mode, and how to display time on the OLED 
 * screen display of the TinyScreen+ using the SAMD21 built-in Real-Time Clock. 
 * 
 * Written by Laveréna Wienclaw for TinyCircuits, April 27, 2020
 * 
 * http://TinyCircuits.com
 *******************************************************************************/

#include <SPI.h>
#include <TinyScreen.h>
TinyScreen display = TinyScreen(TinyScreenPlus);

#define SerialMonitorInterface SerialUSB
#include <RTCZero.h>
RTCZero rtc;

void RTCwakeHandler() {
}

void wakeHandler() {
}

/* Change these values to set the current initial time */
const byte seconds = 30;
const byte minutes = 30;
const byte hours = 12;

/* Change these values to set the current initial date */
const byte day = 18;
const byte month = 7;
const byte year = 20;

void setup(void)
{
  for (int i = 0; i <= 45; i++) {
    if ( (i != PIN_USB_DM) && (i != PIN_USB_DP)) {
      pinMode(i, INPUT_PULLUP);
    }
  }
  display.begin();
  
  rtc.begin();

  // Set the time & date
  rtc.setTime(hours, minutes, seconds);
  rtc.setDate(day, month, year);

  // Set Interrupt
  rtc.attachInterrupt(RTCwakeHandler);
  attachInterrupt(TSP_PIN_BT1, wakeHandler, FALLING);
  attachInterrupt(TSP_PIN_BT2, wakeHandler, FALLING);
  attachInterrupt(TSP_PIN_BT3, wakeHandler, FALLING);
  attachInterrupt(TSP_PIN_BT4, wakeHandler, FALLING);

  SerialMonitorInterface.begin(115200);


#if defined(ARDUINO_ARCH_SAMD)
  // https://github.com/arduino/ArduinoCore-samd/issues/142
  // Clock EIC in sleep mode so that we can use pin change interrupts
  // The RTCZero library will setup generic clock 2 to XOSC32K/32
  // and we'll use that for the EIC. Increases power consumption by ~50uA
  GCLK->CLKCTRL.reg = uint16_t(GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK2 | GCLK_CLKCTRL_ID( GCLK_CLKCTRL_ID_EIC_Val ) );
  while (GCLK->STATUS.bit.SYNCBUSY) {}
#endif
  display.setFont(thinPixel7_10ptFontInfo);
  display.fontColor(TS_8b_White, TS_8b_Black);

  delay(1000);
}


void loop() {
  display.on();
  
  printTime();
  int timeNow = millis();
  while (millis() < timeNow + 1000){} // fancy delay

  display.off();
  rtc.standbyMode();
}

void printTime(void){
  display.clearScreen();
  display.setCursor(0, 0);
  display.print(rtc.getDay());
  display.print("/");
  display.print(rtc.getMonth());
  display.print("/");
  display.print(rtc.getYear());

  display.setCursor(0, 12);
  display.print(rtc.getHours());
  display.print(":");
  display.print(rtc.getMinutes());
  display.print(":");
  display.print(rtc.getSeconds());
}
