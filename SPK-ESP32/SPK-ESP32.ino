#include <TinyGPSPlus.h>
#include <SSLClient.h>
#include <WiFiClientSecure.h>

//================GPS================
#define GPS_RX_PIN 34
#define GPS_TX_PIN 35
#define GPS_BAUD 9600


//==============Serial===============
#define SERIAL_MONITOR Serial
#define SERIAL_GPS Serial1

#define WIFI_SSID "Phuongthao_2.4G"
#define WIFI_PASSWORD "xitrum25032002"

//==============Firebase===============
#define API_KEY "AIzaSyBuLticw5nMQeC2KftZkPhg2k5bJfa8K48"
#define USER_EMAIL "vuduylam250302@gmail.com"
#define USER_PASSWORD "123456"
#define DATABASE_URL "https://test-a2978-default-rtdb.asia-southeast1.firebasedatabase.app/"

//================ ================
void getGps(float& latitude, float& longitude);
float getDistance(float latitude1, float longtitude1, float latitude2, float longtitude2);
void processData(AsyncResult &aResult);

//================ ================
TinyGPSPlus gps;

SSL_CLIENT ssl_client;

using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);

UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD, 3000);
FirebaseApp app;
RealtimeDatabase Database;
AsyncResult databaseResult;

//================ ================

void setup() {
  SERIAL_MONITOR.begin(115200);
  SERIAL_GPS.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);

  set_ssl_client_insecure_and_buffer(ssl_client);

  Serial.println("Initializing app...");
  initializeApp(aClient, app, getAuth(user_auth), auth_debug_print, "🔐 authTask");

  app.getApp<RealtimeDatabase>(Database);

  Database.url(DATABASE_URL);
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
