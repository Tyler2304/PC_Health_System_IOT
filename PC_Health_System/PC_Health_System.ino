#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#include <WiFiS3.h>      
#include <ThingSpeak.h> 
#include <DHT.h> 
#include <ArduinoBLE.h> 

const char* ssid = "BT-X6CM6W";        
const char* pass = "EpN6Ar9cpXaDvc";   
unsigned long myChannelNumber = 3229297;    
const char * myWriteAPIKey = "R9AZNFQZOXNYUT9P";   

#define BLACK   0x0000
#define WHITE   0xFFFF
#define CYAN    0x07FF
#define RED     0x001F
#define GREEN   0x07E0

//Register the library for the temp sensor and OLED display
Adafruit_SSD1351 OLED = Adafruit_SSD1351(128, 128, &SPI, 10, 8, 9);
WiFiClient  client;
DHT dht(4, DHT11);

BLEService pcHealthService("19B10000-E8F2-537E-4F6C-D104768A1214"); 
BLEStringCharacteristic statusChar("19B10001-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify, 20);

const int sampleWindow = 50; 
unsigned int sample;
unsigned long lastConnectionTime = 0;             
const unsigned long postingInterval = 20L * 1000L; 

//Set values for the sensor modules
int micLevel = 0;
int vibrationState = 0;
int lastVibrationState = -1;
int smokeLevel = 0;
float temp = 0;
String status = "Safe";

//Fixing the mirrored OLED display
void writeCommand(uint8_t c) {
  digitalWrite(8, LOW);
  digitalWrite(10, LOW);
  SPI.transfer(c);
  digitalWrite(10, HIGH);
}
void writeData(uint8_t d) {
  digitalWrite(8, HIGH);
  digitalWrite(10, LOW);
  SPI.transfer(d);
  digitalWrite(10, HIGH);
}
void forceMirrorFix() {
  writeCommand(0xA0);
  writeData(0x77); 
}

void setup() {
  Serial.begin(9600);
  pinMode(2, INPUT); 
  dht.begin(); 
  OLED.begin();
  forceMirrorFix();
  OLED.fillScreen(BLACK);
  OLED.setCursor(32, 10);
  OLED.setTextColor(WHITE);
  OLED.setTextSize(1);
  OLED.println("Init WiFi & BLE...");
  WiFi.begin(ssid, pass); 
  ThingSpeak.begin(client);
  OLED.fillScreen(BLACK);
  OLED.setCursor(32, 5); 
  OLED.setTextColor(CYAN);
  OLED.setTextSize(1);   
  OLED.println("PC HEALTH"); 
  OLED.drawFastHLine(0, 18, 128, WHITE);
  OLED.drawFastHLine(0, 120, 128, WHITE);

 // Initiates the BLE Signal to detect on nrf scanner app
  BLE.begin();
  BLE.setLocalName("PC Health Monitor");
  BLE.setAdvertisedService(pcHealthService);
  pcHealthService.addCharacteristic(statusChar);
  BLE.addService(pcHealthService);
  
  statusChar.writeValue("System OK");
  BLE.advertise();
  Serial.println("BLE device active");

  
  OLED.setTextColor(WHITE);
  OLED.setCursor(32, 25); OLED.println("Noise:");
  OLED.setCursor(32, 50); OLED.println("Vib:");   
  OLED.setCursor(32, 75); OLED.println("Smoke:");
  OLED.setCursor(32, 100); OLED.println("Temp:");
}

// Loops the main sections of code to constantly update BLE, Thingspeak and the OLED display
void loop() {
  BLE.poll();
  micLevel = analogRead(A0);
  vibrationState = digitalRead(2);
  smokeLevel = analogRead(A1); 
  temp = dht.readTemperature();
  String newStatus = "System OK";

  // Causes the new updates in BLE
  if (smokeLevel > 600) {
    newStatus = "DANGER: SMOKE!";
  } else if (temp > 75) {
    newStatus = "ALERT: HIGH TEMP";
  }

  //  Updates BLE
  if (status != newStatus) {
    status = newStatus;
    statusChar.writeValue(status);
    Serial.print("BLE Update: ");
    Serial.println(status);
  }
  
  // Microphone settings
  OLED.fillRect(70, 25, 50, 20, BLACK); 
  OLED.setCursor(70, 25);
  OLED.setTextSize(1); 
  OLED.setTextColor(GREEN);
  OLED.print(micLevel);

  // Vibration Level Detection
  if(vibrationState != lastVibrationState) {
    OLED.fillRect(60, 50, 60, 15, BLACK); 
    OLED.setCursor(60, 50);
    OLED.setTextSize(1);
    if(vibrationState == HIGH) { OLED.setTextColor(RED); OLED.print("WARNING!"); } 
    else { OLED.setTextColor(GREEN); OLED.print("STABLE"); }
    lastVibrationState = vibrationState;
  }

  // Smoke Level Detection
  OLED.fillRect(70, 75, 60, 15, BLACK); 
  OLED.setCursor(70, 75);
  OLED.setTextSize(1);
  if(smokeLevel > 600) { OLED.setTextColor(RED); OLED.print("FIRE?"); } 
  else { OLED.setTextColor(GREEN); OLED.print("SAFE"); }

  // Temperature Levels
  OLED.fillRect(70, 100, 60, 15, BLACK);
  OLED.setCursor(70, 100);
  OLED.setTextSize(1);
  if (temp > 75) {OLED.setTextColor(RED); OLED.print(temp,1);}
  else {OLED.setTextColor(GREEN); OLED.print(temp,1);}
  
  // Uploads the metrics to ThingSpeak
  if (millis() - lastConnectionTime > postingInterval) {
    ThingSpeak.setField(1, micLevel);
    ThingSpeak.setField(2, vibrationState);
    ThingSpeak.setField(3, smokeLevel);
    ThingSpeak.setField(4, temp);      
    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    lastConnectionTime = millis(); 
  }
}