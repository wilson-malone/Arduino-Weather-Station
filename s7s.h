#ifndef s7s_h
#define s7s_h
#include <Arduino.h>

class s7s{
	
	public:
	
		void Transmit(String toSend, byte decimals, byte address);

		void setDecimals(byte decimals, byte address);

		void clearDisplayI2C(byte address);
		
		void setBrightness(byte address, byte brightness);
		
		void addressChange(byte oldAddress, byte newAddress);

};


#endif