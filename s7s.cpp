#include <s7s.h>
#include <Arduino.h>
#include <Wire.h>


void s7s::Transmit(String toSend, byte decimals, byte address) {

	Wire.beginTransmission(address);
		for (int i = 0; i < 4; i++) {
		  Wire.write(toSend[i]);
		}
		Wire.endTransmission();

		setDecimals(decimals, address);

}

void s7s::setDecimals(byte decimals, byte address) {
	Wire.beginTransmission(address);
    Wire.write(0x77);
    Wire.write(decimals);
    Wire.endTransmission();
}

void s7s::clearDisplayI2C(byte address) {
    Wire.beginTransmission(address);
    Wire.write(0x76);  // Clear display command
    Wire.endTransmission();
}

void s7s::setBrightness(byte address, byte brightness) {
	Wire.beginTransmission(address);
	Wire.write(0x7A); // Brightness control command
	Wire.write(brightness); // Set brightness level: 0% to 100%
	Wire.endTransmission();


}

void s7s::addressChange(byte oldAddress, byte newAddress) {
	
	Wire.beginTransmission(oldAddress);
	Wire.write('v');
	Wire.endTransmission();

	//Send change address command
	Wire.beginTransmission(oldAddress); // transmit to device #1
	Wire.write(0x80); //Send TWI address change command
	Wire.write(newAddress); //New I2C address to use
	Wire.endTransmission(); //Stop I2C transmission

	Serial.println("I2C address changed to:"); Serial.print(newAddress);

	//Now we talk at this new address

	//Send the reset command to the display - this forces the cursor to return to the beginning of the display
	Wire.beginTransmission(newAddress);
	Wire.write('v');
	Wire.endTransmission();
	
}