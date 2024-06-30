#include <Wire.h>             // I2C library
#include <Adafruit_MS8607.h>  // Adafruit
#include <Adafruit_Sensor.h>  //Adafruit
Adafruit_MS8607 ms8607;       //creating object for the adafruit TPH sensor
#include <RH_RF69.h>          //Radiohead library
#include <SPI.h>              // Used for radio
#include <WindFunctions.h>    //Wind sensors: anemometer and wind direction vane
#include <avr/wdt.h>          // watchdog to prevent program hangups
#include <RHReliableDatagram.h>


#define SERVER_ADDRESS 1

#define POWER 2
#define RF69_FREQ 915.0

#define RFM69_CS 4   // Chip Select
#define RFM69_INT 3  // Interrupt
#define RFM69_RST 2  // Reset
#define LED 8        // RX LED

//Radio Driver
RH_RF69 rf69(RFM69_CS, RFM69_INT);  //Create the object for the radio driver
RHReliableDatagram rf69_manager(rf69, SERVER_ADDRESS);

WindFunctions wind_functions;  //creating an object for the Wind Functions library

long current_time;

//Structure defining the data from the sensors This structure used globally, so it's not in a class.
struct TPHData {
  int16_t temperature;
  int16_t pressure;
  int16_t humidity;
  int16_t speed;
  int16_t direction;
};

TPHData Bundle;  //Create the object for the data structure

// Correct for the station elevation. The unit is inHG*100 For my station at 60ft, it's .07inHG
uint8_t alt_correction = 7;

class RadioFunctions {
public:


  void Radio() {

    if (rf69_manager.waitAvailableTimeout(4000, 200)) {
      // Wait for a message addressed to us from the client
      current_time = millis();

      Blink(LED, 30, 2);  // blink LED 3 times, 40ms between blinks
      if (rf69_manager.recvfromAck(buf, &len, &from)) {

        // Send a reply back to the originator client
        if (!rf69_manager.sendtoWait((uint8_t*) &Bundle, sizeof(Bundle), from)) { 
          Blink(LED, 100, 3);
        }
      }
    }
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

//Declare the variable for the Radio Functions Class
RadioFunctions RF;

class SensorFunctions {

public:

  void GetSensorReadings() {
    //gets the sensor readings from the TPH sensor
    if (!ms8607.reset()) {

      Bundle.temperature = 8;
      Bundle.pressure = 8;
      Bundle.humidity = 8;

      Serial.println("TPH sensor is not responding");

    } else {
      ms8607.getEvent(&pressure, &temp, &humidity);
      Bundle.temperature = temp.temperature * 1.8 + 32;  //Converts Celcius to Fahrenheit
      Bundle.pressure = (pressure.pressure * 2.953) - alt_correction;       //Converts from hPa to inHG*100. Decimal will be added later on the receiver end
      Bundle.humidity = humidity.relative_humidity;
    }
  }

private:
  sensors_event_t temp, pressure, humidity;
};

//Declare the sensor functions class
SensorFunctions SF;

void setup() {
  Serial.begin(9600);

  wdt_enable(WDTO_8S);  // Enable the Watchdog at 8 seconds. Resets the board if there's a hangup somewhere.
  Serial.println("Watchdog enable");

  // Try to initialize adafruit sensor
  if (!ms8607.begin()) {
    Serial.println("Failed to find MS8607 chip");
    while (1) { break; }
  } else {
    Serial.println("Found MS8607");
  }

  //End of adafruit sensor module initialization
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
    Serial.println("Reliable datagram manager initialization failed");
  } else {
    Serial.println("Radio Init OK");
  }

  if (!rf69.setFrequency(RF69_FREQ)) {
    Serial.println("setFrequency failed");
  } else {
    Serial.println("Frequency Set OK");
  }

  // If you are using a high power RF69 eg RFM69HW, you *must* set a Tx power with the
  // ishighpowermodule flag set like this:
  rf69.setTxPower(POWER, true);  // 2nd arg must be true for 69HCW
  Serial.print("Radio power set to"); Serial.println(POWER);
  // The encryption key has to be the same as the one in the server
  uint8_t key[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
  rf69.setEncryptionKey(key);

  Serial.println("Encryption key: ");

  Serial.print("RFM69 radio @");
  Serial.print((int)RF69_FREQ);
  Serial.println(" MHz");

  //End radio section

  current_time = millis();
}

void loop() {

  //gets the sensor readings
  SF.GetSensorReadings();

  Bundle.speed = wind_functions.readWindSpeed(0x03) * 2.236;  // convert to miles per hour

  Bundle.direction = wind_functions.readWindDirection16(0x02);

  RF.Radio();

  wdt_reset();  // reset the watchdog


  while (millis() - current_time >= 10000) {
    RF.Radio();
    Bundle.temperature = 7;
    Bundle.pressure = 7;
    Bundle.humidity = 7;
    Bundle.speed = 7;
    Bundle.direction = 19;
    wdt_reset();
    if (millis() - current_time < 10000) {
      break;
    }
  }
}
