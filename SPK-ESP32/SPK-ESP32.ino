#include <TinyGPSPlus.h>

//================GPS================
#define GPS_RX_PIN
#define GPS_TX_PIN
#define GPS_BAUD 9600

//==============Serial===============
#define SERIAL_MONITOR Serial
#define SERIAL_GPS Serial1

//================ ================
void getGps(float& latitude, float& longitude);

//================ ================
TinyGPSPlus gps;

void setup() {
  SERIAL_MONITOR.begin(115200);
  SERIAL_GPS.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
}

void loop() {

}

void getGps(float& latitude, float& longitude){
  boolean newData = false;
  for (unsigned long start = millis(); millis() - start < 1000;){
    while (neogps.available()){
      if (gps.encode(neogps.read()))
        if (gps.location.isValid()){
          newData = true;
          break;
        }
    }
  }
  
  if (newData) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
    newData = false;
  }
  else {
    Serial.println("No GPS data is available");
  }
}
