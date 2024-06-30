#include <Wire.h>                           // Include the Arduino I2C library
#include <SPI.h>                            //Serial Peripheral interface. This is used for the radio
#include <RH_RF69.h>                        //Radiohead Library for the receiver
#include <SparkFun_Alphanumeric_Display.h>  //The 14 segment alphanumeric display library
#include <s7s.h>
#include <RHReliableDatagram.h>
#include "WiFiS3.h"
#include "arduino_secrets.h"
#include <WDT.h>

#define SERVER_ADDRESS 1 //Radio server address
#define MY_ADDRESS 2 // This device's address

#define RF69_FREQ 915.0  //915mhz for USA and Canada
#define POWER 2
#define RFM69_CS 4   // Radio pin for Chip Select
#define RFM69_INT 3  // radio pin for IRQ interrupt.
#define RFM69_RST 2  // Radio pin for radio reset
#define LED 8        // The blinky light LED so that we know that data is being received.


s7s s7dis;  // calling an instance of the seven segment display

// initialize instance of the radio driver
RH_RF69 rf69(RFM69_CS, RFM69_INT);
RHReliableDatagram rf69_manager(rf69, MY_ADDRESS);

//Defines the timer so that we know how long it's been since the last transmission
long current_time;

//14 Segment display object for the library functions
HT16K33 _14display;

//Received data structure
struct TPHData {
  int16_t temperature = 9;
  int16_t pressure = 9999;
  int16_t humidity = 99;
  int16_t speed = 9;
  int16_t direction = 18;
};

TPHData Bundle;  // Make a variable for the structure above and call it 'Bundle'

//The displayed directions correspond to this list.
char *_direction_list[] = { "   N", " NNE", "  NE", " ENE", "   E", " ESE", "  SE", " SSE", "   S", " SSW", "  SW", " WSW", "   W", " WNW", "  NW", " NNW", "   N", "FAIL", "INIT", "STBY" };

char tempString[10];  // Will be used with sprintf to create strings
char pressString[10];
char humidString[10];
char speedString[10];

char ssid[] = SECRET_SSID;  // your network SSID (name)
char pass[] = SECRET_PASS;  // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;
WiFiServer server(80);
IPAddress ip(192, 168, 1, 200);

//Creates a class of functions for operating the radio
class RadioFunctions {
public:

  void Radio() {
    uint8_t radiopacket[] = "ON";
    // Send a message to the DESTINATION!
    if (rf69_manager.sendtoWait(radiopacket, sizeof(radiopacket), SERVER_ADDRESS)) {
      // Now wait for a reply from the server
      current_time = millis();
      if (rf69_manager.recvfromAckTimeout((uint8_t *)&Bundle, &datalen, 2000, &from)) {
        Blink(LED, 30, 2);  // blink LED 3 times, 40ms between blinks
      }
    }
  }


private:
  uint8_t datalen = sizeof(Bundle);
  uint8_t from;
  //This defines the function for the blinky LED light
  void Blink(byte pin, byte delay_ms, byte loops) {
    while (loops--) {
      digitalWrite(pin, HIGH);
      delay(delay_ms);
      digitalWrite(pin, LOW);
      delay(delay_ms);
    }
  }
};


RadioFunctions RF;  //Creates Radio Functions object

//setup function
void setup() {
  Serial.begin(9600);
  
  Serial.println("Serial Begin...");

  Wire.begin();  // Initialize hardware I2C pins

  Serial.println("Wire I2C library begin");

  // Clear the 7 segment displays before jumping into loop
  s7dis.clearDisplayI2C(0x01);
  s7dis.clearDisplayI2C(0x02);
  s7dis.clearDisplayI2C(0x03);
  s7dis.clearDisplayI2C(0x71);

  Serial.println("Clear 7 segment Displays");

  //Brightness is stored in non-volatile memory on the 7 segment displays, so it only needs to be called once.

  // s7dis.addressChange(0x71, 0x04);

  // s7dis.setBrightness(0x01, 75);
  // s7dis.setBrightness(0x02, 75);
  // s7dis.setBrightness(0x03, 75);
  // s7dis.setBrightness(0x71, 75);



  //Initializes the 14 segment display at its default address.
  if (_14display.begin(0x70) == false) {
    Serial.println("14 Segment display did not acknowledge! Freezing.");
  } else {
    Serial.println("Start 14 seg display");
  }



  //The 14 segment display does not rely on non-volatile memory for it's brightness setting, therefore it has to be set each startup.
  if (_14display.setBrightness(1)) {
    Serial.println("Set 14 seg display brightness");
  } else {
    Serial.println("14 seg display brightness failed");
  }



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
    Serial.println("Reliable datagram manager initialization failed");
  } else {
    Serial.println("Reliable datagram Init OK");
  }

  if (!rf69.setFrequency(RF69_FREQ)) {
    Serial.println("setFrequency failed");
  } else {
    Serial.println("Frequency Set OK");
  }

  // If you are using a high power RF69 eg RFM69HW, you *must* set a Tx power with the
  // ishighpowermodule flag set like this:
  rf69.setTxPower(POWER, true);  // 2nd arg must be true for 69HCW
  Serial.print("Radio power set to: ");
  Serial.println(POWER);
  // The encryption key has to be the same as the one in the server
  uint8_t key[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
  rf69.setEncryptionKey(key);

  Serial.println("Encryption key: ");

  Serial.print("RFM69 radio @");
  Serial.print((int)RF69_FREQ);
  Serial.println(" MHz");

  //End Radio Initialization

  //Wifi Initialization
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true)
      ;
  }
  _14display.print("CFG");
  WiFi.config(ip);

  // attempt to connect to WiFi network:

  _14display.print("WIFI");
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(5000);
  }
  server.begin();
  // you're connected now, so print out the status:
  printWifiStatus();

   if(WDT.begin(5500)) {
    Serial.print("WDT interval: ");
    WDT.refresh();
    Serial.print(WDT.getTimeout());
    WDT.refresh();
    Serial.println(" ms");
    WDT.refresh();
  } else {
    Serial.println("Error initializing watchdog");
    while(1){}
  }


  current_time = millis();

  Serial.println("Setup Complete");
}


void loop() {
  //Call the radio receive function
  RF.Radio();

  //Call the 7 segment displays.
  //7 Segment displays need to have the strings formatted to display properly.
  sprintf(tempString, "%3dF", Bundle.temperature);
  sprintf(pressString, "%4d", Bundle.pressure);
  sprintf(humidString, "%3d-", Bundle.humidity);
  sprintf(speedString, "%4d", Bundle.speed);

  // Input string to send to displays, the display address, and the decimal point configuration as a binary sequence.
  s7dis.Transmit(tempString, 0b00100000, 0x01);
  s7dis.Transmit(pressString, 0b00000010, 0x02);
  s7dis.Transmit(humidString, 0b00101000, 0x03);
  s7dis.Transmit(speedString, 0b00000100, 0x71);

  //14 segment display transmission. I only need one, so the default address 0x70 works fine.

  _14display.print(_direction_list[Bundle.direction]);

  while (millis() - current_time >= 5000) {
    s7dis.clearDisplayI2C(0x01);
    s7dis.clearDisplayI2C(0x02);
    s7dis.clearDisplayI2C(0x03);
    s7dis.clearDisplayI2C(0x71);
    char *message[] = { "NO S", "O SI", " SIG", "SIGN", "IGNA", "GNAL", "NAL ", "AL N", "L NO", " NO " };
    for (int counter = 0; counter <= 9; counter++) {
        if (millis() - current_time < 5000) {
        break;
      }
      _14display.print(message[counter]);
      WDT.refresh(); //refresh the watchdog
      //Gotta call the radio receive again or else we'll be stuck in this loop for the next 5 seconds or so.
      RF.Radio();

    }
  }

  WebServer();
  WDT.refresh();
  delay(2000);

}


//Send the data to a webpage
void WebServer() {

  float floatpressure=Bundle.pressure/100.0;
  float floatspeed=Bundle.speed/10.0;
  // listen for incoming clients
  WiFiClient client = server.available();
  if (client) {
    // an HTTP request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the HTTP request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard HTTP response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 8");         // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          client.println("<head>");
          client.println("<title>Home Weather</title>");
          client.println("</head>");
          // output the value of each sensor
          client.println("<body>");
          client.print("Temperature: "); client.print(Bundle.temperature); client.println("F");
          client.println("<br>");
          client.print("Pressure: "); client.println(floatpressure); client.println(" InHG");
          client.println("<br>");
          client.print("Humidity: "); client.println(Bundle.humidity); client.println(" %rH");
          client.println("<br>");
          client.print("Direction: "); client.println(_direction_list[Bundle.direction]);
          client.println("<br>");
          client.print("Speed: "); client.println(floatspeed); client.println(" MPH");
          client.println("<br>");
          client.println("</body>");
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);

    // close the connection:
    client.stop();
  }
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
