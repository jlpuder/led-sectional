#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP_WiFiManager.h>
#include <FastLED.h>
#include <vector>
using namespace std;

#define FASTLED_ESP8266_RAW_PIN_ORDER

#define NUM_AIRPORTS 100 // This is really the number of LEDs
#define WIND_THRESHOLD 25 // Maximum windspeed for green, otherwise the LED turns yellow
#define LOOP_INTERVAL 5000 // ms - interval between brightness updates and lightning strikes
#define DO_LIGHTNING true // Lightning uses more power, but is cool.
#define DO_WINDS true // color LEDs for high winds
#define REQUEST_INTERVAL 900000 // How often we update. In practice LOOP_INTERVAL is added. In ms (15 min is 900000)

String Router_SSID;
String Router_Pass;

String AP_SSID;
String AP_PASS;

// Define the array of leds
CRGB leds[NUM_AIRPORTS];
#define DATA_PIN    5 // Kits shipped after March 1, 2019 should use 14. Earlier kits us 5.
#define BRIGHTNESS_PIN A0
#define TRIGGER_PIN D3
#define LED_TYPE    WS2811
#define COLOR_ORDER RGB
#define BRIGHTNESS 30 // 20-30 recommended. If using a light sensor, this is the initial brightness on boot.

/* ----------------------------------------------------------------------- */

std::vector<unsigned short int> lightningLeds;
std::vector<String> airports({
  "KSGF",
  "NULL",
  "KHFJ",
  "NULL",
  "KJLN",
  "NULL",
  "KPTS",
  "NULL",
  "KMIO",
  "KGMJ",
  "NULL",
  "KXNA",
  "KVBT",
  "KROG",
  "KASG",
  "KSLG",
  "NULL",
  "KH71",
  "KGCM",
  "KTUL",
  "KOWP",
  "NULL",
  "KBVO",
  "NULL",
  "KIDP",
  "KCFV",
  "KPPF",
  "KCNU",
  "KK88",
  "NULL",
  "K13K",
  "NULL",
  "KEQA",
  "NULL",
  "NULL",
  "KEMP",
  "KUKL",
  "NULL",
  "KOWI",
  "NULL",
  "KIXD",
  "KOJC",
  "NULL",
  "KLWC",
  "KTOP",
  "KFOE",
  "NULL",
  "NULL",
  "KFRI",
  "KMHK",
  "KMYZ",
  "NULL",
  "NULL",
  "KFNB",
  "NULL",
  "KSTJ",
  "NULL",
  "KEZZ",
  "KGPH",
  "KMCI",
  "KMKC",
  "KLXT",
  "KLRY",
  "NULL",
  "NULL",
  "KRAW",
  "NULL",
  "KSZL",
  "KDMO",
  "NULL",
  "KVER",
  "NULL",
  "NULL",
  "NULL",
  "KIRK",
  "NULL",
  "NULL",
  "NULL",
  "KMYJ",
  "NULL",
  "KCOU",
  "KJEF",
  "NULL",
  "KVIH",
  "NULL",
  "KAIZ",
  "KOZS",
  "NULL",
  "KTBN",
  "NULL",
  "NULL",
  "KUNO",
  "NULL",
  "KBPK",
  "KFLP",
  "NULL",
  "KHRO",
  "KBBG",
  "KFWB",
  "NULL"
});

#define READ_TIMEOUT 15 // Cancel query if no data received (seconds)
#define WIFI_TIMEOUT 60000 // in ms
#define RETRY_TIMEOUT 15000 // in ms

#define SERVER "www.aviationweather.gov"
#define BASE_URI "/adds/dataserver_current/httpparam?dataSource=metars&requestType=retrieve&format=xml&hoursBeforeNow=3&mostRecentForEachStation=true&stationString="

boolean ledStatus = true; // used so leds only indicate connection status on first boot, or after failure
int loops = -1;
int timer = 0;
int loopTimer = 0;
int requestTimer = 0;


int status = WL_IDLE_STATUS;

void configModeCallback (ESP_WiFiManager *myESP_WiFiManager)
{
  Serial.print("Entered config mode with ");
  Serial.println("AP_SSID : " + myESP_WiFiManager->getConfigPortalSSID() + " and AP_PASS = " + myESP_WiFiManager->getConfigPortalPW());
  fill_rainbow(leds, NUM_AIRPORTS, CRGB::Blue); // Just let us know we're running
  FastLED.show();
}

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(74880);

  pinMode(LED_BUILTIN, OUTPUT); // give us control of the onboard LED
  pinMode(0, INPUT_PULLUP); //Flash button for config portal
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(BRIGHTNESS_PIN, INPUT);

  // Initialize LEDs
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_AIRPORTS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  ESP_WiFiManager wifiManager("WeatherMap");
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setDebugOutput(true);
  wifiManager.setMinimumSignalQuality(-1);

  Router_SSID = wifiManager.WiFi_SSID();
  Router_Pass = wifiManager.WiFi_Pass();

  if (Router_SSID != "") {
    wifiManager.setConfigPortalTimeout(60);
    Serial.println("Stored credentials received - set timeout to 60s.");
    fill_solid(leds, NUM_AIRPORTS, CRGB::Orange);
    FastLED.show();
  } else {
    Serial.println("No stored credentials found - no portal timeout.");
  }

  AP_SSID = "WeatherMap";
  AP_PASS = "KCweather";

  if (!wifiManager.autoConnect(AP_SSID.c_str(), AP_PASS.c_str())) {
    Serial.println("Failed to connect and timeout expired, resetting...");
    ESP.reset();
    delay(1000);    
  }

  Serial.println("WiFi connected");
  digitalWrite(LED_BUILTIN, LOW); // on if we're awake
  fill_solid(leds, NUM_AIRPORTS, CRGB::Purple); // indicate status with LEDs
  FastLED.show();
  ledStatus = false;
}

void adjustBrightness() {
  byte brightness;
  float reading;

  reading = analogRead(BRIGHTNESS_PIN);

  Serial.print("Light reading: ");
  Serial.print(reading);
  Serial.print(" raw, ");

  brightness = reading * (90.0 / 1024.0) + 10.0;
  
  Serial.print(brightness);
  Serial.println(" brightness");
  FastLED.setBrightness(brightness);
  FastLED.show();
}

void loop() {
  int c;
  loops++;
  timer = millis();
  
  //Call Config Portal if Requested
  if ((digitalRead(TRIGGER_PIN) == LOW))
  {
    Serial.println("\nConfiguration portal requested.");
    digitalWrite(LED_BUILTIN, LOW); // turn the LED on by making the voltage LOW to tell us we are in configuration mode.
    fill_rainbow(leds, NUM_AIRPORTS, CRGB::Blue); // Just let us know we're running
    FastLED.show();

    //Local intialization. Once its business is done, there is no need to keep it around
    ESP_WiFiManager wifiManager;

    //Check if there is stored WiFi router/password credentials.
    //If not found, device will remain in configuration mode until switched off via webserver.
    Serial.print("Opening configuration portal. ");
    Router_SSID = wifiManager.WiFi_SSID();
    if (Router_SSID != "")
    {
      wifiManager.setConfigPortalTimeout(60); //If no access point name has been previously entered disable timeout.
      Serial.println("Got stored Credentials. Timeout 60s");
    }
    else
      Serial.println("No stored Credentials. No timeout");

    //it starts an access point
    //and goes into a blocking loop awaiting configuration
    if (!wifiManager.startConfigPortal((const char *) AP_SSID.c_str(), AP_PASS.c_str()))
    {
      Serial.println("Not connected to WiFi but continuing anyway.");
    }
    else
    {
      Serial.println("Config successful");
      fill_solid(leds, NUM_AIRPORTS, CRGB::Purple); // indicate status with LEDs
      FastLED.show();
      loops = 0;
    }

    digitalWrite(LED_BUILTIN, HIGH); // Turn led off as we are not in configuration mode.
  }

  // Connect to WiFi. We always want a wifi connection for the ESP8266
  if (WiFi.status() != WL_CONNECTED && timer % WIFI_TIMEOUT == 0) {
    if (ledStatus) fill_solid(leds, NUM_AIRPORTS, CRGB::Orange); // indicate status with LEDs, but only on first run or error
    FastLED.show();
    WiFi.mode(WIFI_STA);
    //wifi_set_sleep_type(LIGHT_SLEEP_T); // use light sleep mode for all delays
    Serial.print("WiFi connecting..");
    WiFi.begin(Router_SSID, Router_Pass);
    Serial.println("WiFi Connection Lost... trying again in 60s");
    FastLED.show();
    ledStatus = true;
  } else if (WiFi.status() == WL_CONNECTED && ledStatus) {
    Serial.println("OK!");
    if (ledStatus) fill_solid(leds, NUM_AIRPORTS, CRGB::Purple); // indicate status with LEDs
    FastLED.show();
    ledStatus = false;
    loops = 0;
  }

  // Do some lightning / adjust brightness
  if (timer >= loopTimer + LOOP_INTERVAL) {
    if (DO_LIGHTNING && lightningLeds.size() > 0) {
      std::vector<CRGB> lightning(lightningLeds.size());
      for (unsigned short int i = 0; i < lightningLeds.size(); ++i) {
        unsigned short int currentLed = lightningLeds[i];
        lightning[i] = leds[currentLed]; // temporarily store original color
        leds[currentLed] = CRGB::White; // set to white briefly
        Serial.print("Lightning on LED: ");
        Serial.println(currentLed);
      }
      delay(25); // extra delay seems necessary with light sensor
      FastLED.show();
      delay(25);
      for (unsigned short int i = 0; i < lightningLeds.size(); ++i) {
        unsigned short int currentLed = lightningLeds[i];
        leds[currentLed] = lightning[i]; // restore original color
      }
      FastLED.show();
    }
    adjustBrightness();
    loopTimer = timer;
  }

  if (timer >= requestTimer + REQUEST_INTERVAL || loops == 0) {
    
    Serial.println("Getting METARs ...");
    if (getMetars()) {
      Serial.println("Refreshing LEDs.");
      FastLED.show();
      if ((DO_LIGHTNING && lightningLeds.size() > 0)) {
        Serial.println("There is lightning, so no long sleep.");
        digitalWrite(LED_BUILTIN, HIGH);
        //delay(LOOP_INTERVAL); // pause during the interval
      } else {
        Serial.print("No lightning; Going into sleep for: ");
        Serial.println(REQUEST_INTERVAL);
        digitalWrite(LED_BUILTIN, HIGH);
        //delay(REQUEST_INTERVAL);
      }
    } else {
      digitalWrite(LED_BUILTIN, HIGH);
    }
    requestTimer = timer;
  }
}

bool getMetars(){
  lightningLeds.clear(); // clear out existing lightning LEDs since they're global
  fill_solid(leds, NUM_AIRPORTS, CRGB::Black); // Set everything to black just in case there is no report
  uint32_t t;
  char c;
  boolean readingAirport = false;
  boolean readingCondition = false;
  boolean readingWind = false;
  boolean readingGusts = false;
  boolean readingWxstring = false;

  std::vector<unsigned short int> led;
  String currentAirport = "";
  String currentCondition = "";
  String currentLine = "";
  String currentWind = "";
  String currentGusts = "";
  String currentWxstring = "";
  String airportString = "";
  bool firstAirport = true;
  for (int i = 0; i < NUM_AIRPORTS; i++) {
    if (airports[i] != "NULL" && airports[i] != "VFR" && airports[i] != "MVFR" && airports[i] != "WVFR" && airports[i] != "IFR" && airports[i] != "LIFR") {
      if (firstAirport) {
        firstAirport = false;
        airportString = airports[i];
      } else airportString = airportString + "," + airports[i];
    }
  }

  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  Serial.println("\nStarting connection to server...");
  // if you get a connection, report back via serial:
  if (!client.connect(SERVER, 443)) {
    Serial.println("Connection failed!");
    client.stop();
    return false;
  } else {
    Serial.println("Connected ...");
    Serial.print("GET ");
    Serial.print(BASE_URI);
    Serial.print(airportString);
    Serial.println(" HTTP/1.1");
    Serial.print("Host: ");
    Serial.println(SERVER);
    Serial.println("Connection: close");
    Serial.println();
    // Make a HTTP request, and print it to console:
    client.print("GET ");
    client.print(BASE_URI);
    client.print(airportString);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(SERVER);
    client.println("Connection: close");
    client.println();
    client.flush();
    t = millis(); // start time
    FastLED.clear();

    Serial.print("Getting data");

    while (!client.connected()) {
      if ((millis() - t) >= (READ_TIMEOUT * 1000)) {
        Serial.println("---Timeout---");
        client.stop();
        return false;
      }
      Serial.print(".");
      delay(1000);
    }

    Serial.println();

    while (client.connected()) {
      if ((c = client.read()) >= 0) {
        yield(); // Otherwise the WiFi stack can crash
        currentLine += c;
        if (c == '\n') currentLine = "";
        if (currentLine.endsWith("<station_id>")) { // start paying attention
          if (!led.empty()) { // we assume we are recording results at each change in airport
            for (vector<unsigned short int>::iterator it = led.begin(); it != led.end(); ++it) {
              doColor(currentAirport, *it, currentWind.toInt(), currentGusts.toInt(), currentCondition, currentWxstring);
            }
            led.clear();
          }
          currentAirport = ""; // Reset everything when the airport changes
          readingAirport = true;
          currentCondition = "";
          currentWind = "";
          currentGusts = "";
          currentWxstring = "";
        } else if (readingAirport) {
          if (!currentLine.endsWith("<")) {
            currentAirport += c;
          } else {
            readingAirport = false;
            for (unsigned short int i = 0; i < NUM_AIRPORTS; i++) {
              if (airports[i] == currentAirport) {
                led.push_back(i);
              }
            }
          }
        } else if (currentLine.endsWith("<wind_speed_kt>")) {
          readingWind = true;
        } else if (readingWind) {
          if (!currentLine.endsWith("<")) {
            currentWind += c;
          } else {
            readingWind = false;
          }
        } else if (currentLine.endsWith("<wind_gust_kt>")) {
          readingGusts = true;
        } else if (readingGusts) {
          if (!currentLine.endsWith("<")) {
            currentGusts += c;
          } else {
            readingGusts = false;
          }
        } else if (currentLine.endsWith("<flight_category>")) {
          readingCondition = true;
        } else if (readingCondition) {
          if (!currentLine.endsWith("<")) {
            currentCondition += c;
          } else {
            readingCondition = false;
          }
        } else if (currentLine.endsWith("<wx_string>")) {
          readingWxstring = true;
        } else if (readingWxstring) {
          if (!currentLine.endsWith("<")) {
            currentWxstring += c;
          } else {
            readingWxstring = false;
          }
        }
        t = millis(); // Reset timeout clock
      } else if ((millis() - t) >= (READ_TIMEOUT * 1000)) {
        Serial.println("---Timeout---");
        fill_solid(leds, NUM_AIRPORTS, CRGB::Cyan); // indicate status with LEDs
        FastLED.show();
        ledStatus = true;
        client.stop();
        return false;
      }
    }
  }
  // need to doColor this for the last airport
  for (vector<unsigned short int>::iterator it = led.begin(); it != led.end(); ++it) {
    doColor(currentAirport, *it, currentWind.toInt(), currentGusts.toInt(), currentCondition, currentWxstring);
  }
  led.clear();

  // Do the key LEDs now if they exist
  for (int i = 0; i < (NUM_AIRPORTS); i++) {
    // Use this opportunity to set colors for LEDs in our key then build the request string
    if (airports[i] == "VFR") leds[i] = CRGB::Green;
    else if (airports[i] == "WVFR") leds[i] = CRGB::Yellow;
    else if (airports[i] == "MVFR") leds[i] = CRGB::Blue;
    else if (airports[i] == "IFR") leds[i] = CRGB::Red;
    else if (airports[i] == "LIFR") leds[i] = CRGB::Magenta;
  }

  client.stop();
  return true;
}

void doColor(String identifier, unsigned short int led, int wind, int gusts, String condition, String wxstring) {
  CRGB color;
  Serial.print(identifier);
  Serial.print(": ");
  Serial.print(condition);
  Serial.print(" ");
  Serial.print(wind);
  Serial.print("G");
  Serial.print(gusts);
  Serial.print("kts LED ");
  Serial.print(led);
  Serial.print(" WX: ");
  Serial.println(wxstring);
  if (wxstring.indexOf("TS") != -1) {
    Serial.println("... found lightning!");
    lightningLeds.push_back(led);
  }
  if (condition == "LIFR" || identifier == "LIFR") color = CRGB::Magenta;
  else if (condition == "IFR") color = CRGB::Red;
  else if (condition == "MVFR") color = CRGB::Blue;
  else if (condition == "VFR") {
    if ((wind > WIND_THRESHOLD || gusts > WIND_THRESHOLD) && DO_WINDS) {
      color = CRGB::Yellow;
    } else {
      color = CRGB::Green;
    }
  } else color = CRGB::Black;

  leds[led] = color;
}
