#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>

const char SSID[]     = SECRET_SSID;    // Network SSID (name)
const char PASS[]     = SECRET_OPTIONAL_PASS;    // Network password (use for WPA, or use as key for WEP)


float netpress;
float netspeed;
int netdirection;
int nethumid;
int nettemp;

void initProperties(){

  ArduinoCloud.addProperty(netpress, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(netspeed, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(netdirection, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(nethumid, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(nettemp, READ, ON_CHANGE, NULL);

}

WiFiConnectionHandler ArduinoIoTPreferredConnection(SSID, PASS);
