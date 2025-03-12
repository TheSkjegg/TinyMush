// Description: 
//    TinyMush code, ESP32 with WIFI and a tactile pushbutton. Controls 10 RGB LED's by either button or webpage including "OK to wake" alarm
//    
// Button functions:
//    Short press: Toggles the LED's ON/OFF
//    Long press: Toggles between increasing or decreasing the brightness
//    Double Click: If turning OFF and then ON again immmediatley, mode will increment by one
//    
// Webpage functions:
//    Displays current time
//    Set alarm time
//    Enable/disable alarm
//    Brightness slider
//    Mode increment button
//
// Modes: 
//    1:    changes between rainbow, rainbowWithGlitter, confetti, sinelon, juggle and bpm patterns every 10th second
//    2..7: each of the patterns above fixed
//    8/9:  select static color of outer and inner LED's  
//    9/10: select static color on all LED's
//
// Usage:
//    Developed on the Seeed Studio XIAO ESP32 C3 board, as used in TinyMush
//
// Author: 
//    The Skjegg 01/02/2025
//
// Versions:
//    v001: First version

#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <WebServer.h>
#include <FastLED.h>

#define NUM_LEDS 10
#define DATA_PIN 9
#define BUTTON_PIN 10
#define maxMode 7
CRGB leds[NUM_LEDS];

bool ledsOn = false;
bool fadeUp = false;
uint8_t brightness = 100;
int mode = 0;
unsigned long timer = 0;

int alarmHour = 7;
int alarmMinute = 0;
bool alarmEnabled = false;

// Replace these with your network credentials
const char* ssid = "YOUR SSID HERE";
const char* password = "YOUR PASSWORD HERE";

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000); // NTP server, time offset in seconds, update interval

// Create a web server on port 80
WebServer server(80);

// Function to handle the root URL
void handleRoot() {
  String html = "<html><head><title>ESP32 Clock</title><style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; padding: 50px; }";
  html += "h1 { font-size: 3em; }";
  html += "p { font-size: 2em; display: inline-block; }";
  html += "button { font-size: 1.5em; padding: 10px 20px; margin: 10px; }";
  html += ".alarm-container { display: flex; justify-content: center; align-items: center; }";
  html += ".alarm-time { margin-left: 10px; visibility: " + String(alarmEnabled ? "visible" : "hidden") + "; }"; // Hide alarm time when disabled
  html += ".slider-container { margin-top: 20px; }";
  html += ".slider { width: 100%; height: 25px; }";
  html += ".mode-button { font-size: 2em; padding: 15px 30px; margin-top: 30px; }"; // Updated style for the mode button
  html += "</style></head><body>";
  html += "<h1>Current time: " + timeClient.getFormattedTime() + "</h1>";
  html += "<h1>Set alarm time: <input type='time' id='alarmTime' onchange='setAlarm()' required></h1>"; // Single time input field with onchange event
  html += "<div class='alarm-container'><button onclick='toggleAlarm()'>" + String(alarmEnabled ? "Disable Alarm" : "Enable Alarm") + "</button>";
  if (alarmEnabled && alarmHour != -1 && alarmMinute != -1) {
    html += "<p class='alarm-time'>Alarm: " + String(alarmHour) + ":" + (alarmMinute < 10 ? "0" : "") + String(alarmMinute) + "</p>";
  }
  html += "</div>";
  html += "<div class='slider-container'>";
  html += "<h1 id='sliderValue'>Brightness Value: " + String(brightness) + "</h1>";
  html += "<input type='range' min='0' max='100' value='" + String(brightness) + "' class='slider' id='slider' oninput='updateSlider(this.value)'>";
  html += "</div>";
  html += "<button class='mode-button' onclick='incrementMode()'>Mode</button>"; // Updated button text and style

  html += "<script>";
  html += "function setAlarm() {";
  html += "var time = document.getElementById('alarmTime').value;";
  html += "if (time) {";
  html += "var xhr = new XMLHttpRequest();";
  html += "xhr.open('GET', '/setAlarm?time=' + time, true);";
  html += "xhr.onload = function() {";
  html += "if (xhr.status === 200) {";
  html += "location.reload();"; // Reload the page to update the alarm time
  html += "}";
  html += "};";
  html += "xhr.send();";
  html += "}";
  html += "}";
  html += "function toggleAlarm() {";
  html += "var xhr = new XMLHttpRequest();";
  html += "xhr.open('GET', '/toggleAlarm', true);";
  html += "xhr.onload = function() {";
  html += "if (xhr.status === 200) {";
  html += "location.reload();"; // Reload the page to update the alarm state
  html += "}";
  html += "};";
  html += "xhr.send();";
  html += "}";
  html += "function updateSlider(value) {";
  html += "var xhr = new XMLHttpRequest();";
  html += "xhr.open('GET', '/setSlider?value=' + value, true);";
  html += "xhr.onload = function() {";
  html += "if (xhr.status === 200) {";
  html += "document.getElementById('sliderValue').innerText = 'Brightness: ' + value;";
  html += "}";
  html += "};";
  html += "xhr.send();";
  html += "}";
  html += "function incrementMode() {";
  html += "var xhr = new XMLHttpRequest();";
  html += "xhr.open('GET', '/incrementMode', true);";
  html += "xhr.onload = function() {";
  html += "if (xhr.status === 200) {";
  html += "location.reload();"; // Reload the page to update the mode
  html += "}";
  html += "};";
  html += "xhr.send();";
  html += "}";

  html += "</script>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}


// Function to handle the /setAlarm URL
void handleSetAlarm() {
  if (server.hasArg("time")) {
    String time = server.arg("time");
    int separatorIndex = time.indexOf(':');
    alarmHour = time.substring(0, separatorIndex).toInt();
    alarmMinute = time.substring(separatorIndex + 1).toInt();
  }
  server.send(200, "text/plain", "Alarm Set");
}

// Function to handle the /toggleAlarm URL
void handleToggleAlarm() {
  alarmEnabled = !alarmEnabled;
  server.send(200, "text/plain", "Alarm Toggled");
}

// Function to handle the /setSlider URL
void handleSetSlider() {
  if (server.hasArg("value")) {
    brightness = server.arg("value").toInt();
    if(brightness < 40)
      FastLED.setBrightness(brightness);
    else
      FastLED.setBrightness(map(brightness, 40, 100, 40, 255));
    FastLED.show();
  }
  server.send(200, "text/plain", "Slider Set");
}

void handleIncrementMode() {
  mode++;
  if (mode > maxMode) {
    mode = 1;
  }
  server.send(200, "text/plain", "Mode Incremented");
}

// Function to check if it's time to trigger the alarm
void checkAlarm() {
  if (alarmEnabled && timeClient.getHours() == alarmHour && timeClient.getMinutes() == alarmMinute) {
    mode = 0; // Set alarm mode
    ledsOn = true;
  }
}

// Function to check the button state and toggle LEDs
void checkButton() {
  bool currentButtonState = digitalRead(BUTTON_PIN);
  if (currentButtonState == LOW) {
    if(mode == 0) // reset alarm
      mode = 1; 
    if((millis() - timer) < 500){ // Double click, change mode
      if(mode < maxMode)
        mode++;
      else
        mode = 1;
    ledsOn = true;
    while (currentButtonState == LOW)
            currentButtonState = digitalRead(BUTTON_PIN);
    delay(100);
    }
    else{
      delay(250); // Wait a bit to check for long press
      currentButtonState = digitalRead(BUTTON_PIN);
      if(currentButtonState == LOW){ // Long press, adjust brightness
        if(!ledsOn){ // Just turn on LED's if they were OFF
          ledsOn = true;
          fill_rainbow(leds, NUM_LEDS, 0); // Use fill_rainbow for the rainbow effect
          FastLED.show();
          while (currentButtonState == LOW)
            currentButtonState = digitalRead(BUTTON_PIN);
        }
        else{ // Adjust brightness on LED's if they were ON
          while (currentButtonState == LOW) {
            if(fadeUp && (brightness < 100))
              brightness++;
            else if (!fadeUp && (brightness > 2))
              brightness--;
            
            //FastLED.setBrightness(brightness);
            if(brightness < 40)
              FastLED.setBrightness(brightness);
            else
              FastLED.setBrightness(map(brightness, 40, 100, 40, 255));
            FastLED.show();
            delay(50);
            currentButtonState = digitalRead(BUTTON_PIN);
          }
          fadeUp = !fadeUp;
        }
      }
      else{ // not long press, toggle ON/OFF
        ledsOn = !ledsOn;
        if(!ledsOn){
          for (int i = 0; i < NUM_LEDS; i++)
            leds[i] = CRGB::Black; // Set all LEDs to black (off)
        FastLED.show();
        }
      }
    }
  timer = millis();
  }
}

void setup() {
  // Start Serial for debugging
  Serial.begin(115200);
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected!");

  // Print the IP address
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  // Start the NTP client
  timeClient.begin();

  // Start the web server
  server.on("/", handleRoot);
  server.on("/setAlarm", handleSetAlarm);
  server.on("/toggleAlarm", handleToggleAlarm);
  server.on("/setSlider", handleSetSlider);
  server.on("/incrementMode", handleIncrementMode);

  server.begin();
  
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(map(brightness, 40, 100, 40, 255));
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initial LED state
  FastLED.clear(true);
  FastLED.show();
}

void loop() {
  // Update the NTP client
  timeClient.update();

  // Handle web server
  server.handleClient();

  // Check if it's time to trigger the alarm
  checkAlarm();

  // Check the button state and toggle LEDs
  checkButton();

  if (ledsOn) {
    if (mode == 0){ // Alarm mode only
      static float wave = 0;
      
      for (int i = 0; i < NUM_LEDS; ++i) {
        float wavePos = (sin((i * 0.4) + wave) * 0.5 + 0.5) * 255;
        leds[i] = CRGB(wavePos, 0, 0); // Red component only
      }
      
      wave += 0.1; // Adjust speed of wave here
      
      FastLED.show();
      delay(100); 
    }
    else if (mode == 1){
      static uint8_t hue = 0;
      fill_rainbow(leds, NUM_LEDS, hue);
      FastLED.show();
      hue++;
      delay(30);
    }
    else if (mode == 2){
      for (int i = 0; i < NUM_LEDS; i++)
        leds[i] = CRGB::White;
      FastLED.show();
      delay(100);
    }
    else if (mode == 3){
      for (int i = 0; i < NUM_LEDS; i++)
        leds[i] = CRGB::Green;
      FastLED.show();
      delay(100);
    }
    else if (mode == 4){
      for (int i = 0; i < NUM_LEDS; i++)
        leds[i] = CRGB::Blue;
      FastLED.show();
      delay(100);
    }
    else if (mode == 5){
      for (int i = 0; i < NUM_LEDS; i++)
        leds[i] = CRGB::Magenta;
      FastLED.show();
      delay(100);
    }
    else if (mode == 6){
      static uint8_t hue = 0;
      fill_rainbow(leds, NUM_LEDS, hue);
      FastLED.show();
      hue++;
      delay(30);
    }
    else if (mode == 7){
      static uint8_t hue = 0;
      fadeToBlackBy( leds, NUM_LEDS, 10);
      int pos = random16(NUM_LEDS);
      leds[pos] += CHSV( hue + random8(64), 200, 255);
      FastLED.show();
      hue++;
      delay(20);
    }
  }
}
