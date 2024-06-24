#include "DHT.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#define DHTPIN 15      // Digital Pin connected to DHT sensor
#define DHTTYPE DHT22  // Currently using DHT 22

#define LEDPIN 4

DHT dht(DHTPIN, DHTTYPE);

// ON INSTALL: ADD IDENTFIER HERE
const char* identifier = "id2";

// ON INSTALL: WIFI HERE
const char* ssid = "";  // TW - TODO: Input WiFi credentials
const char* password = "";

//Your Domain name with URL path or IP address with path
const char* serverNamePOST = "http://192.168.0.212:5000/temperature_post";
const char* serverNameGET = "https://h6clwj7ppl.execute-api.us-east-2.amazonaws.com/dev/settings";

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
unsigned long lastTimeSD = 0;
unsigned long timerDelay = 3600000;  // SET TO ONE HOUR
unsigned long timerDelaySD = 60000;  // SET TO ONE SECOND

// JSON buffer
JsonDocument doc;

// Global int var
float prefTemp;

File myFile;

void setup() {
  Serial.begin(9600);

  dht.begin();

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 seconds before publishing the first reading.");

  // sets settings
  WiFiClientSecure client;
  client.setInsecure();  // Not using certificate check while testing

  HTTPClient https;
  https.useHTTP10(true);
  Serial.println("https.begin...");
  if (https.begin(client, serverNameGET)) {  // HTTPS
    Serial.println("Sending GET request...");
    https.addHeader("X-device", "12345678");
    int httpCode = https.GET();
    Serial.printf("Response code: %u\n", httpCode);
    Serial.printf("Content length: %u\n", https.getSize());
    Serial.println("HTTP response:");
    //Serial.println(https.getString());
    String json = https.getString();
    Serial.println(json);

    DeserializationError error = deserializeJson(doc, json);

    // Parse JSON
    const char* bodyJson = doc["body"];
    if (bodyJson == nullptr) {
        Serial.println("Retrieval failed");
        return;
    }
    StaticJsonDocument<1024> bodyDoc; 
    DeserializationError bodyError = deserializeJson(bodyDoc, bodyJson);

    // Access the temperature value
    prefTemp = bodyDoc[identifier]["temperature"];
    Serial.println(prefTemp);

    https.end();
  } else {
    Serial.println("Could not connect to server");
  }


  if (!SD.begin(5)) {
    Serial.println("SD initialization failed!");
    return;
  }
  Serial.println("SD initialization done.");

  // Set LED pin
  pinMode(LEDPIN, OUTPUT);
}

// Blink function for LED
void blink(int count) {
  for (int i = 0; i < count; i++) {
    digitalWrite(LEDPIN, HIGH);
    delay(50);
    digitalWrite(LEDPIN, LOW);
    delay(50);
  }
}

// Alert function
void alert() {
  //***************
  // SMS CODE HERE
  //***************
  digitalWrite(LEDPIN, HIGH);
  delay(1000);
  digitalWrite(LEDPIN, LOW);
  Serial.println("ALERT");
}

void loop() {
  /*---DHT11 sensor processes---*/

  // Take measurements & send every 3 seconds
  delay(1000);

  // Read humidity
  float h = dht.readHumidity();
  // Read Temperature
  float t = dht.readTemperature();
  // Read Temperature as Fahrenheit
  float f = dht.readTemperature(true);

  // Check for failures
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("DHT FAILURE TO MEASURE"));
    blink(100000000);
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C "));
  Serial.print(f);
  Serial.print(F("°F  Heat index: "));
  Serial.print(hic);
  Serial.print(F("°C "));
  Serial.print(hif);
  Serial.println(F("°F"));

  // TW - testing String convert
  Serial.println("Fahrenheit to string embedded: " + String(f, 2));

  // Check if need to alert
  if (f > prefTemp) {
    alert();
  }

  // SD log
  if ((millis() - lastTimeSD) > timerDelaySD) {
    // Read contents
    myFile = SD.open("/DATA.LOG", FILE_READ);
    String currFile = myFile.readString();
    myFile.close();  // close the file:
    // Write contents
    myFile = SD.open("/DATA.LOG", FILE_WRITE);
    if (myFile) {
      myFile.print(currFile);
      myFile.print(millis());
      myFile.print(F("ms "));
      myFile.print(F("Humidity: "));
      myFile.print(h);
      myFile.print(F("%  Temperature: "));
      myFile.print(t);
      myFile.print(F("°C "));
      myFile.print(f);
      myFile.print(F("°F  Heat index: "));
      myFile.print(hic);
      myFile.print(F("°C "));
      myFile.print(hif);
      myFile.println(F("°F"));
      myFile.close();  // close the file:
      Serial.println("completed SD write");
    } else {
      Serial.println("ERROR WITH SD");
    }
    lastTimeSD = millis();
    blink(1);
  }

  /*---ESP32 WiFi / HTTP processes---*/

  //Send an HTTP POST request full of data from the sd card every hour.
  if ((millis() - lastTime) > timerDelay) {
    //Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      HTTPClient http;

      // Your Domain name with URL path or IP address with path
      http.begin(client, serverNamePOST);

      // If you need Node-RED/server authentication, insert user and password below
      //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");

      // Specify content-type header
      http.addHeader("Content-Type", "application/json");

      // Only POST if Temperature is above danger threshold
      if (f > prefTemp) {
        // Data to send with HTTP POST
        //String httpRequestData = "{\"Temperature\":\"String(f, 2)\"}";
        String httpRequestData = "{\"Temperature\":";
        httpRequestData = httpRequestData += String(f, 2) + "}";
        Serial.print(httpRequestData);
        // Send HTTP POST request
        int httpResponseCode = http.POST(httpRequestData);

        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
      }

      // Free resources
      http.end();

    } else {
      Serial.println("WiFi Disconnected");
    }
    lastTime = millis();
    blink(5);
  }
}
