#include <Adafruit_AS7341.h>   // For AS7341 sensor
#include <TinyGPS++.h>         // For GPS module
#include <HardwareSerial.h>
#include <LoRa.h>              // For LoRa communication

// AS7341 sensor
Adafruit_AS7341 as7341;

// GPS module
TinyGPSPlus gps;
HardwareSerial gpsSerial(1); // Use UART1 for GPS
#define RXD2 16              // GPS RX pin
#define TXD2 17              // GPS TX pin

// LoRa SX1278
#define SCK 18
#define MISO 19
#define MOSI 23
#define SS 5
#define RST 14
#define DIO0 2

// LoRa frequency (433 MHz)
#define LORA_BAND 433E6

// Authentication key
const String AUTH_KEY = "Chlorophyll";  // Replace with your unique authentication key

// Excitation LED
#define EXCITATION_LED 13  // GPIO pin for excitation LED

void setup() {
  Serial.begin(115200);

  // Initialize I2C for AS7341
  if (!as7341.begin()) {
    Serial.println("AS7341 sensor not detected!");
    while (1);
  }
  Serial.println("AS7341 sensor initialized.");

  // Set sensor settings
  as7341.setATIME(150);  // Integration time (150 * 2.78 ms ≈ 417 ms)
  as7341.setASTEP(999);  // Integration step
  as7341.setGain(AS7341_GAIN_256X);

  // Initialize GPS
  gpsSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.println("GPS module initialized.");
  
  // Initialize LoRa
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("LoRa initialization failed!");
    while (1);
  }
  Serial.println("LoRa initialized.");

  // Initialize excitation LED
  pinMode(EXCITATION_LED, OUTPUT);
  digitalWrite(EXCITATION_LED, LOW); // Start with the LED off
}

void loop() {
  uint16_t readings[11];         // Store fluorescence readings
  uint16_t ambientReadings[11];  // Store ambient light readings

  // **Step 1: Measure ambient light (LED OFF)**
  digitalWrite(EXCITATION_LED, LOW);
  delay(1500);
  if (!as7341.readAllChannels(ambientReadings)) {
    Serial.println("Error reading ambient light!");
    return;
  }

  // **Step 2: Measure fluorescence (LED ON)**
  digitalWrite(EXCITATION_LED, HIGH);
  delay(1500);
  if (!as7341.readAllChannels(readings)) {
    digitalWrite(EXCITATION_LED, LOW);
    Serial.println("Error reading fluorescence!");
    return;
  }
  digitalWrite(EXCITATION_LED, LOW);

  // **Step 3: Corrected red fluorescence (680 nm, Channel 7)**
  uint16_t rawRed = readings[7];               // Fluorescence with LED
  uint16_t ambientRed = ambientReadings[7];    // Ambient interference
  uint16_t correctedRed = (rawRed > ambientRed) ? (rawRed - ambientRed) : 0;

  Serial.print("Corrected Red (680 nm): ");
  Serial.println(correctedRed);

  // **Step 4: Chlorophyll estimation**
  float chlorophyllConcentration = (correctedRed - 1.88) / 0.484;
  if (chlorophyllConcentration < 0) chlorophyllConcentration = 0;

  Serial.print("Estimated Chlorophyll: ");
  Serial.print(chlorophyllConcentration, 2);
  Serial.println(" µg/L");

  // **Step 5: Read GPS Data**
  String gpsData = readGPS();
  Serial.println("GPS Data: " + gpsData);

  // **Step 6: Prepare Data for LoRa Transmission**
  String dataToSend = AUTH_KEY + "RED:" + String(correctedRed) + 
                      ",CHL:" + String(chlorophyllConcentration, 2) + 
                      ",LAT:" + String(gps.location.lat(), 8) + 
                      ",LON:" + String(gps.location.lng(), 8);

  // **Step 7: Send Data via LoRa**
  LoRa.beginPacket();
  LoRa.print(dataToSend);
  LoRa.endPacket();
  Serial.println("Data sent via LoRa: " + dataToSend);

  delay(1500); // Adjust as needed
}

// **Optimized GPS Reading Function**
String readGPS() {
  String gpsString = "0";  // Default to "0" if no update
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
    if (gps.location.isUpdated()) {
      gpsString = "Lat:" + String(gps.location.lat(), 8) + 
                  ",Lon:" + String(gps.location.lng(), 8);
    }
  }
  return gpsString;
}
