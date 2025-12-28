#include <TinyGPSPlus.h>

//================GPS================
#define GPS_RX_PIN 14
#define GPS_TX_PIN 15
#define GPS_BAUD 9600

//==============Serial===============
#define SERIAL_MONITOR Serial
#define SERIAL_GPS Serial1

//================ ================
void getGps(float& latitude, float& longitude);
float getDistance(float latitude1, float longtitude1, float latitude2, float longtitude2);

//================ ================
TinyGPSPlus gps;

void setup() {
  SERIAL_MONITOR.begin(115200);
  SERIAL_GPS.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
}

void loop() {

}

void getGps(float& latitude, float& longitude) {
  boolean newData = false;
  for (unsigned long start = millis(); millis() - start < 1000;){
    while (SERIAL_GPS.available()){
      if (gps.encode(SERIAL_GPS.read()))
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

float getDistance(float latitude1, float longtitude1, float latitude2, float longtitude2) {
  // Variables
  float distCalc=0;
  float distCalc2=0;
  float difLatiude=0;
  float difLongtitude=0;

  // Calculations
  difLatiude  = radians(latitude2-latitude1);
  latitude1 = radians(latitude1);
  latitude2 = radians(latitude2);
  difLongtitude = radians((longtitude2)-(longtitude1));

  distCalc = (sin(difLatiude/2.0)*sin(difLatiude/2.0));
  distCalc2 = cos(latitude1);
  distCalc2*=cos(latitude2);
  distCalc2*=sin(difLongtitude/2.0);
  distCalc2*=sin(difLongtitude/2.0);
  distCalc +=distCalc2;

  distCalc=(2*atan2(sqrt(distCalc),sqrt(1.0-distCalc)));
  
  distCalc*=6371000.0; //Converting to meters

  return distCalc;
}
