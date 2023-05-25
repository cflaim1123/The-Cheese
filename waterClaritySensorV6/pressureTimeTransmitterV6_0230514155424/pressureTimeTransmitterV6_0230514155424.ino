// Include Libraries
#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include "RTClib.h"
#include <MS5803_05.h> 
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#define I2C_BUS_1_SDA 6 
#define I2C_BUS_1_SCL 9 
#define chipSelect 10

// Make datetime variable
char buffer[] = "yyyy-MM-dd HH:mm:ss";

// Make second I2C bus
TwoWire bus1 = TwoWire(1);

// Initialize clock and pressure sensor objects
MS_5803 press1 = MS_5803(512, &bus1, I2C_BUS_1_SDA, I2C_BUS_1_SCL);
RTC_DS3231 rtc;

// Variables for test data
int sample_num = 0;
float pressure;
float temp; 
bool picture_flag = true;
bool sleep_flag = false;

// Sample number tracker for picture flag
int last_picture_sample = 0;
unsigned long elapsed_time = 0;

// Serial communication baud rate
int baud = 115200;
int sample_delay = 1000;
unsigned long elapsed_time_at_sample;

// MAC Address of responder
uint8_t broadcastAddress[] = {0xCC, 0xDB, 0xA7, 0xE64, 0x58, 0x48}; 

// Define a data structure
typedef struct struct_message {
  char a[32];
  int b;
  float c;
  float d;
  bool e;
  unsigned long f;
} struct_message;

// Create a structured object
struct_message sensorData;

// Peer info
esp_now_peer_info_t peerInfo;

void setup() {
  Serial.begin(baud);
  unsigned long start = millis();
  Serial.print("Time millis:");
  Serial.println(start);
  Serial.println();
  Serial.println();
  Wire.begin();
  rtc.begin();
  delay(500);

  // initialize SD card
  initMicroSDCard();
  // initialize pressure sensor
  initPressureSensor();
  // initialize RTC
  initRTC();
  // initialize ESP Now and peer address info
  initESPNow();

  // Uncomment the following line to set the time
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  DateTime now = rtc.now();
  String nowString = String(now.year()) + "-" + String(now.month()) + "-" + 
  String(now.day()) + " " + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());

  String tempHeader = "################## Metadata log start ##################";
  const char* logHeader = tempHeader.c_str();
  logFile(SD, logHeader, true);

  tempHeader = "Sample #, Pressure (mbar), Temperature (deg C), Time (yyyy-MM-dd HH:mm:ss), Picture sample (T/F), Elapsed time since power on";
  const char* header = tempHeader.c_str();
  appendFile(SD, "/data.csv", header);
}

void loop() {
  String sampleString = "Sample: " + String(sample_num);
  logFile(SD, sampleString.c_str(), true);

  // Get current time, pressure, and temp
  DateTime now = rtc.now();
  press1.readSensor();
  pressure = press1.pressure();
  temp = press1.temperature();
  elapsed_time_at_sample = millis();
  delay(10);

  if(pressure > 1020){
    sample_delay = 200;
  }

  String nowString = String(now.year()) + "-" + String(now.month()) + "-" + String(now.day()) + 
  " " + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());
  delay(10);

  // Set default pressure flag
  picture_flag = false;
  // Set picture flag
  if(sample_num % 100 == 0){
      picture_flag = true;
  }
  
  
  // Format structured data
   strcpy(sensorData.a, nowString.c_str());
  sensorData.b = sample_num;
  sensorData.c = pressure;
  sensorData.d = temp;
  sensorData.e = picture_flag;
  sensorData.f = elapsed_time_at_sample;

  // Create data row and write it to file
  String tempDataRow = String(sample_num) + "," + String(pressure) + "," + String(temp) + "," + 
      nowString + "," + String(picture_flag) + "," + String(elapsed_time_at_sample);
  const char* dataRow = tempDataRow.c_str();
  //Serial.println("Created data row");

  appendFile(SD, "/data.csv", dataRow);

    // Send message via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &sensorData, sizeof(sensorData));
   
  if (result == ESP_OK) {
    Serial.println("Sending confirmed");
    logFile(SD, "Sending confirmed", true);
    logFile(SD, "", true);
  }
  else {
    Serial.println("Sending error");
    logFile(SD, "Sending error", true);
    logFile(SD, "", true);
  }
    // Print sent data to serial monitor
  Serial.print("pressure: ");
  Serial.println(pressure);
  Serial.print("Temperature: ");
  Serial.println(temp);
  Serial.print("Date and time:");
  //Serial.println(nowString);

  Serial.print(now.year());
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  //Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  //Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();

  sample_num++;
  delay(sample_delay); 
}

//_____________________________________________________________________________________//
// Init                                                                                //
//_____________________________________________________________________________________//

void initESPNow(){
// Set ESP32 as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Initilize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    logFile(SD, "Error initializing ESP-NOW", true);
    return;
  }

  delay(1000);

  // Register the send callback
  esp_now_register_send_cb(OnDataSent);

  delay(1000);
  
  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  delay(1000);

  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    logFile(SD, "Failed to add peer", true);
    return;
  }
  delay(1000);
}

void initRTC(){
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    logFile(SD, "Couldn't find RTC", true);
    Serial.flush();
    //while (1) delay(10);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    logFile(SD, "RTC lost power, let's set the time!", true);
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
}

// Init micro SD
void initMicroSDCard() {
if(!SD.begin(chipSelect)){
    Serial.println("Card Mount Failed");
    logFile(SD, "Card Mount Failed", true);
    return;
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    logFile(SD, "No SD card attached", true);
    return;
  }

  Serial.print("SD Card Type: ");
  logFile(SD, "SD Card Type: ", false);
  if(cardType == CARD_MMC){
    Serial.println("MMC");
    logFile(SD, "MMC", true);
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
    logFile(SD, "SDSC", true);
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
    logFile(SD, "SDHC", true);
  } else {
    Serial.println("UNKNOWN");
    logFile(SD, "UNKNOWN", true);
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
}

void initPressureSensor(){
      // Check if pressure sensor initialized
  if (press1.initializeMS_5803()) {
    Serial.println( "MS5803 1 CRC check OK." );
    logFile(SD, "MS5803 1 CRC check ok", true);
  } 
  else {
    Serial.println( "MS5803 1 CRC check FAILED!" );
    logFile(SD, "MS5803 1 CRC check FAILED!", true);
  }
  delay(3000);
}
//_____________________________________________________________________________________//
// SD file functions                                                                   //
//_____________________________________________________________________________________//

// Ex) createDir(SD, "/mydir");
void createDir(fs::FS &fs, const char * path){
  Serial.printf("Creating Dir: %s\n", path);
  if(fs.mkdir(path)){
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

// Ex) writeFile(SD, "/hello.txt", "Hello ");
void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

// Ex) appendFile(SD, "/hello.txt", "World!\n");
void appendFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.println(message)){
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void logFile(fs::FS &fs, const char * message, bool newLine){
  Serial.printf("Logging to file: %s\n", "/log.txt");

  File file = fs.open("/log.txt", FILE_APPEND);
  if(newLine){
    if(!file){
    Serial.println("Failed to open file for logging");
    return;
    }
    if(file.println(message)){
      Serial.println("Message logged");
  } else {
      Serial.println("Logging failed");
    }
  }
  else{
    if(!file){
    Serial.print("Failed to open file for logging");
    return;
    }
    if(file.print(message)){
      Serial.print("Message logged");
  } else {
      Serial.print("Logging failed");
    }
  }
  file.close();

}

//_____________________________________________________________________________________//
// Data callback                                                                       //
//_____________________________________________________________________________________//

// Callback function called when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  logFile(SD, "\r\nLast Packet Send Status:\t", false);
  logFile(SD, status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail", true);
}

