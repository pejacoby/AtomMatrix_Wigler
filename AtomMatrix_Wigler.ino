#include <M5Atom.h>
#include <SD.h>
#include <SPI.h>
#include <TinyGPS++.h>
#include <WiFi.h>
// AtomMatrix_Wigler v ?? -- very much a playground
// Based on Version 1.3.2 from https://github.com/lukeswitz/AtomGPS_wigler/
// LED
bool ledState = false;
bool buttonLedState = true;
// useful color constants (RGB)
#define RED 0xff0000
#define GREEN 0x00ff00
#define BLUE 0x0000ff
#define YELLOW 0xffff00
#define PURPLE 0x800080
#define CYAN 0x00ffff
#define WHITE 0xffffff
#define OFF 0x000000

// LED positions -- the M5Matrix has a 5x5 LED display, array [0,24]
#define LED_CENTER 12 // Center of M5Matrix screen
#define LED_ACTIVE 17 // Active Status - 4th row center
#define LED_GPSFIX 20 // GPS fix indicator, 5th row 1st LED
#define LED_COUNT 25  // M5Matrix has 25 LEDs
// #define LED_ACTIVE 0 // if running on M5Atom with single LED

// GPS and Filesys
TinyGPSPlus gps;
char fileName[50];
const int maxMACs = 150;  // TESTING: buffer size
char macAddressArray[maxMACs][20];
int macArrayIndex = 0;

// Network Scanning
const int popularChannels[] = { 1, 6, 11 };
const int standardChannels[] = { 2, 3, 4, 5, 7, 8, 9, 10 };
const int rareChannels[] = { 12, 13, 14 };  // Depending on region
int timePerChannel[14] = { 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200 };
// colors for channels
int colorPerChannel[14] = {GREEN, BLUE, BLUE, BLUE, BLUE, GREEN, BLUE, BLUE, BLUE, BLUE, GREEN, CYAN, CYAN, CYAN};
// global arrays for matrix numbers and characters
int capW[LED_COUNT]; int capI[LED_COUNT]; int lowI[LED_COUNT]; int capF[LED_COUNT];
int one[LED_COUNT]; int two[LED_COUNT]; int three[LED_COUNT]; int four[LED_COUNT]; int five[LED_COUNT]; int six[LED_COUNT];
int seven[LED_COUNT]; int eight[LED_COUNT]; int nine[LED_COUNT]; int ten[LED_COUNT]; int eleven[LED_COUNT];
int twelve[LED_COUNT]; int thirteen[LED_COUNT]; int fourteen[LED_COUNT];
// array of arrays of all display numbers
int *displayNumbers[15] = { };


void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");
  M5.begin(true, false, true);
  SPI.begin(23, 33, 19, -1);              // investigate the -1 assignment and esp32 boards

// silly startup light sequence
   openingWords();

  while (!SD.begin()) {  // params throw a bunch of gpio warnings, TODO assign ss 
    Serial.println("SD Card initialization failed! Retrying...");
    blinkLED(RED, 500);  // will hang here until SD is readable
    delay(1000);
  }
  Serial.println("SD Card initialized.");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  Serial.println("WiFi initialized.");

  Serial1.begin(9600, SERIAL_8N1, 22, -1);
  Serial.println("GPS Serial initialized.");
  waitForGPSFix();
  initializeFile();
}

void loop() {
  static unsigned long lastBlinkTime = 0;
  const unsigned long blinkInterval = 3000;

  M5.update(); // update button status

  // if button was pressed, toggle the state of LED display (on/off)
  if (M5.Btn.wasPressed()) {
    buttonLedState = !buttonLedState;
  }

  while (Serial1.available() > 0) {
    gps.encode(Serial1.read());
  }

  if (gps.location.isValid()) {
    unsigned long currentMillis = millis();  //get the time here for accurate blinks
    if (currentMillis - lastBlinkTime >= blinkInterval && buttonLedState) {
      M5.dis.drawpix(LED_ACTIVE, GREEN);  // Flash green without a static blink 
      delay(120);
      M5.dis.drawpix(LED_ACTIVE,OFF); // turn off just the pixel
      M5.dis.clear();  // clear the whole display -- do we need this??  Seems like it for other lights to be accurate
      lastBlinkTime = currentMillis;
    }

    float lat = gps.location.lat();
    float lon = gps.location.lng();
    float altitude = gps.altitude.meters();
    float accuracy = gps.hdop.hdop();
    char utc[21];
    sprintf(utc, "%04d-%02d-%02d %02d:%02d:%02d", gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second());
    // Dynamic async per-channel scanning
    for (int channel = 1; channel <= 14; channel++) {
      int numNetworks = WiFi.scanNetworks(false, true, false, timePerChannel[channel - 1], channel);
      for (int i = 0; i < numNetworks; i++) {
        char currentMAC[20];
        strcpy(currentMAC, WiFi.BSSIDstr(i).c_str());
        if (!isMACSeen(currentMAC)) {
          strcpy(macAddressArray[macArrayIndex++], currentMAC);
          if (macArrayIndex >= maxMACs) macArrayIndex = 0;
          char dataString[300];
          snprintf(dataString, sizeof(dataString), "%s,\"%s\",%s,%s,%d,%d,%.6f,%.6f,%.2f,%.2f,WIFI", currentMAC, WiFi.SSID(i).c_str(), getAuthType(WiFi.encryptionType(i)), utc, WiFi.channel(i), WiFi.RSSI(i), lat, lon, altitude, accuracy);
          logData(dataString);

          // blink the LED for the channel ** this will impact performance if interval is too big
          blinkLEDchannel(colorPerChannel[channel],100,channel);
          // blink the actual channel number
//          blinkLEDnumber(colorPerChannel[channel],100,channel);
        }
      }
      // Update the scan duration for this channel based on the results
      updateTimePerChannel(channel, numNetworks);

      // update GPS status bar
      updateGpsStatusDisplay(accuracy,PURPLE);

    }
  } else {
    blinkLED(PURPLE, 500);  // waiting for GPS fix
  }
  delay(250);  // tested against 0,150,500 to yield most nets
}

// non-blocking LED blink - at the LED_ACTIVE location
void blinkLED(uint32_t color, unsigned long interval) {
  static unsigned long previousBlinkMillis = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - previousBlinkMillis >= interval) {
    ledState = !ledState;
    M5.dis.drawpix(LED_ACTIVE, ledState ? color : OFF); // blink the ACTIVE status LED
    previousBlinkMillis = currentMillis;
  }
}

// blink the LED corresponding to the channel position in the matrix
// make this fast so we don't kill performance
void blinkLEDchannel(uint32_t color, unsigned long interval, int position) {
  static unsigned long previousBlinkMillis = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - previousBlinkMillis >= interval) {
    ledState = !ledState;
    M5.dis.drawpix(position - 1, ledState ? color : OFF); // blink the LED corresponding to the channel
    previousBlinkMillis = currentMillis;
  }
}

// display the channel number seen from character array -- for extra fun - ****
//  ** NEED TO GET THE displayNumbers array into scope to use it at this point
void blinkLEDnumber(uint32_t color, unsigned long interval, int dispNum) {
  static unsigned long previousBlinkMillis = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - previousBlinkMillis >= interval) {
    ledState = !ledState;
    displayChar(displayNumbers[dispNum], ledState ? color : OFF); // number from array of LED representations
//    M5.dis.drawpix(position - 1, ledState ? color : OFF); // blink the LED corresponding to the channel
    previousBlinkMillis = currentMillis;
  }
  Serial.println("blinkLEDnumber: " + String(dispNum));
}


// connect to GPS and hold until we have a fix, blinking LED PURPLE while we wait
void waitForGPSFix() {
  Serial.println("Waiting for GPS fix...");
  while (!gps.location.isValid()) {
    if (Serial1.available() > 0) {
      gps.encode(Serial1.read());
    }
    blinkLED(PURPLE, 250);
  }
  M5.dis.clear();
  Serial.println("GPS fix obtained.");
}

// set up unique filename for this date and run, with appropriate Wigle.net headers
void initializeFile() {
  int fileNumber = 0;
  bool isNewFile = false;
  char fileDateStamp[16];
  sprintf(fileDateStamp, "%04d-%02d-%02d-", gps.date.year(), gps.date.month(), gps.date.day());
  do {
    snprintf(fileName, sizeof(fileName), "/AtomWigler-%s%d.csv", fileDateStamp, fileNumber);
    isNewFile = !SD.exists(fileName);
    fileNumber++;
  } while (!isNewFile);
  if (isNewFile) {
    File dataFile = SD.open(fileName, FILE_WRITE);
    if (dataFile) {
      dataFile.println("WigleWifi-1.4,appRelease=1.300000,model=GPS Kit,release=1.100000F+00,device=M5ATOM,display=NONE,board=ESP32,brand=M5");
      dataFile.println("MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type");
      dataFile.close();
      Serial.println("New file created: " + String(fileName));
    }
  } else {
    Serial.println("Using existing file: " + String(fileName));
  }
}

// check is MAC has already been observed and is in the cache array
bool isMACSeen(const char* mac) {
  for (int i = 0; i < macArrayIndex; i++) {
    if (strcmp(macAddressArray[i], mac) == 0) {
      return true;
    }
  }
  return false;
}

// Write a line of log data to the active output file
void logData(const char* data) {
  File dataFile = SD.open(fileName, FILE_APPEND);
  if (dataFile) {
    dataFile.println(data);
    dataFile.close();
  } else {
    Serial.println("Error opening " + String(fileName));
    blinkLED(RED, 500);
  }
}

// convert AuthType into appropriate text value for Wigle.net data file
const char* getAuthType(uint8_t wifiAuth) {
  switch (wifiAuth) {
    case WIFI_AUTH_OPEN:
      return "[OPEN]";
    case WIFI_AUTH_WEP:
      return "[WEP]";
    case WIFI_AUTH_WPA_PSK:
      return "[WPA_PSK]";
    case WIFI_AUTH_WPA2_PSK:
      return "[WPA2_PSK]";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "[WPA_WPA2_PSK]";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "[WPA2_ENTERPRISE]";
    case WIFI_AUTH_WPA3_PSK:
      return "[WPA3_PSK]";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "[WPA2_WPA3_PSK]";
    case WIFI_AUTH_WAPI_PSK:
      return "[WAPI_PSK]";
    default:
      return "[UNDEFINED]";
  }
}

// TESTING: algo for timePerChan
void updateTimePerChannel(int channel, int networksFound) {
  const int FEW_NETWORKS_THRESHOLD = 1;
  const int MANY_NETWORKS_THRESHOLD = 5;
  const int TIME_INCREMENT = 50;  // how many ms to adjust by
  const int MAX_TIME = 400;
  const int MIN_TIME = 100;

  if (networksFound >= MANY_NETWORKS_THRESHOLD) {
    timePerChannel[channel - 1] += TIME_INCREMENT;
    if (timePerChannel[channel - 1] > MAX_TIME) {
      timePerChannel[channel - 1] = MAX_TIME;
    }
  } else if (networksFound <= FEW_NETWORKS_THRESHOLD) {
    timePerChannel[channel - 1] -= TIME_INCREMENT;
    if (timePerChannel[channel - 1] < MIN_TIME) {
      timePerChannel[channel - 1] = MIN_TIME;
    }
  }
}

// fill the whole display with one color
// this is the same as M5.dis.fillpix(color)  !!!
void fillDisplay(int fillColor) {
  for(int i = 0; i < 25; i++) {
      M5.dis.drawpix(i, fillColor);
  }
}

// use the bottom row of the display as VU meter for GPS accuracy
//   this is LED positions 20 - 24
void updateGpsStatusDisplay(float accuracy, int color) {
  float scaleFactor = 2.25; // delta between VU steps
  float threshold = 2; // initial threshold for "full" VU meter setting
  int colorGPS[5] = { 0x200000, 0x400000, 0x600000, 0x800000, 0xA00000 }; // All RED but shades thereof

  M5.dis.drawpix(20,colorGPS[0]); // if we have a fix, leave first/lowest LED set

// loop through all positions, light LED based on accuracy thresholds
// more LEDs for more accuracy, so we count backwards from the right
  for (int i = 24; i > 20; i--) {
    if (accuracy <= threshold) {
       M5.dis.drawpix(i, colorGPS[i-20]);   // was just 'color'
    } else {
      M5.dis.drawpix(i, OFF);
    }
  //  Serial.println("GPS display: accuracy: "+ String(accuracy) + " , threshold: " + String(threshold));
    threshold = threshold * scaleFactor;
  }
}

// display a character defined by a 5x5 integer matrix. cell value 1 is on, 0 is off
void displayChar(int arr[], int color) {
  int pixelColor = 0;

  for(int i = 0; i < 25; i++) {
    if (arr[i] == 1) {
       pixelColor = color;
//       Serial.print("1, ");
    } else {
       pixelColor = OFF;
//       Serial.print("0, ");
    };
    M5.dis.drawpix(i,pixelColor);
//    delay(20);
  }
    Serial.println("Displayed char...");
}

// A silly introduction to the unit - display the word "WiFi"
void openingWords() {

  //  setupDisplayChars();  // could put this in a subroutine for cleanliness
  // define letters for the display
  int capW[25] = {
  1,0,0,0,1,
  1,0,0,0,1,
  1,0,1,0,1,
  1,1,0,1,1,
  1,0,0,0,1
  };
  int capI[25] = {
  1,1,1,1,1,
  0,0,1,0,0,
  0,0,1,0,0,
  0,0,1,0,0,
  1,1,1,1,1
  };
  int lowI[25] = {
  0,0,1,0,0,
  0,0,0,0,0,
  0,0,1,0,0,
  0,0,1,0,0,
  0,1,1,1,0
  };
  int capF[25] = {
  1,1,1,1,1,
  1,0,0,0,0,
  1,1,1,1,0,
  1,0,0,0,0,
  1,0,0,0,0
  };

  // define numbers for the display
  int zero[25] = {
  0,0,1,1,0,
  0,1,0,0,1,
  0,1,0,0,1,
  0,1,0,0,1,
  0,0,1,1,0
  };
  int one[25] = {
  0,0,1,0,0,
  0,1,1,0,0,
  0,0,1,0,0,
  0,0,1,0,0,
  0,1,1,1,0
  };
  int two[25] = {
  0,1,1,1,1,
  0,0,0,0,1,
  0,0,1,1,1,
  0,1,0,0,0,
  0,1,1,1,1
  };
  int three[25] = {
  0,1,1,1,1,
  0,0,0,0,1,
  0,0,1,1,0,
  0,0,0,0,1,
  0,1,1,1,1
  };
  int four[25] = {
  0,1,0,0,1,
  0,1,0,0,1,
  0,1,1,1,1,
  0,0,0,0,1,
  0,0,0,0,1
  };
  int five[25] = {
  0,1,1,1,1,
  0,1,0,0,0,
  0,1,1,1,1,
  0,0,0,0,1,
  0,1,1,1,1
  };
  int six[25] = {
  0,0,1,1,1,
  0,1,0,0,0,
  0,1,1,1,1,
  0,1,0,0,1,
  0,1,1,1,1
  };
  int seven[25] = {
  0,1,1,1,1,
  0,0,0,1,0,
  0,0,1,0,0,
  0,0,1,0,0,
  0,0,1,0,0
  };
  int eight[25] = {
  0,1,1,1,1,
  0,1,0,0,1,
  0,0,1,1,0,
  0,1,0,0,1,
  0,1,1,1,1
  };
  int nine[25] = {
  0,1,1,1,1,
  0,1,0,0,1,
  0,1,1,1,1,
  0,0,0,0,1,
  0,0,0,0,1
  };
  int ten[25] = {
  1,0,1,1,1,
  1,0,1,0,1,
  1,0,1,0,1,
  1,0,1,0,1,
  1,0,1,1,1
  };
  int eleven[25] = {
  1,0,0,1,0,
  1,0,1,1,0,
  1,0,0,1,0,
  1,0,0,1,0,
  1,0,1,1,1
  };
  int twelve[25] = {
  1,0,1,1,1,
  1,0,0,0,1,
  1,0,1,1,1,
  1,0,1,0,0,
  1,0,1,1,1
  };
  int thirteen[25] = {
  1,0,1,1,1,
  1,0,0,0,1,
  1,0,0,1,1,
  1,0,0,0,1,
  1,0,1,1,1
  };
  int fourteen[25] = {
  1,0,1,0,1,
  1,0,1,0,1,
  1,0,1,1,1,
  1,0,0,0,1,
  1,0,0,0,1
  };

  // array of arrays of all display numbers
  int *displayNumbers[15] = { zero, one, two, three, four, five, six, seven, eight, nine, ten, eleven, twelve, thirteen, fourteen };
 
  // roll a color background
  fillDisplay(GREEN);
  delay(500);
  fillDisplay(OFF);
  delay(500);

  M5.dis.clear(); // full clear of display, reset cursor -- DO WE NEED THIS?
  Serial.println("Displaying Opening Words...");
  // WiFi!
  displayChar(capW,BLUE);
  delay(500);
  displayChar(lowI,GREEN);
  delay(500);
  displayChar(capF,BLUE);
  delay(500);
  displayChar(lowI,GREEN);
  delay(500);

  fillDisplay(OFF);

  // run all the numbers
  Serial.println("Displaying all the number shapes...");
  for (int i = 0; i < 15; i++) {
    displayChar(displayNumbers[i], CYAN);
    delay(500);
    M5.dis.clear();
  }

}
