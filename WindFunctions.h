#ifndef WindFunctions_h
#define WindFunctions_h
#include <Arduino.h>



//Read the wind speed from serial. Must input an address
class WindFunctions{
	public:
	//Variables
	int16_t WindDirection;
	int16_t WindSpeed;
	
	//Functions
	int16_t readWindSpeed(uint8_t A_Address);
	int16_t readWindDirection(uint8_t B_Address);
	boolean ModifyAddress(uint8_t Address1, uint8_t Address2);


	
	private:
	//Variables
	// uint8_t ModifyAddressCOM[11];
	// uint8_t Data[7];
	// uint8_t COM[8];
	// boolean ret;
	// long curr;
	// long curr1;
	// uint8_t ch;
	// uint16_t crc;
	// uint8_t *buf;
	// int pos;
	// int16_t Tineout;
	// uint8_t *buffer;
	// size_t left;
	// size_t offset;


	
	
	//Functions
	size_t readN(uint8_t *buf, size_t len);
	uint16_t CRC16_2(uint8_t *buf, int16_t len);
	void addedCRC(uint8_t *buf, int len);
	


};

#endif