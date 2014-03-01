
//----------------------------------------------------------------------------------------------------------------------
// TinyTX - An ATtiny84 and RFM12B Wireless Temperature Sensor Node
// By Nathan Chantrell. For hardware design see http://nathan.chantrell.net/tinytx
//
// Using the Dallas DS18B20 temperature sensor
//
// Licenced under the Creative Commons Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0) licence:
// http://creativecommons.org/licenses/by-sa/3.0/
//
// Requires Arduino IDE with arduino-tiny core: http://code.google.com/p/arduino-tiny/
// and small change to OneWire library, see: http://arduino.cc/forum/index.php/topic,91491.msg687523.html#msg687523
//----------------------------------------------------------------------------------------------------------------------

#include <OneWire.h> // http://www.pjrc.com/teensy/arduino_libraries/OneWire.zip
#include <JeeLib.h> // https://github.com/jcw/jeelib
#include <PinChangeInterrupt.h>
#include <DallasTemperature.h>


ISR(WDT_vect) { Sleepy::watchdogEvent(); } // interrupt handler for JeeLabs Sleepy power saving

#define myNodeID 3        // RF12 node ID in the range 1-30
#define network 210       // RF12 Network group
#define freq RF12_868MHZ  // Frequency of RFM12B module

#define USE_ACK           // Enable ACKs, comment out to disable
#define RETRY_PERIOD 5    // How soon to retry (in seconds) if ACK didn't come in
#define RETRY_LIMIT 5     // Maximum number of times to retry
#define ACK_TIME 10       // Number of milliseconds to wait for an ack

#define ONE_WIRE_BUS 10   // DS18B20 Temperature sensor is connected on D10/ATtiny pin 13
#define ONE_WIRE_POWER 9  // DS18B20 Power pin is connected on D9/ATtiny pin 12

#define PIR_PIN 7 // D7
#define LDR_POWER 0
#define LDR_PIN 7 // A7

OneWire oneWire(ONE_WIRE_BUS); // Setup a oneWire instance

DallasTemperature sensors(&oneWire); // Pass our oneWire reference to Dallas Temperature

//########################################################################################################################
//Data Structure to be sent
//########################################################################################################################

 typedef struct {
  	  int temp;	// Temperature reading
          int light;
  	  int supplyV;	// Supply voltage
          int pir;
 } Payload;

 Payload tinytx;
 
 boolean ignore = false;
 int lastPirState = -1;
 int pirState = -1;

// Wait a few milliseconds for proper ACK
 #ifdef USE_ACK
  static byte waitForAck() {
   MilliTimer ackTimer;
   while (!ackTimer.poll(ACK_TIME)) {
     if (rf12_recvDone() && rf12_crc == 0 &&
        rf12_hdr == (RF12_HDR_DST | RF12_HDR_CTL | myNodeID))
        return 1;
     }
   return 0;
  }
 #endif

//--------------------------------------------------------------------------------------------------
// Send payload data via RF
//-------------------------------------------------------------------------------------------------
 static void rfwrite(){
  #ifdef USE_ACK
   for (byte i = 0; i <= RETRY_LIMIT; ++i) {  // tx and wait for ack up to RETRY_LIMIT times
     rf12_sleep(-1);              // Wake up RF module
      while (!rf12_canSend())
      rf12_recvDone();
      rf12_sendStart(RF12_HDR_ACK, &tinytx, sizeof tinytx); 
      rf12_sendWait(2);           // Wait for RF to finish sending while in standby mode
      byte acked = waitForAck();  // Wait for ACK
      rf12_sleep(0);              // Put RF module to sleep
      if (acked) { return; }      // Return if ACK received
  
   Sleepy::loseSomeTime(RETRY_PERIOD * 1000);     // If no ack received wait and try again
   }
  #else
     rf12_sleep(-1);              // Wake up RF module
     while (!rf12_canSend())
     rf12_recvDone();
     rf12_sendStart(0, &tinytx, sizeof tinytx); 
     rf12_sendWait(2);           // Wait for RF to finish sending while in standby mode
     rf12_sleep(0);              // Put RF module to sleep
     return;
  #endif
 }



//--------------------------------------------------------------------------------------------------
// Read current supply voltage
//--------------------------------------------------------------------------------------------------
 long readVcc() {
   bitClear(PRR, PRADC); ADCSRA |= bit(ADEN); // Enable the ADC
   long result;
   // Read 1.1V reference against Vcc
   #if defined(__AVR_ATtiny84__) 
    ADMUX = _BV(MUX5) | _BV(MUX0); // For ATtiny84
   #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);  // For ATmega328
   #endif 
   delay(2); // Wait for Vref to settle
   ADCSRA |= _BV(ADSC); // Convert
   while (bit_is_set(ADCSRA,ADSC));
   result = ADCL;
   result |= ADCH<<8;
   result = 1126400L / result; // Back-calculate Vcc in mV
   ADCSRA &= ~ bit(ADEN); bitSet(PRR, PRADC); // Disable the ADC to save power
   return result;
} 

//void wakeUp() {
//  pirState = digitalRead(PIR_PIN); // Read the state of the reed switch
//  if(pirState == lastPirState){
//    ignore = true;
//  } else {
//    ignore = false; 
//  }
//  
//  delay(100);
//  lastPirState = pirState;
//
//
//}

void setup() {

  rf12_initialize(myNodeID,freq,network); // Initialize RFM12 with settings defined above 
  rf12_sleep(0);                          // Put the RFM12 to sleep

  pinMode(ONE_WIRE_POWER, OUTPUT); // set power pin for DS18B20 to output
  
  //pinMode(PIR_PIN, INPUT);                   //set the pin to input
  //pinMode(PIR_PIN, INPUT_PULLUP);

  //attachPcInterrupt(PIR_PIN,wakeUp,CHANGE); // attach a PinChange Interrupt on the falling edge
  
    
  pinMode(LDR_POWER, OUTPUT); // set D7 to output


}

void loop() {
  

    //if(!ignore) {
      
    digitalWrite(LDR_POWER, HIGH);   // turn the LED on (HIGH is the voltage level)
    digitalWrite(ONE_WIRE_POWER, HIGH); // turn DS18B20 sensor on
    delay(10); // The above doesn't seem to work for everyone (why?)

  
    tinytx.pir = (pirState == LOW)?0:1;

    bitClear(PRR, PRADC); ADCSRA |= bit(ADEN); // Enable the ADC
    tinytx.light = 1024-analogRead(LDR_PIN);   // read the input pin (A2)
    ADCSRA &= ~ bit(ADEN); bitSet(PRR, PRADC); // Disable the ADC to save power
    digitalWrite(LDR_POWER, LOW); // set D9 low
 
    sensors.begin(); //start up temp sensor
    sensors.requestTemperatures(); // Get the temperature
    tinytx.temp=(sensors.getTempCByIndex(0)*100); // Read first sensor and convert to integer, reversed at receiving end
    //tinytx.temp=getTemp();
  
    digitalWrite(ONE_WIRE_POWER, LOW); // turn DS18B20 off
  
    tinytx.supplyV = readVcc();

    rfwrite(); // Send data via RF 
    digitalWrite(LDR_POWER, LOW);



    Sleepy::loseSomeTime(60000); //JeeLabs power save function: enter low power mode for 60 seconds (valid range 16-65000 ms)
  
    //ignore = false;
    
    //}


}

//int getTemp(){
// //returns the temperature from one DS18S20 in DEG Celsius
//
// byte data[12];
// byte addr[8];
//
// if ( !ds.search(addr)) {
//   //no more sensors on chain, reset search
//   ds.reset_search();
//   return -1000;
// }
//
// if ( OneWire::crc8( addr, 7) != addr[7]) {
//   return -1000;
// }
//
// if ( addr[0] != 0x10 && addr[0] != 0x28) {
//   return -1000;
// }
//
// ds.reset();
// ds.select(addr);
// ds.write(0x44,1); // start conversion, with parasite power on at the end
//
// byte present = ds.reset();
// ds.select(addr);  
// ds.write(0xBE); // Read Scratchpad
//
// 
// for (int i = 0; i < 9; i++) { // we need 9 bytes
//  data[i] = ds.read();
// }
// 
// ds.reset_search();
// 
// byte MSB = data[1];
// byte LSB = data[0];
//
// float tempRead = ((MSB << 8) | LSB); //using two's compliment
// float TemperatureSum = tempRead / 16;
// 
// return TemperatureSum * 100;
// 
//}

