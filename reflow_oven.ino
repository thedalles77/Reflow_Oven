/* Copyright 2020 Frank Adams
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at
       http://www.apache.org/licenses/LICENSE-2.0
   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
// 
// This Arduino Nano code controls a modified toaster oven which produces the temperature profile needed to
// reflow standard tin-lead solder on a circuit board. An Arduino Nano controls two
// SSRs that power the upper and lower heating elements. The Max6675 reads
// the K-type thermocouple and converts the voltage to a temperature value that is read 
// by the Nano over a SPI bus. The 4 bit bus of a 1602 LCD is controlled by the Nano to 
// display the time, temperature, reflow stage, and relay control signal information. 
// The SSR PWM control signals pulse at a 1 second rate and stay high for an adjustable length of time.
// The PWM values range from 0 thru 6 and give the following millisecond pulse widths:
// 0=off always, 1=167ms, 2=333ms, 3=500ms, 4=667ms, 5=833ms, 6=on always
// The aluminum tray protects the circuit board from being scorched by the lower heating element so it
// can be set to full power. The upper heating element will burn the board so it should never be set past 3 (50%).

// include the LCD and SPI library code:
#include <LiquidCrystal.h>
#include <SPI.h>

// Variables
char stage = 0; // initialize stage counter to beginning value
int top_pwm = 0; // holds the top element pwm value (0 thru 6)
int bottom_pwm = 0; // holds the bottom element pwm value (0 thru 6)
double temperature; // holds the temperature reading from the Max6675
double maxtemp = 100; // maximum temperature reached during reflow
double previous_temp; // holds temperature from previous loop
unsigned long start_door; // time when reflow peak temperature is reached
unsigned long open_door; // length of time since reflow peak temperature
unsigned long loop_timer; // tracks the time when the 1 second loop starts
unsigned long timeat50; // elapsed seconds at 50 degrees
unsigned long timeat150; // elapsed seconds at 150 degrees
unsigned long timeat180up; // elapsed seconds at 180 degrees going up
unsigned long timeat180dn; // elapsed seconds at 180 degrees going down
unsigned long timeatpeak; // elapsed seconds at peak temperature
unsigned long peaktime; // time to peak is 50C to peak temperature
unsigned long soaktime; // soak time is 150C to 180C 
unsigned long reflowtime; // reflow time is 180C up to 180C down
double downrate; // degC per second cooling rate

// initialize the LCD library with the numbers of the interface pins
LiquidCrystal lcd(7, 6, 5, 4, 3, 2); // lcd pins rs, e, d4, d5, d6, d7 as wired to the nano's I/O numbers

// function to display the stage name and the pwm drive strength
void showit(String stagename) {
 lcd.setCursor(8, 0); // position for the stage name
 lcd.print(stagename); // display stage name
 lcd.setCursor(12, 1); // position for the ssr pwm values
 lcd.print("TB"); // TB stands for top bottom
 lcd.print(top_pwm); // display top heater pwm strength 
 lcd.print(bottom_pwm); // display bottom heater pwm strength  
}

// The following readCelsius function is from an answer to a stackexchange.com question about a SPI temp sensor library:
// https://arduino.stackexchange.com/questions/37193/multiple-3-wire-spi-sensor-interfacing-with-arduino

double readCelsius(uint8_t cs) {
  uint16_t v; // 16 bit unsigned integer to store temperature result
  digitalWrite(cs, LOW); // activate the Max6675 chip select 
  v = SPI.transfer(0x00); // read 8 bits from Max6675
  v <<= 8; // v = v shifted left 8 bits to make room for the lower 8 bits coming next
  v |= SPI.transfer(0x00); // v = v or'ed with the 2nd SPI read value
  digitalWrite(cs, HIGH); // turn off the chip select
  if (v & 0x4) {
    return NAN; // no thermocouple attached                                                                                                                                                                                                                           
  }
  v >>= 3; // v = v right shifted 3 bits
  return v*0.25; // return the value divided by 4 to give temperature in Celsius
}

void setup() {
  Serial.begin(9600); // send and receive at 9600 baud to the Arduino serial monitor
  //
  pinMode(8, OUTPUT); // top element control output signal
  pinMode(9, OUTPUT); // bottom element control output signal
  digitalWrite(8, 0); //turn off top element
  digitalWrite(9, 0); //turn off bottom element
  //
  SPI.begin(); // spare pin D11 can't be used as a general purpose 
  // output because it was defined as MOSI for SPI bus.  
  pinMode(10, OUTPUT); // Setup pin 10 as the Max6675 chip select
  digitalWrite(10, HIGH); // and turn the chip select off
  //
  lcd.clear(); // blank the display
  lcd.begin(16, 2); // set up the LCD's number of columns and rows:
  lcd.home(); // place cursor at upper left "home" position
  lcd.print("Reflow Oven"); // display on the top line
  lcd.setCursor(0, 1); // move cursor to second line
  lcd.print("Tin-Lead Profile"); // display on the second line
  //
  delay(5000); // wait 5 seconds before starting the main loop
  //
}

void loop() {
  loop_timer = millis(); // keep track of when the loop starts in milliseconds
  
// PWM Time Slice 1 starts here. Slice 2 thru 6 are at the bottom of the loop.
// Turn on the top and bottom SSR's if the drive strength is 1 or greater.
 
  if (top_pwm >= 1) {
    digitalWrite(8, 1); //turn on top element
  }
  else {
    digitalWrite(8, 0); //turn off top element
  }
  if (bottom_pwm >= 1) {
    digitalWrite(9, 1); // turn on bottom element
  }
  else {
    digitalWrite(9, 0); // turn off bottom element
  }
//
// LCD Setup and Display  
  lcd.clear(); // blank the display  
  lcd.setCursor(0, 0); // place cursor in upper left corner
  lcd.print((millis() / 1000)-4); // show seconds since oven heating started
  lcd.setCursor(4, 0); // place cursor after the seconds
  lcd.print("Sec"); // show units
// Read Temperature
  lcd.setCursor(0, 1); // place cursor on second line, first column
  delay(1); // wait 1 msec to let the noisy control signals die down from the LCD 
  temperature = readCelsius(10); // read Max6675 temperature
  lcd.print(temperature); // show temperature on LCD
  lcd.setCursor(6, 1); // place cursor after the temperature
  lcd.print("DegC"); // show units
// Serial Monitor
  Serial.print((millis() / 1000)-4); // send to serial monitor, the time in seconds since oven heating started 
  Serial.print(","); // comma deliniated for spreadsheet input
  Serial.println(temperature); // send temperature to serial monitor and force a new line

// Each stage of the Switch Case statement is for a different part of the solder profile

  switch (stage) { // select the desired stage

    case 0: // Preheat
      // save the system time (since reset) when the temperature passes 50 degrees
      if (temperature <= 50) { 
        timeat50 = (millis() / 1000);
      }
      //Preheat stage is up to 140 degrees with 3 sub-stages
      if (temperature < 40) { // slowly raise the temp
        top_pwm = 0; 
        bottom_pwm = 5; 
        showit("Preheat1"); //Display "Preheat1" and pwm values on LCD      
      }
      else if (temperature < 100) { // slightly increase the ramp-up rate 
        top_pwm = 1; 
        bottom_pwm = 5; 
        showit("Preheat2"); //Display "Preheat2" and pwm values on LCD
      }
      else if (temperature < 140) { // increase to full heat
        top_pwm = 3; 
        bottom_pwm = 6;
        showit("Preheat3"); //Display "Preheat3" and pwm values on LCD 
      }
      else {  
        stage = stage + 1; // temperature over 140 so move on to soak stage
      }
      break;

    case 1: // Soak
      // save the system time (since reset) when the temperature passes 150
      if (temperature <= 150) {
        timeat150 = (millis() / 1000);
      }
      // save the system time (since reset) when the temperature passes 180 going up
      if (temperature <= 180) {
        timeat180up = (millis() / 1000);
      }
      //Soak stage is 140 deg to 180 deg with 3 sub-stages
      if (temperature < 150) { // reduce heating from 140 to 150 degrees to slow the rate
        top_pwm = 1; 
        bottom_pwm = 4;  
        showit("Soak 1  "); //Display "Soak 1  " and pwm values on LCD    
      }
      else if (temperature < 170) { // move up from 150 to 170 slowly 
        top_pwm = 2; 
        bottom_pwm = 5;  
        showit("Soak 2  "); //Display "Soak 2  " and pwm values on LCD   
      }
      else if (temperature < 180) { // move up from 170 to 180 a little faster 
        top_pwm = 2; 
        bottom_pwm = 6; 
        showit("Soak 3  "); //Display "Soak 3  " and pwm values on LCD 
      }
      else {
        stage = stage + 1; // temperature over 180 so move on to reflow stage
      }
      break;

    case 2: // Reflow
      //Reflow stage from 180 to 212 with 3 sub-stages (currently all set to full heat)
      if (temperature < 190) { 
        top_pwm = 3; 
        bottom_pwm = 6;
        showit("Reflow1 "); //Display "Reflow1 " and pwm values on LCD    
      }
      else if (temperature < 200) { 
        top_pwm = 3; 
        bottom_pwm = 6;
        showit("Reflow2 "); //Display "Reflow2 " and pwm values on LCD               
      }
      else if (temperature < 212) { 
        top_pwm = 3; 
        bottom_pwm = 6; 
        showit("Reflow3 "); //Display "Reflow3 " and pwm values on LCD               
      }
      else {
        start_door = (millis() / 1000); // save the system time for the door opening 
        stage = stage + 1; // move on to cool down stage
      } 
      break;

    case 3: // Cool
      //Cooldown stage is from max temp to 50 degrees 
      top_pwm = 0; // keep the heating elements off
      bottom_pwm = 0;
      if (temperature > 50) {
        // save the system time when the temperature passes 180 degrees going down
        if (temperature >= 180) {
          timeat180dn = (millis() / 1000);
        }
        // capture the maximum temperature
        if (temperature >= maxtemp) {
          maxtemp = temperature; // save max temperature 
          timeatpeak = (millis() / 1000); // save system time at max temperature
        }
        // delay 8 seconds after entering cooldown stage before opening the door 1/2 inch
        open_door = ((millis() / 1000) - start_door); // see how long since entering this stage
        if (open_door >= 8) { // 8 or more seconds have passed
          showit("OpenDoor"); //Display "OpenDoor" and pwm values on LCD 
          downrate = temperature - previous_temp; // adjust door opening to give -2 deg/sec rate
          lcd.setCursor(11, 1); // set position on second line, 11th position
          lcd.print(downrate); // display the negative slope. This overwrites the TB00 pwm values
        }
        else { // 8 seconds have not yet passed
          showit("Rdy Door"); // Display "Rdy Door" and pwm values on LCD
        }
      }
      // At 50 degrees, display the final results and hang the program  
      else { 
        peaktime = timeatpeak - timeat50; // the time in seconds from 50C to the Peak temperature
        soaktime = timeat180up - timeat150; // the time in seconds from 150C to 180C going up
        reflowtime = timeat180dn - timeat180up; // the time in seconds from 180C to peak and back to 180C
        lcd.clear(); // blank the display
        lcd.home(); // place cursor at upper left "home" position        
        lcd.print(maxtemp); // display the maximum temperature value in C
        lcd.setCursor(3, 0); // move cursor past the temperature value
        if (reflowtime < 100) { // 2 digit reflow time
          lcd.print("Cmax  "); // Cmax with 2 spaces
        }
        else { // 3 digit reflow time
          lcd.print("Cmax ");  // Cmax with 1 space
        }
        lcd.print(reflowtime); // display the reflow time value
        lcd.setCursor(11, 0); // move cursor past the time value
        lcd.print("sRflo"); // display s for seconds & Rflo for reflow
        // second line                 
        lcd.setCursor(0, 1); // move cursor to second line
        lcd.print(soaktime); // display the soak time value
        if (soaktime < 100) { // 2 digit value
          lcd.setCursor(2, 1); 
          lcd.print("sSoak  "); // s for seconds & Soak w/ 2 spaces
        }
        else { // 3 digit value
          lcd.setCursor(3, 1);
          lcd.print("sSoak "); // s for seconds & Soak w/ 1 space
        }
        lcd.print(peaktime); // display the peak time value
        lcd.print("sPk"); // s for seconds & Pk for peak

        while(1) {
          //infinite loop so the Arduino serial monitor screen values can be copied to a file
        }
      }
      break;      
    default:
      // if the stage value is not 0 thru 3, things are messed up so turn off the oven and stop
      digitalWrite(8, 0); // force both elements off
      digitalWrite(9, 0); 
      showit("**Error*"); // Display error message on LCD
      while(1) {
        //infinite loop hangs program because something is wrong
      }       
      break;
  }
  previous_temp = temperature; // save temperature for the next loop 
// End of Switch Case
//  
// Heater element control PWM section 
//
// ************************Time Slice 1
// The PWM controls for Time Slice 1 are at the beginning of the loop
// so that the time to execute the loop code is included.
  while ((millis() - loop_timer) < 166) {
    // keep looping until 166 msec have passed
  }
//
// ***********************Time Slice 2
  if (top_pwm >= 2) {
    digitalWrite(8, 1); //turn on top element
  }
  else {
    digitalWrite(8, 0); //turn off top element
  }
  if (bottom_pwm >= 2) {
    digitalWrite(9, 1); // turn on bottom element
  }
  else {
    digitalWrite(9, 0); // turn off bottom element
  }
//
  while ((millis() - loop_timer) < 333) {
    // keep looping until 333 msec have passed
  }
//
// **********************Time Slice 3
  if (top_pwm >= 3) {
    digitalWrite(8, 1); //turn on top element
  }
  else {
    digitalWrite(8, 0); //turn off top element
  }
  if (bottom_pwm >= 3) {
    digitalWrite(9, 1); // turn on bottom element
  }
  else {
    digitalWrite(9, 0); // turn off bottom element
  }
//
  while ((millis() - loop_timer) < 500) {
    // keep looping until 500 msec have passed
  }
//
// *********************Time Slice 4
  if (top_pwm >= 4) {
    digitalWrite(8, 1); //turn on top element
  }
  else {
    digitalWrite(8, 0); //turn off top element
  }
  if (bottom_pwm >= 4) {
    digitalWrite(9, 1); // turn on bottom element
  }
  else {
    digitalWrite(9, 0); // turn off bottom element
  }
//
  while ((millis() - loop_timer) < 666) {
    // keep looping until 666 msec have passed
  }
//
// **********************Time Slice 5
  if (top_pwm >= 5) {
    digitalWrite(8, 1); //turn on top element
  }
  else {
    digitalWrite(8, 0); //turn off top element
  }
  if (bottom_pwm >= 5) {
    digitalWrite(9, 1); // turn on bottom element
  }
  else {
    digitalWrite(9, 0); // turn off bottom element
  }
//
  while ((millis() - loop_timer) < 833) {
    // keep looping until 833 msec have passed
  }
//
// **********************Time Slice 6
  if (top_pwm >= 6) {
    digitalWrite(8, 1); //turn on top element
  }
  else {
    digitalWrite(8, 0); //turn off top element
  }
  if (bottom_pwm >= 6) {
    digitalWrite(9, 1); // turn on bottom element
  }
  else {
    digitalWrite(9, 0); // turn off bottom element
  }
//
  while ((millis() - loop_timer) < 1000) {
    // keep looping until 1 second has passed
  }
} // 1 second has passed, so repeat the main loop
