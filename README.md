# WeatherStation
Home Made Arduino Weather Station
Weather Measurement and Wireless Reporting Device

Custom Libraries Needed:

WindFunctions: https://github.com/wilson-malone/RS485_Wind_Direction_Speed_Sensors_Arduino


Objective: To provide a means of measuring the parameters of atmospheric temperature, humidity, pressure, wind direction, and wind speed. The parameters are then to be transmitted wirelessly to a receiver and display unit.


Introduction

Weather data obtained online often does not have the localized detail or time resolution that is useful for accurate measurement. 

Power considerations: 

The transmitter and sensors have been measured by a multimeter to use an average of 90mA over time. The transmitter spikes to 150mA during transmissions, however the average remains lower. The shortest day-length in the intended region is 8 hours, constituting 1/3 of the 24 hour cycle. The solar panel average output must therefore be at least 300mA over the course of a day to run the transmitter and charge the battery sufficiently. Solar panels rated at 1A will be preferred to account for inefficiencies resulting from cloud cover and obstacles. The battery will be 12 volts lead acid, chosen for its stable chemistry and suitability to deep discharging. To run through the night, the transmitter will use 1.44Ah of energy. Running a battery at or near its capacity is undesirable and reduces battery life, therefore, a battery with a significantly larger capacity will be used.


Components

The anemometer and wind direction sensor use RS485 serial communication limited at 9600baud. The transmitter uses a daughter board designed to interface with the microcontroller’s hardware serial system and convert to the necessary protocol. The rate of sensor readings is limited by the speed of this communication style to once every 3 seconds. These two sensors require 7-24 volts to operate, and consume 15mA each. This power will be provided by a direct connection to the solar charge controller.

The receiver uses four 7-segment displays for displaying temperature, pressure, humidity, and wind speed. A 14 segment alphanumeric display is used to display wind direction as one of the 16 cardinal directions. The displays are serial type displays, utilizing the I2C protocol. This was chosen for the few number of wires required to communicate with these displays. Only two wires, SDA, and SCL are required to communicate with all five displays which are wired in parallel. Each display can be programmed with a unique two-digit hexadecimal address. When transmitting a command to the display, the address is called out, along with the command.

The temperature, pressure and humidity (PHT) sensor sensor utilizes I2C communication, chosen for the relatively few wires that it requires. It operates at 3.3v DC, and will be powered using the Arduino 3.3v pin.

The radio transceivers use SPI communication. They operate in the range of 868 to 915 MHZ, corresponding to the American ISM band that does not requires a license at this power level. 

Microcontrollers

The microcontrollers will be Arduino units, chosen for their built-in power regulation and widespread compatibility with many types of hardware.

An Uno3 utilizing will be used for the server unit (sensor unit), and an Uno 4 will be used for the client (display unit).

Networking

The two units will communicate over 915mhz utilizing frequency shift key modulation. The data will be sent as a data structure consisting of 16 bit signed integers for each of the sensor parameters. The client unit will also have Wi-Fi connectivity, and will act as a web server for local networked computers to access the sensor data over HTTP.

Error reporting

Both units have basic error reporting. The server unit can give sensor fail warnings by transmitting specific numbers in place of sensor readings. The PHT sensor functions can determine if the unit is not responding, and will display the number ‘8’ in place of its sensor readings. If the wind direction sensor is not responding within 1000ms, the function will return an ‘18’ which displays as “FAIL’ on the alphanumeric display. If the anemometer does not respond in 1000ms, the function returns a ‘-1.’ The client unit displays ‘INIT’ when it sends a request to the server on first startup. The client unit has a function where it informs of the absence of a returned signal. The client counts the number of milliseconds between radio transmissions. If the gap between transmissions exceeds 5 seconds, the alphanumeric display will show ‘NO SIGNAL’ in scrolling text, and the other displays will turn off. A blinking  LED light on both transceivers will inform when a transmission has been received. The server unit has a power conserving standby mode, where if it has not received a request in 10 seconds, the radio will listen in 2 second intervals for incoming packets. The outgoing data structure will be set to display ‘7’ in all the categories, and the wind direction will send ‘19’ which displays ‘STBY’ on the client unit. 
