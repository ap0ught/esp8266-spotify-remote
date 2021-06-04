/**The MIT License (MIT)
 
 Copyright (c) 2018 by ThingPulse Ltd., https://thingpulse.com
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 */

#include "SSD1327.h"
#include <time.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
//#include <ESP8266WiFiMulti.h> 
#include <FS.h>
#include <ESP8266mDNS.h>  
#include "SpotifyClient.h"
#include "settings.h"
#include "images.h"
#include "caCert.h"

 // Statements like:
 // #pragma message(Reminder "Fix this problem!")
 // Which will cause messages like:
 // C:\Source\Project\main.cpp(47): Reminder: Fix this problem!
 // to show up during compiles. Note that you can NOT use the
 // words "error" or "warning" in your reminders, since it will
 // make the IDE think it should abort execution. You can double
 // click on these messages and jump to the line in question.
 //
 // see https://stackoverflow.com/questions/5966594/how-can-i-use-pragma-message-so-that-the-message-points-to-the-filelineno
 //
#define Stringize( L )     #L 
#define MakeString( M, L ) M(L)
#define $Line MakeString( Stringize, __LINE__ )
#define Reminder __FILE__ "(" $Line ") : Reminder: "
#ifdef LOAD_SD_LIBRARY
#pragma message(Reminder "Comment out the line with LOAD_SD_LIBRARY /JPEGDecoder/src/User_config.h !")
#endif


const char* host = "api.spotify.com";
const int httpsPort = 443;

int BUFFER_WIDTH = 240;
int BUFFER_HEIGHT = 160;
// Limited to 4 colors due to memory constraints
int BITS_PER_PIXEL = 2; // 2^2 =  4 colors

SpotifyClient client(clientId, clientSecret, redirectUri);
SpotifyData data;
SpotifyAuth auth;

String currentImageUrl = "";
uint32_t lastDrawingUpdate = 0;
uint16_t counter = 0;
long lastUpdate = 0;
//TS_Point lastTouchPoint;
uint32_t lastTouchMillis = 0;
boolean isDownloadingCover = false;

String formatTime(uint32_t time);
void saveRefreshToken(String refreshToken);
String loadRefreshToken();
//void displayLogo();

//void drawSongInfo();
//DrawingCallback drawSongInfoCallback = &drawSongInfo;

void drawBitMap(const unsigned char image[], int x, int y, int w, int h, int color);

extern const unsigned char caCert[] PROGMEM;
extern const unsigned int caCertLen;

void drawString8(uint8_t x, uint8_t y, String textString, uint8_t color);
void drawString16(uint8_t x, uint8_t y, String textString, uint8_t color);

enum DisplayMode {
  timeWeather, songInfo, boot
  };

DisplayMode currentView = boot;

SSD1327 disp(PIN_CS, PIN_DC, PIN_RST);

void setup() {
  Serial.begin(115200);

  Serial.println("Booting:   ");

  disp.init();
  disp.clearBuffer();
  drawBitMap(logo, 0, 0, 128, 64, 1);
  //disp.drawString(0, 112, "Weather Boi v1.0", 0xF, 16);
  disp.writeFullBuffer();

  disp.clearBuffer();
  drawBitMap(logo, 0, 0, 128, 64, 1);
  drawProgressBar(10, 14, 70, 100, 20, 1);
  //drawProgressBar(int progress, int x, int y, int w, int h, int color)
  disp.writeFullBuffer();


  disp.clearBuffer();
  drawBitMap(logo, 0, 0, 128, 64, 1);
  drawProgressBar(20, 14, 70, 100, 20, 1);
  //drawString16(0, 90, "Mount Storage", 0xF);
  disp.writeFullBuffer();

  boolean mounted = SPIFFS.begin();
 
  if (!mounted) {
    disp.clearBuffer();
    drawBitMap(logo, 0, 0, 128, 64, 1);
    drawProgressBar(25, 14, 70, 100, 20, 1);
    //drawString16(0, 90, "Formatting...", 0xF);
    disp.writeFullBuffer();
    
    Serial.println("FS not formatted. Doing that now");
    SPIFFS.format();
    Serial.println("FS formatted...");
    SPIFFS.begin();
  }

  disp.clearBuffer();
  drawBitMap(logo, 0, 0, 128, 64, 1);
  drawProgressBar(30, 14, 70, 100, 20, 1);
  //drawString16(0, 90, "Joining Wi-Fi", 0xF);
  //drawString8(0, 110, ssid, 0xF);
  disp.writeFullBuffer();
  
  Serial.println();
  Serial.print("connecting to ");
  Serial.println(ssid);
  WiFi.hostname(espotifierNodeName);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

    disp.clearBuffer();
  drawBitMap(logo, 0, 0, 128, 64, 1);
  //drawString16(0, 90, "Setting Clock", 0xF);
  drawProgressBar(40, 14, 70, 100, 20, 1);
  disp.writeFullBuffer();
  
     // Synchronize time useing SNTP. This is necessary to verify that
  // the TLS certificates offered by the server are currently valid.
  Serial.print("Setting time using SNTP");
  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));

  //Testing Certificate_______________________
  
//  WiFiClientSecure clientTest;
//  // Load root certificate in DER format into WiFiClientSecure object
//  bool res = clientTest.setCACert_P(caCert, caCertLen);
//  if (!res) {
//    Serial.println("Failed to load root CA certificate!");
//    while (true) {
//      yield();
//    }
//  }
//
//  // Connect to remote server
//  Serial.print("connecting to ");
//  Serial.println("accounts.spotify.com");
//  if (!clientTest.connect("accounts.spotify.com", 443)) {
//    Serial.println("connection failed");
//    return;
//  }
//
//  // Verify validity of server's certificate
//  if (clientTest.verifyCertChain("accounts.spotify.com")) {
//    Serial.println("Server certificate verified");
//  } else {
//    Serial.println("ERROR: certificate verification failed!");
// //   return;
//  }

  
  disp.clearBuffer();
  drawBitMap(logo, 0, 0, 128, 64, 1);
  drawProgressBar(60, 14, 70, 100, 20, 1);
  //drawString16(0, 90, "Setup mDNS", 0xF);
  disp.writeFullBuffer();
  
  if (!MDNS.begin(espotifierNodeName)) {             // Start the mDNS responder for esp8266.local
    Serial.println("Error setting up MDNS responder!");
  }
  else Serial.println("mDNS responder started");

  disp.clearBuffer();
  drawBitMap(logo, 0, 0, 128, 64, 1);
  drawProgressBar(70, 14, 70, 100, 20, 1);
  //drawString16(0, 90, "Spotify Tokens", 0xF);
  disp.writeFullBuffer();

  
  String code = "";
  String grantType = "";
  String refreshToken = loadRefreshToken();
  if (refreshToken == "") {
    Serial.println("No refresh token found. Requesting through browser");

    disp.clearBuffer();
    //for(int i = 0; i < 128; i++) disp.drawLine(i, 0, i, 85, 16, false);
    //drawBitMap(QRcode, 0, 24, 80, 80, 16);
    drawProgressBar(75, 14, 85, 100, 5, 1);
    //drawString16(0, 90, "Go To: ", 0xF);
    String displayURL = espotifierNodeName + ".local";
    //drawString8(0, 110, displayURL, 0xF);
    disp.writeFullBuffer();
    Serial.println ( "Open browser at http://" + espotifierNodeName + ".local" );
    
    code = client.startConfigPortal();
    Serial.println("Got that auth code yo");
    grantType = "authorization_code";
  } 
  else {
    Serial.println("Using refresh token found on the FS");
    code = refreshToken;
    grantType = "refresh_token";
  }


  disp.clearBuffer();
  drawBitMap(logo, 0, 0, 128, 64, 16);
  drawProgressBar(80, 14, 70, 100, 20, 16);
  //drawString16(0, 90, "Spotify Tokens...", 0xF);
  disp.writeFullBuffer();
  Serial.println("OMG OKAY IT'S HAPPENING");
  client.getToken(&auth, grantType, code);
  Serial.printf("Refresh token: %s\nAccess Token: %s\n", auth.refreshToken.c_str(), auth.accessToken.c_str());
  if (auth.refreshToken != "") {
    saveRefreshToken(auth.refreshToken);
  }
  //client.setDrawingCallback(&drawSongInfoCallback);
}
//LOOP_________________________________________________________________________________________________
void loop() {
    Serial.println("Entered Loop");

    if (millis() - lastUpdate > 1000) {
      uint16_t responseCode = client.update(&data, &auth);
      Serial.printf("HREF: %s\n", data.image300Href.c_str());
      lastUpdate = millis();
      Serial.printf("--------Response Code: %d\n", responseCode);
      Serial.printf("--------Free mem: %d\n", ESP.getFreeHeap());
      if (responseCode == 401) {
        client.getToken(&auth, "refresh_token", auth.refreshToken);
        if (auth.refreshToken != "") {
          saveRefreshToken(auth.refreshToken);
        }
      }
      if (responseCode == 200) {
 
      }
      if (responseCode == 400) {
        Serial.println("Please define\nclientId and clientSecret");
      }

      Serial.print("Title:  ");
      Serial.println(data.title);
      Serial.print("Artist: ");
      Serial.println(data.artistName);

    disp.clearBuffer();
    //drawString16(0, 90, data.title, 0xF);
    disp.writeFullBuffer();

    }

    
    //drawSongInfo();
    
//    if (touchController.isTouched(500) && millis() - lastTouchMillis > 1000) {
//      TS_Point p = touchController.getPoint();
//      lastTouchPoint = p;
//      lastTouchMillis = millis();
//      String command = "";
//      String method = "";
//      if (p.y > 160) {
//        if (p.x < 80) {
//          method = "POST"; 
//          command = "previous";
//        } else if (p.x >= 80 && p.x <= 160) {
//          method = "PUT"; 
//          command = "play";
//          if (data.isPlaying) {
//            command = "pause";
//          }
//          data.isPlaying = !data.isPlaying;   
//        } else if (p.x > 160) {
//          method = "POST"; 
//          command = "next";
//        }
//        uint16_t responseCode = client.playerCommand(&auth, method, command);
//    		Serial.print("playerCommand response =");
//    		Serial.println(responseCode);
//  	  }
//    }

} // void loop();

/*
    @brief      Draw a PROGMEM-resident 1-bit image at the specified (x,y) position, using the specified foreground color (unset bits are transparent).
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with monochrome bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
    @param    color 4-bit 0 to 15;
*/
void drawBitMap(const unsigned char image[], int x, int y, int w, int h, int color){
  //Assumes image is square
  //Assumes 1 bit per pixel
  //const unsigned char image[(sizeof(myBitmap))] = myBitmap;
  int height = h;
  int width = w/8; //  divide by into # of byte segments
  int arrPos = 0;
  unsigned char bits[8];
  for(int k = 0; k < height; k++){
    for(int j = 0; j < width; j++){
      for (int i=0; i<8; i++) {
        bits[i] = ((1 << (i % 8)) & (pgm_read_word_near(&image[arrPos]))) >> (i % 8);
      }
      //Serial.println(bits);
      for(int h = sizeof(bits)-1; h >= 0; h--){
        if(bits[h]){
          disp.drawPixel(((j+1)*8)-h+y, k+x, color, false);
        }
        else{
          disp.drawPixel(((j+1)*8)-h+y, k+x, 0, false);
        }
        
      }
      arrPos++;
    }
  }
}

void drawString16(uint8_t x, uint8_t y, String textString, uint8_t color){
  char text[64];
  textString.toCharArray(text, 64);
  const char* thisChar;
  uint8_t xOffset = 0;
  for (thisChar = text; *thisChar != '\0'; thisChar++) {
    disp.drawChar16(x+xOffset, y, *thisChar, color);
    xOffset += 8;
  }
}

void drawString8(uint8_t x, uint8_t y, String textString, uint8_t color){
  char text[64];
  textString.toCharArray(text, 64);
  const char* thisChar;
  uint8_t xOffset = 0;
  for (thisChar = text; *thisChar != '\0'; thisChar++) {
    disp.drawChar(x+xOffset, y, *thisChar, color);
    xOffset += 8;
  }
}

/*
    @brief    Draws a recagular progress bar 
    @param    progress  0 to 100 percentage of bar to fill;
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
    @param    color 4-bit 0 to 15;
*/
void drawProgressBar(int progress, int x, int y, int w, int h, int color){
  disp.drawLine(x, y, x+w, y, color, false);      //Top Horizontal
  disp.drawLine(x, y+h, x+w, y+h, color, false);  //Bottom Horizontal
  disp.drawLine(x, y, x, y+h, color, false);      //Left Vertical
  disp.drawLine(x+w, y, x+w, y+h, color, false);  //Right Vertical
  for(int j = 0; j < w; j++){
      disp.drawLine(x+j, y, x+j, y+h, color, false);
    if((progress*w)/100 < j) break; //Stop drawing at progress bar finish
  }
}

String formatTime(uint32_t time) {
  char time_str[10];
  uint8_t minutes = time / (1000 * 60);
  uint8_t seconds = (time / 1000) % 60;
  sprintf(time_str, "%2d:%02d", minutes, seconds);
  return String(time_str);
}

void saveRefreshToken(String refreshToken) {
  
  File f = SPIFFS.open("/refreshToken3.txt", "w+");
  if (!f) {
    Serial.println("Failed to open config file");
    return;
  }
  f.println(refreshToken);
  f.close();
}

String loadRefreshToken() {
  Serial.println("Loading config");
  File f = SPIFFS.open("/refreshToken3.txt", "r");
  if (!f) {
    Serial.println("Failed to open config file");
    return "";
  }
  while(f.available()) {
      //Lets read line by line from the file
      String token = f.readStringUntil('\r');
      Serial.printf("Refresh Token: %s\n", token.c_str());
      f.close();
      return token;
  }
  return "";
}
