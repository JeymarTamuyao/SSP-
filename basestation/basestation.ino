#define BLYNK_TEMPLATE_ID "TMPL6N9IMfUMm"    
#define BLYNK_TEMPLATE_NAME "SSPproject"  
#define BLYNK_AUTH_TOKEN "nuskxDkwl8aIP8Gf7ClQo3t_y9vzvsDV"  

#include <SPI.h>
#include <LoRa.h>
#include <U8g2lib.h>
#include <WiFiManager.h>
#include <FirebaseESP32.h>
#include <BlynkSimpleEsp32.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// LoRa SX1278 Pins
#define SCK 18
#define MISO 19
#define MOSI 23
#define LORA_CS 5
#define LORA_RST 14
#define LORA_DIO0 2
#define LORA_BAND 433E6

const String AUTH_KEY = "Chlorophyll"; 

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

const char* FIREBASE_HOST = "https://chl-a-buoy-default-rtdb.firebaseio.com/";
const char* FIREBASE_AUTH = "Osz2FZqvawdpcOw5ir7nTbnGiSnbcBqwdEcV9OT9";  

FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

char auth[] = BLYNK_AUTH_TOKEN;

String red_data = "0", chl_data = "0", latitude = "0.0", longitude = "0.0";
String googleMapsLink = "";
String riskstatus = "";

void setup() {
    Serial.begin(115200);

    u8g2.begin();
    displayMessage("Initializing...");

    SPI.begin(SCK, MISO, MOSI);

    // Initialize LoRa
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    if (!LoRa.begin(LORA_BAND)) {
        displayMessage("LoRa Failed!");
        while (1);
    }
    displayMessage("LoRa initialized");

    WiFiManager wifiManager;
    wifiManager.autoConnect("AutoConnectAP");

    if (WiFi.status() != WL_CONNECTED) {
        displayMessage("WiFi Failed!");
    } else {
        displayMessage("WiFi initialized");

        firebaseConfig.host = FIREBASE_HOST;
        firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
        Firebase.begin(&firebaseConfig, &firebaseAuth);

        if (Firebase.ready()) {
            displayMessage("Firebase initialized");
        } else {
            displayMessage("Firebase Failed!");
        }

        Blynk.begin(auth, WiFi.SSID().c_str(), WiFi.psk().c_str());
        displayMessage("Blynk initialized");

        timeClient.begin();
        timeClient.update();
    }
}

void loop() {
    Blynk.run();

    timeClient.update();

    int packetSize = LoRa.parsePacket();
    if (packetSize) {
        String receivedData;
        while (LoRa.available()) {
            receivedData += (char)LoRa.read();
        }

        if (receivedData.startsWith(AUTH_KEY)) {
            receivedData = receivedData.substring(AUTH_KEY.length());

            red_data = extractValue(receivedData, "ChlorophyllRED:", ",");
            chl_data = extractValue(receivedData, "CHL:", ",");
            latitude = extractValue(receivedData, "LAT:", ",");
            longitude = extractValue(receivedData, "LON:", "");

            googleMapsLink = generateGoogleMapsLink(latitude, longitude);
            riskstatus = determineRiskStatus(chl_data.toFloat());

            displayData();

            if (WiFi.status() == WL_CONNECTED) {
                sendToFirebase();
                sendToBlynk();
            }
        } else {
            Serial.println("Unauthorized data received and discarded.");
        }
    }
}

void displayData() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    int xRed = (128 - u8g2.getStrWidth(("RED: " + red_data).c_str())) / 2;
    int xChl = (128 - u8g2.getStrWidth(("CHL: " + chl_data).c_str())) / 2;
    int xLat = (128 - u8g2.getStrWidth(("Lat: " + latitude).c_str())) / 2;
    int xLon = (128 - u8g2.getStrWidth(("Lon: " + longitude).c_str())) / 2;
    u8g2.drawStr(xRed, 15, ("RED: " + red_data).c_str());
    u8g2.drawStr(xChl, 30, ("CHL: " + chl_data).c_str());
    u8g2.drawStr(xLat, 45, ("Lat: " + latitude).c_str());
    u8g2.drawStr(xLon, 60, ("Lon: " + longitude).c_str());
    u8g2.sendBuffer();
}

void displayMessage(String message) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setCursor((128 - u8g2.getStrWidth(message.c_str())) / 2, 30);
    u8g2.print(message);
    u8g2.sendBuffer();
    delay(1500);
}

String extractValue(String data, String key, String delimiter) {
    int startIndex = data.indexOf(key) + key.length();
    int endIndex = (delimiter != "") ? data.indexOf(delimiter, startIndex) : data.length();
    return data.substring(startIndex, endIndex);
}

String generateGoogleMapsLink(String lat, String lon) {
    return "https://www.google.com/maps?q=" + lat + "," + lon;
}

String determineRiskStatus(float chlConcentration) {
    if (chlConcentration <= 5) {
        return "Very Low Risk";
    } else if (chlConcentration <= 10) {
        return "Low Risk";
    } else if (chlConcentration <= 50) {
        return "Moderate Risk";
    } else if (chlConcentration >= 100) {
        return "High Risk";
    }
    return "Unknown Risk";  // Added default return value
}

void sendToFirebase() {
    FirebaseJson jsonPayload;
    jsonPayload.set("red", red_data);
    jsonPayload.set("chlorophyll", chl_data);
    jsonPayload.set("latitude", latitude);
    jsonPayload.set("longitude", longitude);
    jsonPayload.set("timestamp", timeClient.getFormattedTime());

    if (!Firebase.setJSON(firebaseData, "/buoy_data", jsonPayload)) {
        Serial.println("Firebase set failed: " + firebaseData.errorReason());
    } else {
        Serial.println("Firebase data updated successfully.");
    }
}

void sendToBlynk() {
    Blynk.virtualWrite(V0, red_data);
    Blynk.virtualWrite(V1, chl_data);
    Blynk.virtualWrite(V2, latitude);
    Blynk.virtualWrite(V3, longitude);
    Blynk.virtualWrite(V4, googleMapsLink);
    Blynk.virtualWrite(V5, riskstatus);

    Serial.println("Data sent to Blynk:");
    Serial.println("Red data: " + red_data);
    Serial.println("Chlorophyll data: " + chl_data);
    Serial.println("Latitude: " + latitude);
    Serial.println("Longitude: " + longitude);
    Serial.println("Google Maps Link: " + googleMapsLink);
    Serial.println("Risk Status: " + riskstatus);
}
