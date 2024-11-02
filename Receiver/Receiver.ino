#include <Wire.h>                           // Include the Arduino I2C library
#include <SPI.h>                            //Serial Peripheral interface. This is used for the radio
#include <RH_RF69.h>                        //Radiohead Library for the receiver
#include <SparkFun_Alphanumeric_Display.h>  //The 14 segment alphanumeric display library
#include <s7s.h>
#include <RHDatagram.h>
#include <WDT.h>

#define SERVER_ADDRESS 1 //Radio server address
#define MY_ADDRESS 2 // This device's address

#define RF69_FREQ 915.0  //915mhz for USA and Canada
#define POWER 5       // Radio power output dbm
#define RFM69_CS 4   // Radio pin for Chip Select
#define RFM69_INT 3  // radio pin for IRQ interrupt.
#define RFM69_RST 2  // Radio pin for radio reset
#define LED 8        // The blinky light LED so that we know that data is being received.

uint8_t transmit_counter = 0; //Defines the retry counter.
char *message[] = { "NO S", "O SI", " SIG", "SIGN", "IGNA", "GNAL", "NAL ", "AL N", "L NO", " NO " }; //No signal message
unsigned long radio_timer;
unsigned long blink_interval;
bool blink;
bool LEDstatus;

// Will be used with sprintf to create strings
char tempString[10];  
char pressString[10];
char humidString[10];
char speedString[10];
char directionString[10];

//Received data structure
struct TPHData {
  int16_t temperature = 9;
  int16_t pressure = 9999;
  int16_t humidity = 99;
  int16_t speed = 9;
  int16_t direction = 17;
}; TPHData Bundle;  // Make a variable for the structure above and call it 'Bundle'

s7s s7dis;  // calling an instance of the seven segment display

HT16K33 _14display; //14 Segment display object for the library functions


// initialize instance of the radio driver
RH_RF69 rf69(RFM69_CS, RFM69_INT);
RHDatagram rf69_manager(rf69, MY_ADDRESS);
#define CONFIG_GFSK (RH_RF69_DATAMODUL_DATAMODE_PACKET | RH_RF69_DATAMODUL_MODULATIONTYPE_FSK | RH_RF69_DATAMODUL_MODULATIONSHAPING_FSK_BT1_0)
#define CONFIG_MANCHESTER (RH_RF69_PACKETCONFIG1_PACKETFORMAT_VARIABLE | RH_RF69_PACKETCONFIG1_DCFREE_MANCHESTER | RH_RF69_PACKETCONFIG1_CRC_ON | RH_RF69_PACKETCONFIG1_ADDRESSFILTERING_NONE)

static const RH_RF69::ModemConfig Table = {CONFIG_GFSK, 0x06, 0x83, 0x02, 0x75, 0xf3, 0xf3, CONFIG_MANCHESTER};

//The displayed directions correspond to this list. Uncomment if using the 16 direction option.
//The wind direction sensor vane has two Norths 0x00 and 0x10 for some reason. It also transmits both 000 and 360 when
//in the 360 degree mode. I don't really understand why that's necessary, but this accomodates it.
//char *_direction_list[] = { "   N", " NNE", "  NE", " ENE", "   E", " ESE", "  SE", " SSE", "   S", " SSW", "  SW", " WSW", "   W", " WNW", "  NW", " NNW", "   N", "INIT", "FAIL", "STBY" };


void RadioTX() {
  uint8_t radiopacket[] = "ON";
  // Send a message to the server
  rf69_manager.sendto(radiopacket, sizeof(radiopacket), SERVER_ADDRESS);
  radio_timer = millis();
}

bool RadioCopy(){
  bool ret = false;
  uint8_t datalen = sizeof(Bundle);
  uint8_t from;
  if (rf69_manager.recvfrom((uint8_t *)&Bundle, &datalen, &from)) {
    Bundle.temperature = ntohs(Bundle.temperature);
    Bundle.pressure = ntohs(Bundle.pressure);
    Bundle.humidity = ntohs(Bundle.humidity);
    Bundle.speed = ntohs(Bundle.speed);
    Bundle.direction = ntohs(Bundle.direction);
    ret = true;
    blink=true;  // blink LED
    transmit_counter=0;
  }
}


void display_data(){
    //Call the 7 segment displays.
    //7 Segment displays need to have the strings formatted to display properly.
    sprintf(tempString, "%3dF", Bundle.temperature);
    sprintf(pressString, "%4d", Bundle.pressure);
    sprintf(humidString, "%3d-", Bundle.humidity);
    sprintf(speedString, "%4d", Bundle.speed);
    sprintf(directionString, "%4d", Bundle.direction);

    //_14display.print(_direction_list[Bundle.direction]);
    //Send data to 14 segment display.
    _14display.print(directionString);
    // Input string to send to 7 segment displays, the display address, and the decimal point configuration as a binary sequence.
    s7dis.Transmit(tempString, 0b00100000, 0x01);
    s7dis.Transmit(pressString, 0b00000010, 0x02);
    s7dis.Transmit(humidString, 0b00101000, 0x03);
    s7dis.Transmit(speedString, 0b00000100, 0x71);
}

//setup function
void setup() {
  Serial.begin(115200);
  
  Serial.println("Serial Begin...");
  Wire.begin();  // Initialize hardware I2C pins
  Serial.println("Wire I2C library begin");
    
  //Initializes the 14 segment display at its default address.
  if (_14display.begin(0x70) == false) {
    Serial.println("14 Segment display did not acknowledge! Freezing.");
  } else {
    Serial.println("Start 14 seg display");
  }

  //The 14 segment display does not rely on non-volatile memory for it's brightness setting, therefore it has to be set each startup.
  // if (_14display.setBrightness(1)) {
  //   Serial.println("Set 14 seg display brightness");
  // } else {
  //   Serial.println("14 seg display brightness failed");
  // }

   _14display.print("ON");

  // Clear the 7 segment displays before jumping into loop
  s7dis.clearDisplayI2C(0x01);
  s7dis.clearDisplayI2C(0x02);
  s7dis.clearDisplayI2C(0x03);
  s7dis.clearDisplayI2C(0x71);

  Serial.println("Clear 7 segment Displays");

  
  // s7dis.addressChange(0x71, 0x04);

  //Brightness is stored in non-volatile memory on the 7 segment displays, so it only needs to be called once.


  s7dis.setBrightness(0x01, 0xff);
  s7dis.setBrightness(0x02, 0xff);
  s7dis.setBrightness(0x03, 0xff);
  s7dis.setBrightness(0x71, 0xff);


  //Radio Initialization
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
    Serial.println("Datagram Init OK");
  }

  if (!rf69.setFrequency(RF69_FREQ)) {
    Serial.println("setFrequency failed");
  } else {
    Serial.println("Frequency Set OK");
  }

  // If you are using a high power RF69 eg RFM69HW, you *must* set a Tx power with the
  // ishighpowermodule flag set like this:
  rf69.setTxPower(POWER, true);  // 2nd arg must be true for 69HCW
  Serial.print("Radio power set to: "); Serial.println(POWER);
  Serial.print("RFM69 radio @"); Serial.print((int)RF69_FREQ); Serial.println(" MHz");

  rf69.setModemRegisters(&Table);

  //End Radio Initialization

   if(WDT.begin(5500)) {
    Serial.print("WDT interval: "); Serial.print(WDT.getTimeout()); Serial.println(" ms");
  } else {
    Serial.println("Error initializing watchdog");
      while(1){}
  }


  Serial.println("Setup Complete");

  Serial.end();

  RadioTX();

}


void loop() {
  if (millis() - radio_timer > 2000){
    RadioTX();
    transmit_counter++;
  }

  //Call the radio receive function.
  if (!rf69_manager.available()) {
    if(transmit_counter > 2){
      s7dis.clearDisplayI2C(0x01);
      s7dis.clearDisplayI2C(0x02);
      s7dis.clearDisplayI2C(0x03);
      s7dis.clearDisplayI2C(0x71);
      while(transmit_counter > 2){
        for (int counter = 0; counter <= 9; counter++) {
          _14display.print(message[counter]);
          //Gotta call the radio again or else we'll be stuck in this loop
          RadioTX();
          if (rf69_manager.waitAvailableTimeout(1000)){
            RadioCopy();
            display_data();
            break;
          }
          WDT.refresh(); //refresh the watchdog
        }
      }
    }  
  }else{
    RadioCopy();
    display_data();
  }


  if(blink==false && LEDstatus==true && millis()-blink_interval>40){
    digitalWrite(LED, LOW);
    LEDstatus=false;
  }

  if (blink==true && LEDstatus==false){
    digitalWrite(LED, HIGH);
    blink=false;
    LEDstatus=true;
    blink_interval=millis();
  }

  WDT.refresh();
}



