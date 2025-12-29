#define ENABLE_USER_AUTH
#define ENABLE_DATABASE
#define ENABLE_GSM_NETWORK
#define ENABLE_ESP_SSLCLIENT

#define TINY_GSM_MODEM_SIM7600

#include <Arduino.h>
#include <FirebaseClient.h>
#include <TinyGPSPlus.h>
#include <TinyGsmClient.h>

//================GPS================
#define GPS_RX_PIN 34
#define GPS_TX_PIN 35
#define GPS_BAUD 9600

//==============Modem===============
#define SIM_MODEM_BAUD 115200
#define SIM_MODEM_RST 5
#define SIM_MODEM_RST_LOW true // active LOW
#define SIM_MODEM_RST_DELAY 200
#define SIM_MODEM_TX 26
#define SIM_MODEM_RX 27

//==============Serial===============
#define SERIAL_MONITOR Serial
#define SERIAL_GPS Serial1
#define SERIAL_AT Serial2

#define TINY_GSM_DEBUG SERIAL_MONITOR
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

//=========GPRS credentials, if any========
const char apn[] = "m3-world";
const char gprsUser[] = "";
const char gprsPass[] = "";

//==============Firebase===============
#define API_KEY "AIzaSyBuLticw5nMQeC2KftZkPhg2k5bJfa8K48"
#define USER_EMAIL "vuduylam250302@gmail.com"
#define USER_PASSWORD "123456"
#define DATABASE_URL "https://test-a2978-default-rtdb.asia-southeast1.firebasedatabase.app/"

//================Function declaration================
//GPS
void getGps(float& latitude, float& longitude, float& accuracy, int& year, int& month, int& day, int& hour, int& minute, int& second); 
float getDistance(float latitude1, float longitude1, float latitude2, float longitude2);

//Firebase
void getFirebaseData();
void processData(AsyncResult &aResult);
void auth_debug_print(AsyncResult &aResult);
void initModem();

//GSM
void sendSms(const char* message);
void callPhoneNumber();

//================ ================
TinyGPSPlus gps;

TinyGsm modem(SERIAL_AT);
TinyGsmClient gsm_client(modem, 0), stream_gsm_client(modem, 1);
ESP_SSLClient ssl_client, stream_ssl_client;

using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client), streamClient(stream_ssl_client);

UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD, 3000);
FirebaseApp app;
RealtimeDatabase Database;
AsyncResult streamResult;

//================Global Variable================
String deviceId = "02:AF:35:8C:12:D4";
String phoneNumber = "";
bool antiTheftEnabled = false;
bool crashDetected = false;
unsigned long ms = 0;

float latitude, longitude, accuracy;
int year, month, day;
int hour, minute, second;
String timestamp = "";

//===============================================
void setup() {
  //UART settings
  SERIAL_MONITOR.begin(115200);
  SERIAL_GPS.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  SERIAL_AT.begin(SIM_MODEM_BAUD, SERIAL_8N1, SIM_MODEM_RX, SIM_MODEM_TX);

  initModem();

  Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);

  ssl_client.setInsecure();
  ssl_client.setDebugLevel(1);
  ssl_client.setBufferSizes(2048 /* rx */, 1024 /* tx */);
  ssl_client.setClient(&gsm_client);

  stream_ssl_client.setInsecure();
  stream_ssl_client.setDebugLevel(1);
  stream_ssl_client.setBufferSizes(2048 /* rx */, 1024 /* tx */);
  stream_ssl_client.setClient(&stream_gsm_client);

  Serial.println("Initializing app...");
  initializeApp(aClient, app, getAuth(user_auth), auth_debug_print, "🔐 authTask");

  app.getApp<RealtimeDatabase>(Database);

  Database.url(DATABASE_URL);

  streamClient.setSSEFilters("get,put,patch,keep-alive,cancel,auth_revoked");

  Database.get(streamClient, "/Data", processData, true, "streamTask");
  //Database.get(streamClient, "/examples/Stream/data", processData, true, "streamTask");
}

void loop() {
  // To maintain the authentication and async tasks
  app.loop();
  if(millis() - ms > 20000) {
    ms = millis();
    SERIAL_MONITOR.println();
    SERIAL_MONITOR.println("=================================================================================");

    getGps(latitude, longitude, accuracy, year, month, day, hour, minute, second);

    if (app.ready()) {
      SERIAL_MONITOR.println("----------- Get data from Firebase -----------");
      getFirebaseData();
      JsonWriter writer;
      object_t json, locObj, crashObj, idObj, phoneObj, timeObj;

      object_t lat, lon;
      writer.create(lat, "latitude", String(latitude, 8));
      writer.create(lon, "longitude", String(longitude, 8));
      writer.join(locObj, 2, lat, lon);

      writer.create(crashObj, "crashDetected", crashDetected);
      writer.create(idObj, "deviceId", deviceId);
      writer.create(phoneObj, "phoneNumber", phoneNumber);
      writer.create(timeObj, "timestamp", timestamp);

      object_t locWrapper;
      writer.create(locWrapper, "location", locObj);

      writer.join(json, 5, crashObj, idObj, locWrapper, phoneObj, timeObj);

      Database.set<object_t>(aClient, "/Data", json, processData, "setTask");
    }

    SERIAL_MONITOR.println("=================================================================================");
  }
}

template <typename T>
void check_and_print_value(T value) {
    // To make sure that we actually get the result or error.
    if (aClient.lastError().code() == 0)
    {
        Serial.print("Success, Value: ");
        Serial.println(value);
    }
    else
        Firebase.printf("Error, msg: %s, code: %d\n", aClient.lastError().message().c_str(), aClient.lastError().code());
}

void getFirebaseData() {
    phoneNumber = Database.get<String>(aClient, "/examples/test");
    SERIAL_MONITOR.print("Get phone number status: ");
    check_and_print_value(phoneNumber);
    antiTheftEnabled = Database.get<bool>(aClient, "/examples/antiTheftEnabled");
    SERIAL_MONITOR.print("Get antiTheftEnabled flag status: ");
    if (antiTheftEnabled){
      SERIAL_MONITOR.println("On");
    }
    else {
      SERIAL_MONITOR.println("Off");
    }
}

void getGps(float& latitude, float& longitude, float& accuracy,
            int& year, int& month, int& day,
            int& hour, int& minute, int& second) {
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

  SERIAL_MONITOR.println();
  SERIAL_MONITOR.println("------------------- GPS Data -------------------");

  if (newData) {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
    accuracy = gps.hdop.isValid() ? gps.hdop.hdop() : -1;
    
    if (gps.date.isValid()) {
      year  = gps.date.year();
      month = gps.date.month();
      day   = gps.date.day();
    }

    if (gps.time.isValid()) {
      hour   = gps.time.hour();
      minute = gps.time.minute();
      second = gps.time.second();
    }

    timestamp = String(year) + "-" + String(month) + "-" + String(day) + " " +
                String(hour) + ":" + String(minute) + ":" + String(second);

    SERIAL_MONITOR.println("GPS OK:");
    SERIAL_MONITOR.print("latitude: "); SERIAL_MONITOR.print(latitude, 8);
    SERIAL_MONITOR.print("\tlongitude: "); SERIAL_MONITOR.println(longitude, 8);
    // SERIAL_MONITOR.print("Latitude: "); SERIAL_MONITOR.println(latitude, 8);
    // SERIAL_MONITOR.print("Longitude: "); SERIAL_MONITOR.println(longitude, 8);
    SERIAL_MONITOR.print("accuracy: "); SERIAL_MONITOR.println(accuracy);

    SERIAL_MONITOR.print("timestamp: "); SERIAL_MONITOR.println(timestamp);
     
    newData = false;
  }
  else {
    SERIAL_MONITOR.println("No GPS data is available. Trying to get LBS...");
    getLbs();
  } 
}

float getDistance(float latitude1, float longitude1, float latitude2, float longitude2) {
  // Variables
  float distCalc=0;
  float distCalc2=0;
  float difLatiude=0;
  float diflongitude=0;

  // Calculations
  difLatiude  = radians(latitude2-latitude1);
  latitude1 = radians(latitude1);
  latitude2 = radians(latitude2);
  diflongitude = radians((longitude2)-(longitude1));

  distCalc = (sin(difLatiude/2.0)*sin(difLatiude/2.0));
  distCalc2 = cos(latitude1);
  distCalc2*=cos(latitude2);
  distCalc2*=sin(diflongitude/2.0);
  distCalc2*=sin(diflongitude/2.0);
  distCalc +=distCalc2;

  distCalc=(2*atan2(sqrt(distCalc),sqrt(1.0-distCalc)));
  
  distCalc*=6371000.0; //Converting to meters

  return distCalc;
}

void initModem() {
  // Resetting the modem
  #if defined(SIM_MODEM_RST)
    SERIAL_MONITOR.println("Resetting modem...");
    pinMode(SIM_MODEM_RST, SIM_MODEM_RST_LOW ? OUTPUT_OPEN_DRAIN : OUTPUT);
    digitalWrite(SIM_MODEM_RST, SIM_MODEM_RST_LOW);
    delay(100);
    digitalWrite(SIM_MODEM_RST, !SIM_MODEM_RST_LOW);
    delay(3000);
    digitalWrite(SIM_MODEM_RST, SIM_MODEM_RST_LOW);
    SERIAL_MONITOR.println("Waiting for resetting modem...");
    delay(3000);  
  #endif

  SERIAL_MONITOR.println("Initializing modem...");
  while (!modem.init()) {
      SERIAL_MONITOR.println("Modem init failed! Retrying...");
      delay(2000);
  }

  SERIAL_MONITOR.println("Modem initialized successfully!");
  /**
    * 2 Automatic
    * 13 GSM Only
    * 14 WCDMA Only
    * 38 LTE Only
    */
  SERIAL_MONITOR.println("Setting Network Mode...");
  modem.setNetworkMode(2);
  if (modem.waitResponse(10000L) != 1) {
    SERIAL_MONITOR.println("Set Net workMode faill");
  }
  SERIAL_MONITOR.println("Setting Network Mode successfully");

  String name = modem.getModemName();
  SERIAL_MONITOR.print("Modem Name: "); SERIAL_MONITOR.println(name);

  String modemInfo = modem.getModemInfo();
  SERIAL_MONITOR.print("Modem Info: "); SERIAL_MONITOR.println(modemInfo);

  SERIAL_MONITOR.println("Waiting for network...");
  while (!modem.waitForNetwork()) {
      SERIAL_MONITOR.println("Get network failed! Retrying...");
      delay(2000);
  }
  SERIAL_MONITOR.println("Get network successfully!");

  //Connect GPRS
  SERIAL_MONITOR.print("Connecting to APN: ");
  SERIAL_MONITOR.println(apn);
  while (!modem.gprsConnect(apn, gprsUser, gprsPass)){
      SERIAL_MONITOR.println("GPRS connect failed");
      delay(2000);
  }
  SERIAL_MONITOR.println("GPRS connect successfully");

  if (modem.isNetworkConnected())
      SERIAL_MONITOR.println("Network connected");
}

void auth_debug_print(AsyncResult &aResult) {
    if (aResult.isEvent())
    {
        Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());
    }

    if (aResult.isDebug())
    {
        Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
    }

    if (aResult.isError())
    {
        Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
    }
}

void processData(AsyncResult &aResult) {
  // Exits when no result is available when calling from the loop.
  if (!aResult.isResult())
    return;

  if (aResult.isEvent()) {
    Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());
  }

  if (aResult.isDebug()) {
    Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
  }

  if (aResult.isError()) {
      Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
  }

  if (aResult.available()) {
    RealtimeDatabaseResult &stream = aResult.to<RealtimeDatabaseResult>();
    if (stream.isStream()) {
      Serial.println("----------------------------");
      Firebase.printf("task: %s\n", aResult.uid().c_str());
      Firebase.printf("event: %s\n", stream.event().c_str());
      Firebase.printf("path: %s\n", stream.dataPath().c_str());
      Firebase.printf("data: %s\n", stream.to<const char *>());
      Firebase.printf("type: %d\n", stream.type());

      // The stream event from RealtimeDatabaseResult can be converted to the values as following.
      bool v1 = stream.to<bool>();
      int v2 = stream.to<int>();
      float v3 = stream.to<float>();
      double v4 = stream.to<double>();
      String v5 = stream.to<String>();
    }
    else {
      Serial.println("----------------------------");
      Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
    }
    Firebase.printf("Free Heap: %d\n", ESP.getFreeHeap());
  }
}

void sendSms(const char* message) {
  bool resSMS = modem.sendSMS(phoneNumber.c_str(), message);
  SERIAL_MONITOR.print("SMS: ");
  SERIAL_MONITOR.println(resSMS ? "Ok" : "Failed");
}

void callPhoneNumber() {
  bool resCall = modem.callNumber(phoneNumber.c_str());
  SERIAL_MONITOR.print("Call:");
  if(resCall){
    SERIAL_MONITOR.println("Ok");
  }
  else{
    SERIAL_MONITOR.println("Failed");
  }
}

void getLbs()
{
  SERIAL_MONITOR.println();
  SERIAL_MONITOR.println("------------------ LBS Data ------------------");
  if (modem.getGsmLocation(&latitude, &longitude, &accuracy, &year, &month, &day, &hour, &minute, &second)) 
  {
    SERIAL_MONITOR.println("LBS OK:");
    SERIAL_MONITOR.print("latitude: "); SERIAL_MONITOR.print(latitude, 8);
    SERIAL_MONITOR.print("\tlongitude: "); SERIAL_MONITOR.println(longitude, 8);
    // SERIAL_MONITOR.print("Latitude: "); SERIAL_MONITOR.println(latitude, 8);
    // SERIAL_MONITOR.print("Longitude: "); SERIAL_MONITOR.println(longitude, 8);
    SERIAL_MONITOR.print("accuracy: "); SERIAL_MONITOR.println(accuracy);

    timestamp = String(year) + "-" + String(month) + "-" + String(day) + " " +
                String(hour) + ":" + String(minute) + ":" + String(second);
    SERIAL_MONITOR.print("timestamp: "); SERIAL_MONITOR.println(timestamp);
  } else {
    SERIAL_MONITOR.println("Couldn't get GSM location");
  }
}
