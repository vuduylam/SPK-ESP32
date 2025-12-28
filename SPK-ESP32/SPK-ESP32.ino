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
void getGps(float& latitude, float& longitude);
float getDistance(float latitude1, float longtitude1, float latitude2, float longtitude2);

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
String phoneNumber = "";
bool antiTheftEnabled = false;
unsigned long ms = 0;

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

  Database.get(streamClient, "/examples/Stream/data", processData, true, "streamTask");
}

void loop() {
  // To maintain the authentication and async tasks
  app.loop();

  if (millis() - ms > 20000 && app.ready()) {
    ms = millis();

    JsonWriter writer;

    object_t json, obj1, obj2;

    writer.create(obj1, "ms", ms);
    writer.create(obj2, "rand", random(10000, 30000));
    writer.join(json, 2, obj1, obj2);

    Database.set<object_t>(aClient, "/examples/Stream/data", json, processData, "setTask");
    
    Serial.println();
    Serial.println("=========== Get data from Firebase ===========");
    getFirebaseData();
    Serial.println("=============== End get data =================");
    Serial.println();
    sendSms("hello from esp32");
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
    SERIAL_MONITOR.print("Get phone number stastus: ");
    check_and_print_value(phoneNumber);
    antiTheftEnabled = Database.get<bool>(aClient, "/examples/antiTheftEnabled");
    SERIAL_MONITOR.print("Get antiTheftEnabled flag stastus: ");
    if (antiTheftEnabled){
      SERIAL_MONITOR.println("On");
    }
    else {
      SERIAL_MONITOR.println("Off");
    }
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

void initModem() {
  // Resetting the modem
  #if defined(SIM_MODEM_RST)
      pinMode(SIM_MODEM_RST, SIM_MODEM_RST_LOW ? OUTPUT_OPEN_DRAIN : OUTPUT);
      digitalWrite(SIM_MODEM_RST, SIM_MODEM_RST_LOW);
      delay(100);
      digitalWrite(SIM_MODEM_RST, !SIM_MODEM_RST_LOW);
      delay(3000);
      digitalWrite(SIM_MODEM_RST, SIM_MODEM_RST_LOW);
  #endif

  DBG("Wait...");
  delay(3000);

  DBG("Initializing modem...");
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
  modem.setNetworkMode(2);
  if (modem.waitResponse(10000L) != 1) {
      DBG(" setNetworkMode faill");
  }

  String name = modem.getModemName();
  DBG("Modem Name:", name);

  String modemInfo = modem.getModemInfo();
  DBG("Modem Info:", modemInfo);

  SERIAL_MONITOR.print("Waiting for network...");
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
