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

  // Set sensor settings for integration time, step, and gain
  as7341.setATIME(100);  // 100 * 2.78 ms = ~278 ms integration time
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
  uint16_t readings[12];

  // Turn on the excitation LED
  digitalWrite(EXCITATION_LED, HIGH);

  // Read all spectral channels from AS7341
  if (!as7341.readAllChannels(readings)) {
    Serial.println("Error reading AS7341 channels!");
    digitalWrite(EXCITATION_LED, LOW); // Turn off the LED on error
    return;
  }

  // Turn off the excitation LED after reading
  digitalWrite(EXCITATION_LED, LOW);

  // Extract the red (680 nm) value (Channel 5)
  uint16_t redValue = readings[5];
  Serial.print("Red (680 nm) value: ");
  Serial.println(redValue);

  // Calculate the estimated chlorophyll concentration using the formula
  float chlorophyllConcentration = (redValue - 10.723) / 3.4571;

  // Check if the chlorophyll concentration is negative or zero
  if (chlorophyllConcentration <= 0) {
    chlorophyllConcentration = 0;
  }

  Serial.print("Estimated Chlorophyll Concentration: ");
  Serial.print(chlorophyllConcentration, 2);
  Serial.println(" µg/L");

  // Read GPS data
  String gpsData = readGPS();
  Serial.print("GPS Data: ");
  Serial.println(gpsData);

  // Prepare data for transmission in the desired format with authentication key
  String dataToSend = AUTH_KEY + "RED:" + String(redValue) + 
                      ",CHL:" + String(chlorophyllConcentration, 2) + 
                      ",LAT:" + String(gps.location.lat(), 8) + 
                      ",LON:" + String(gps.location.lng(), 8);

  // Send data via LoRa
  LoRa.beginPacket();
  LoRa.print(dataToSend);
  LoRa.endPacket();

  Serial.println("Data sent via LoRa: " + dataToSend);

  delay(1500); // Adjust as needed
}

String readGPS() {
  String gpsString = "";
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
    if (gps.location.isUpdated()) {
      gpsString = "Lat: " + String(gps.location.lat(), 8) + 
                  ", Lon: " + String(gps.location.lng(), 8);
    }
  }
  if (gpsString == "") {
    gpsString = "0";
  }
  return gpsString;
}
