#include <Wire.h>             // I2C library
#include <Adafruit_MS8607.h>  // Adafruit
#include <Adafruit_Sensor.h>  //Adafruit
#include <RH_RF69.h>          //Radiohead library
#include <SPI.h>              // Used for radio
#include <WindFunctions.h>    //Wind sensors: anemometer and wind direction vane
#include <RHDatagram.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

// Configuration
#define SERVER_ADDRESS 1 //This device's radio address
#define POWER 6 // Radio Power Output dbm
#define RF69_FREQ 902.3 // Radio Frequency
#define RFM69_CS 4   // Chip Select
#define RFM69_INT 3  // Interrupt
#define RFM69_RST 2  // Reset
#define LED 8        // Receiver LED pin
#define WINDSENSORS 5 //Pin that switches the wind sensors on or off for power saving
#define AZIMUTH 220 // The direction that the wind direction sensor is actually pointed.
#define ALT_CORRECTION 7 // InHG*100 correction for altitude


// Library Objects
Adafruit_MS8607 ms8607;       //creating object for the adafruit TPH sensor
WindFunctions wind_functions; //My library for the wind sensors :-)
RH_RF69 rf69(RFM69_CS, RFM69_INT);  //Create the object for the radio driver
RHDatagram rf69_manager(rf69, SERVER_ADDRESS); // Rediable datagram object for the radio


// Defining variables
unsigned long current_time;
unsigned long blink_interval;
bool blink;
bool LEDstatus;
bool messageRX;
bool standby;
uint8_t from;

//Structure defining the data from the sensors This structure used globally, so it's not in a class.
struct TPHData {
  int16_t temperature;
  int16_t pressure;
  int16_t humidity;
  int16_t speed;
  int16_t direction;
}; TPHData Bundle;  //Create the object for the data structure

#define CONFIG_GFSK (RH_RF69_DATAMODUL_DATAMODE_PACKET | RH_RF69_DATAMODUL_MODULATIONTYPE_FSK | RH_RF69_DATAMODUL_MODULATIONSHAPING_FSK_BT1_0)
#define CONFIG_MANCHESTER (RH_RF69_PACKETCONFIG1_PACKETFORMAT_VARIABLE | RH_RF69_PACKETCONFIG1_DCFREE_MANCHESTER | RH_RF69_PACKETCONFIG1_CRC_ON | RH_RF69_PACKETCONFIG1_ADDRESSFILTERING_NONE)

static const RH_RF69::ModemConfig Table = { CONFIG_GFSK, 0x01, 0x00, 0x08, 0x00, 0xe1, 0xe1, CONFIG_MANCHESTER};


void EndianChanger(){
  Bundle.temperature = htons(Bundle.temperature);
  Bundle.pressure = htons(Bundle.pressure);
  Bundle.humidity = htons(Bundle.humidity);
  Bundle.speed = htons(Bundle.speed);
  Bundle.direction = htons(Bundle.direction);
}
void RadioRX() {
  uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
  current_time=millis();
  // Wait for a message addressed to us from the client
  if (rf69_manager.recvfrom(buf, &len, &from)) {
    if(!LEDstatus){blink=true;}
    messageRX = true;
  }

}

void RadioTX(){
  // Send a reply back to the originator client
  rf69_manager.sendto((uint8_t*) &Bundle, sizeof(Bundle), from);
}

void GetTPHSensorReadings() {
  sensors_event_t temp, pressure, humidity;
  //gets the sensor readings from the TPH sensor
  if (!ms8607.reset()) {

    Bundle.temperature = 8;
    Bundle.pressure = 8;
    Bundle.humidity = 8;

    Serial.println("TPH sensor is not responding");


  } else {
    ms8607.getEvent(&pressure, &temp, &humidity);
    Bundle.temperature = temp.temperature * 1.8 + 32;  //Converts Celcius to Fahrenheit
    Bundle.pressure = (pressure.pressure * 2.953) + ALT_CORRECTION;       //Converts from hPa to inHG*100. Decimal will be added later on the receiver end
    Bundle.humidity = humidity.relative_humidity;
  }
}

void goToSleep() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sei();
  sleep_cpu();
  //Resumes here after sleep interrupt from radio
  sleep_disable();
  standby = false;
  digitalWrite(WINDSENSORS, HIGH);
  delay(1000); //The wind sensors don't wake up very quickly, and will give garbage readings.
}

void wakeUp() {
  //Nothing needs to be done here.

}


void setup() {
  Serial.begin(9600);

  //wdt_enable(WDTO_8S);  // Enable the Watchdog at 8 seconds. Resets the board if there's a hangup somewhere.
  Serial.println("Watchdog enable");

  
  // disable ADC
  ADCSRA = 0;
  Serial.println("ADC turned off");

  attachInterrupt(digitalPinToInterrupt(RFM69_INT), wakeUp, RISING);

  // Try to initialize adafruit sensor
  if (!ms8607.begin()) {
    Serial.println("Failed to find MS8607 chip");
  } else {
    Serial.println("Found MS8607");
  }

  //End of adafruit sensor module initialization

  //The Wind Sensors On-Off switch. Helps conserve power.
  pinMode(WINDSENSORS, OUTPUT);
  digitalWrite(WINDSENSORS, HIGH);

  //Radio Section
  pinMode(RFM69_RST, OUTPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(RFM69_RST, LOW);

  Serial.println("RFM69 Start");

  digitalWrite(RFM69_RST, HIGH);
  delay(10);
  digitalWrite(RFM69_RST, LOW);
  delay(10);

  Serial.println("Radio Reset");

  if (!rf69_manager.init()) {
    Serial.println("Datagram manager initialization failed");
  } else {
    Serial.println("Radio Init OK");
  }

  if (!rf69.setFrequency(RF69_FREQ)) {
    Serial.println("setFrequency failed");
  } else {
    Serial.println("Frequency Set");
  }

  rf69.setModemRegisters(&Table);

  // If you are using a high power RF69 eg RFM69HW, you *must* set a Tx power with the
  // ishighpowermodule flag set like this:
  rf69.setTxPower(POWER, true);  // 2nd arg must be true for 69HCW
  Serial.print("Radio power set to"); Serial.println(POWER);
  Serial.print("RFM69 radio @"); Serial.print((int)RF69_FREQ); Serial.println(" MHz");

  //End radio section
}

void loop() {

  if (!rf69_manager.available()){
    if (!standby && millis()-current_time > 8000){
      digitalWrite(LED, LOW);
      digitalWrite(WINDSENSORS, LOW);
      rf69.setModeRx();
      messageRX = false;
      standby = true;
      goToSleep();
    }
  }else{
    RadioRX();
        //gets the TPH sensor readings
    GetTPHSensorReadings();
    //gets the direction sensor readings
    Bundle.direction=(wind_functions.readWindDirection360(0x02));
    
    if(Bundle.direction != -1){
      Bundle.direction = (Bundle.direction/10) + AZIMUTH;
      if(Bundle.direction == 0){
        Bundle.direction = 360;
      }
      if (Bundle.direction > 360){
        Bundle.direction = Bundle.direction - 360;
      }
    }
    //Gets speed sensor readings
    Bundle.speed = wind_functions.readWindSpeed(0x03) * 2.236;  // convert to miles per hour

    //Changes the byte order of outgoing 16 bit integers. 
    EndianChanger();
    RadioTX();
    }


  if(!blink && LEDstatus && millis()-blink_interval>30){
    digitalWrite(LED, LOW);
    LEDstatus=false;
  }

  if (blink && !LEDstatus){
    digitalWrite(LED, HIGH);
    blink=false;
    LEDstatus=true;
    blink_interval=millis();
  }

}
