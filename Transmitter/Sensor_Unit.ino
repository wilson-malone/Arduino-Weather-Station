#include <Wire.h>             // I2C library
#include <Adafruit_MS8607.h>  // Adafruit
#include <Adafruit_Sensor.h>  //Adafruit
#include <RH_RF69.h>          //Radiohead library
#include <SPI.h>              // Used for radio
#include <WindFunctions.h>    //Wind sensors: anemometer and wind direction vane
#include <avr/wdt.h>          // watchdog to prevent program hangups
#include <RHDatagram.h>

// Configuration
#define SERVER_ADDRESS 1 //This device's radio address
#define POWER 5 // Radio Power Output dbm
#define RF69_FREQ 915.0 // Radio Frequency
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
uint8_t receive_retry = 0;
int16_t WindDirection;

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

static const RH_RF69::ModemConfig Table = {CONFIG_GFSK, 0x06, 0x83, 0x02, 0x75, 0xf3, 0xf3, CONFIG_MANCHESTER};

class RadioFunctions {
public:
 //Arduinos are little endian, so you have to change to network byte order for transmission.
  void EndianChanger(){
    Bundle.temperature = htons(Bundle.temperature);
    Bundle.pressure = htons(Bundle.pressure);
    Bundle.humidity = htons(Bundle.humidity);
    Bundle.speed = htons(Bundle.speed);
    Bundle.direction = htons(Bundle.direction);
  }
  bool Radio() {
    bool ret = false;
    if (rf69_manager.waitAvailableTimeout(2000)) {
      // Wait for a message addressed to us from the client
      if (rf69_manager.recvfrom(buf, &len, &from)) {
        rf69_manager.sendto((uint8_t*) &Bundle, sizeof(Bundle), from);
        ret = true;
        receive_retry=0;
        Blink(LED, 10, 2);  // blink LED 2 times, 10ms between blinks
        // Send a reply back to the originator client
      }
    }
    return ret;
  }
private:
  uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
  uint8_t from;

  void Blink(byte pin, byte delay_ms, byte loops) {
    while (loops--) {
      digitalWrite(pin, HIGH);
      delay(delay_ms);
      digitalWrite(pin, LOW);
      delay(delay_ms);
    }
  }
};

class SensorFunctions {
public:
  void GetTPHSensorReadings() {
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

private:
  sensors_event_t temp, pressure, humidity;
};

//Declare the classes
SensorFunctions SF;
RadioFunctions RF;


void setup() {
  Serial.begin(9600);

  wdt_enable(WDTO_8S);  // Enable the Watchdog at 8 seconds. Resets the board if there's a hangup somewhere.
  Serial.println("Watchdog enable");

  
  // disable ADC for power saving. I'm not using it anyways.
  ADCSRA = 0;
  Serial.println("ADC turned off");

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

  //A custom modem register using Manchester encoding at 19.2k baud.
  rf69.setModemRegisters(&Table);

  // If you are using a high power RF69 eg RFM69HW, you *must* set a Tx power with the
  // ishighpowermodule flag set like this:
  rf69.setTxPower(POWER, true);  // 2nd arg must be true for 69HCW
  Serial.print("Radio power set to"); Serial.println(POWER);
  Serial.print("RFM69 radio @"); Serial.print((int)RF69_FREQ); Serial.println(" MHz");

  //End radio section



}

void loop() {
  //gets the TPH sensor readings
  SF.GetTPHSensorReadings();
  //gets the direction sensor readings
  Bundle.direction=(wind_functions.readWindDirection360(0x02));
  
  if(Bundle.direction != -1){
    Bundle.direction = (Bundle.direction/10) + AZIMUTH;
  }
  if(Bundle.direction == 0){
    Bundle.direction = 360;
  }
  if (Bundle.direction > 360){
    Bundle.direction = Bundle.direction - 360;
  }

  //Gets speed sensor readings
  Bundle.speed = wind_functions.readWindSpeed(0x03) * 2.236;  // convert to miles per hour

  //Changes the byte order of outgoing 16 bit integers. 
  RF.EndianChanger();

  if (!RF.Radio()){
    receive_retry++;
    if (receive_retry > 3){
      digitalWrite(WINDSENSORS, LOW);
      Bundle.temperature = 7;
      Bundle.pressure = 7;
      Bundle.humidity = 7;
      Bundle.speed = 7;
      Bundle.direction = 19;
      RF.EndianChanger();
      while(!RF.Radio()){
        wdt_reset();
      }
      digitalWrite(WINDSENSORS, HIGH);
      
    }
  }

  wdt_reset();
}
