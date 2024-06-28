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
const char* identifier = "0001";

// ON INSTALL: WIFI HERE
const char* ssid = "Apex";  // TW - TODO: Input WiFi credentials
const char* password = "PasswordThe";

// Addresses for POST and GET servers
const char* serverNamePOST = "https://fgnbnmlckc.execute-api.us-east-2.amazonaws.com/prod/post-data";
const char* serverNameGET = "https://h6clwj7ppl.execute-api.us-east-2.amazonaws.com/prod/settings";

// Sets timer vars
unsigned long lastTime = 0;
unsigned long timerDelay = 60000;  // SET TO 60 SECONDS

// Set alert var
bool alertDetected;

// JSON buffer
JsonDocument doc;

// Global int var
float prefTemp;

File myFile;

void setup() {
  // Begins serial
  Serial.begin(9600);

  // Begins DHT
  dht.begin();

  // Sets vars
  alertDetected = false;

  // ----Begin WIFI----
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  // ----END WIFI----

  // ----Begin GET Request for settings----
  WiFiClientSecure client;
  client.setInsecure();  // Not using certificate check while testing

  HTTPClient https;
  https.useHTTP10(true);
  Serial.println("https.begin...");
  if (https.begin(client, serverNameGET)) {  // HTTPS
    Serial.println("Sending GET request...");
    https.addHeader("ESP32-device", identifier);

    int httpCode = https.GET();

    Serial.printf("Response code: %u\n", httpCode);
    Serial.printf("Content length: %u\n", https.getSize());
    String json = https.getString();

    DeserializationError error = deserializeJson(doc, json);

    // Parse JSON
    const char* bodyJson = doc["body"];
    if (bodyJson == nullptr) {
      Serial.println("Retrieval failed");
      return;
    }
    StaticJsonDocument<1024> bodyDoc;
    DeserializationError bodyError = deserializeJson(bodyDoc, bodyJson);

    // Access the preferred temperature value
    prefTemp = bodyDoc[identifier]["temperature"];
    Serial.print("User Set Preferred Temperature: ");
    Serial.println(prefTemp);

    // Free resource
    https.end();
  } else {
    Serial.println("Could not connect to server");
  }
  // ----END GET Request for settings----

  // ----Begin SD Init----
  if (!SD.begin(5)) {
    Serial.println("SD initialization failed!");
    return;
  }
  Serial.println("SD initialization done.");
  // ----END SD Init----

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

void loop() {
  // Check if needs to alert while waiting to read temp again
  if(alertDetected) {
    blink(5);
  }

  // Check once a minute
  if ((millis() - lastTime) > timerDelay) {
    // Read humidity
    float h = dht.readHumidity();
    // Read Temperature
    float t = dht.readTemperature();
    // Read Temperature as Fahrenheit
    float f = dht.readTemperature(true);

    // Check for failures
    if (isnan(h) || isnan(t) || isnan(f)) {
      Serial.println(F("DHT FAILURE TO MEASURE"));
      blink(1000000000000);
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

    // Check if need to alert
    if (f > prefTemp) {
      alertDetected = true;
      Serial.println("ALERT");
    } else {
      alertDetected = false;
    }

    // -----Log to Local Storage------
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
    lastTime = millis();
    // -----END Log to Local Storage------

    // -----Log to Database------
    WiFiClientSecure client;
    client.setInsecure();

    // Concat post string
    String postStr = "{\"id\":\"";
    postStr += identifier;
    postStr += "\",\"tfi\":\"";
    postStr += String(millis());
    postStr += "\",\"temperature\":\"";
    postStr += String(f, 2);
    postStr += "\",\"humidity\":\"";
    postStr += h;
    postStr += "\",\"alert\":\"";
    postStr += alertDetected;
    postStr += "\"}";

    // Begin Client Send
    HTTPClient https;
    https.useHTTP10(true);
    if (https.begin(client, serverNamePOST)) {  // HTTPS
      Serial.println("Sending POST request...");
      https.addHeader("Content-Type", "application/json");
      int httpResponseCode = https.POST(postStr);
      Serial.print("POST Response code: ");
      Serial.println(httpResponseCode);
      Serial.print("JSON sent: ");
      Serial.println(postStr);

      // Free resource
      https.end();
    } else {
      // If it can't connect
      Serial.println("Error with POST: Could not connect");
    }
    // -----END Log to Database------

    // Blink to LED to confirm
    blink(1);
  }
}