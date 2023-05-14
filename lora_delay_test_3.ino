// Import library
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <SD.h>
#include <RTClib.h>
#include <MemoryFree.h>

// Pin definitions
const int alarmPin = 2;
const int trigPin = 4;
const int echoPin = 5;
const int LoRa_CS = 6;
const int LoRa_RST = 9;
const int LoRa_IRQ = 8;
const int LoRa_EN = 7;
const int SD_CS = 10;

// LoRa settings
#define RFM95_FREQ 915E6 // Set the frequency to 915 MHz

// RTC
RTC_DS3231 rtc;

//--------------ALARM INTERUPT--------------//
void alarmHandler() {
  // Do nothing, waking up is enough
  //Serial.println("Wake up!");
}

//--------------PRINT ALARM TIME--------------//
void printAlarmTime(DateTime alarmTime) {
  char buffer[30];
  sprintf(buffer, "%02d:%02d:%02d", alarmTime.hour(), alarmTime.minute(), alarmTime.second());
  Serial.print("Alarm set for: ");
  Serial.println(buffer);
}

//--------------SET CLOCK ALARM--------------//
void setRTCAlarm(int minutes = 1) {
  // Clear alarm1 flag
  rtc.clearAlarm(1);
  // Get the current time
  DateTime now = rtc.now();

  // Calculate the next alarm time, in the future by specified minutes
  DateTime alarmTime = now + TimeSpan(0, 1, 0, 0);

  // Set alarm1 with matching minutes
  if(rtc.setAlarm1(alarmTime, DS3231_A1_Minute)) {
    printAlarmTime(alarmTime);
  } else {
    Serial.println("Failed to set alarm.");
  }
}

//--------------READ WATER LEVEL--------------//
float readWaterLevel() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(20);
  digitalWrite(trigPin, LOW);

  float duration = pulseIn(echoPin, HIGH);
  float distance = duration * 0.0344 / 2;

  return distance;
}

//--------------SEND DATA LORA--------------//
void transmitDataViaLoRa(float waterLevel) {

  // Reset the LoRa radio
  digitalWrite(LoRa_RST, LOW);
  delay(10);
  digitalWrite(LoRa_RST, HIGH);
  delay(10);
  Serial.println("LoRa Reset");

  // Begin a new LoRa packet for transmission
  LoRa.beginPacket();
  Serial.println("Started a new packet");

  // Add the text "Water Level: " to the packet
  LoRa.print("Water Level: ");
  Serial.println("Added 'Water Level: ' to the packet");

  // Add the water level value (as a float) to the packet
  LoRa.print(waterLevel);
  Serial.println("Added water level to the packet");

  // Add the text " cm" and a newline character to the packet
  LoRa.println(" cm");
  Serial.println("Added ' cm' to the packet");

  // Complete the packet and send it, checking if the transmission was successful
  int transmissionStatus = LoRa.endPacket();
  Serial.println("Attempted to send packet");
  if (transmissionStatus == 1) {
    // Print a message to the Serial monitor indicating that the data was transmitted via LoRa
    Serial.println("Data transmitted via LoRa.");
  } else {
    // Print an error message to the Serial monitor if the transmission failed
    Serial.println("Error: LoRa transmission failed.");
  }
}

//--------------LOG DATA TO SD--------------//
void logDataToSDCard(float waterLevel) {
  File dataFile = SD.open("datalog.txt", FILE_WRITE);

  if (dataFile) {
    DateTime now = rtc.now();
    dataFile.print(now.year(), DEC);
    dataFile.print('/');
    dataFile.print(now.month(), DEC);
    dataFile.print('/');
    dataFile.print(now.day(), DEC);
    dataFile.print(" ");
    dataFile.print(now.hour(), DEC);
    dataFile.print(':');
    dataFile.print(now.minute(), DEC);
    dataFile.print(':');
    dataFile.print(now.second(), DEC);
    dataFile.print(" Water Level: ");
    dataFile.print(waterLevel);
    dataFile.println(" cm");
    dataFile.close();

    Serial.println("Data logged to SD card.");
  } else {
    Serial.println("Error opening datalog.txt");
  }
}

//--------------SETUP ROUTINE--------------//
void setup() {
  Serial.begin(9600);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Initialize the I2C interface
  Wire.begin();

  // Initialize SD card
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
    return;
  }

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("RTC initialization failed!");
    return;
  }
  if (rtc.lostPower()) { // Check if the RTC lost power
    Serial.println("RTC lost power, let's set the time!");
    // Following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Set up the alarm pin as input and attach the interrupt
  pinMode(alarmPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(alarmPin), alarmHandler, FALLING);

  // Enable LoRa radio
  pinMode(LoRa_EN, OUTPUT);
  digitalWrite(LoRa_EN, HIGH);

  // Initialize LoRa
  LoRa.setPins(LoRa_CS, LoRa_RST, LoRa_IRQ);
  if (!LoRa.begin(RFM95_FREQ)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  
}

//--------------MAIN PROGRAM--------------//
void loop() {
   
  Serial.println("Wake up!");
  delay(1000);

  // Enable LoRa radio
  digitalWrite(LoRa_EN, HIGH); 
  //delay(1000);

  // Read water level
  float waterLevel = readWaterLevel();
  Serial.print("Water Level: ");
  Serial.print(waterLevel);
  Serial.println(" cm");
  //delay(1000);

  // Log the data to the SD card
  logDataToSDCard(waterLevel);
  //delay(1000);

  // Check memory before transmitting
  Serial.print("Free memory before transmitting: ");
  Serial.println(freeMemory());

  // Transmit the data via LoRa
  transmitDataViaLoRa(waterLevel);
  delay(1000);

  // Disable LoRa radio
  digitalWrite(LoRa_EN, LOW); 

  // Clear the alarm flag and set the next alarm
  setRTCAlarm();
}
