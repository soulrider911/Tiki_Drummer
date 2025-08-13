/* 
 * Unified Tiki Drummers Arduino Sketch
 * 
 * This consolidated sketch controls a single Tiki Drummer from Walt Disney's Enchanted Tiki Room.
 * It combines the functionality of both the sound/servo control and NeoPixel LED control into a single Arduino.
 * 
 * Hardware Requirements:
 * - Arduino Nano or compatible
 * - DFPlayer Mini MP3 module (pins 10, 11)
 * - Servo motor for drummer arm (pin 7)
 * - 16 NeoPixel LED strip (pin 6)
 * - SD card with 1 audio file for DFPlayer Mini
 * 
 * Original sketches created by Dan Massey (MakerDan) on August 18, 2021.
 * Consolidated by Cascade AI Assistant.
 */

// Include all necessary libraries
#include <Servo.h>
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include <Adafruit_NeoPixel.h>
#include "Button2.h"


#include <math.h>

// ==== Calm Idle Helpers (added) ====
static inline float easeInOutSine(float t) {
  return 0.5f - 0.5f * cosf(2.0f * PI * t);
}
static inline float fractf(float x) {
  return x - floorf(x);
}
static inline float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}
uint32_t lerpColor(uint32_t c1, uint32_t c2, float t) {
  uint8_t r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
  uint8_t r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;
  uint8_t r = (uint8_t)roundf(lerp(r1, r2, t));
  uint8_t g = (uint8_t)roundf(lerp(g1, g2, t));
  uint8_t b = (uint8_t)roundf(lerp(b1, b2, t));
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
uint32_t scaleColor(uint32_t c, float k) {
  uint8_t r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
  r = (uint8_t)roundf(r * k);
  g = (uint8_t)roundf(g * k);
  b = (uint8_t)roundf(b * k);
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
// ==== End Calm Idle Helpers ====


// DFPlayer Mini setup - pins 10 and 11 for communication
static const uint8_t PIN_MP3_TX = 11; // Connects to module's RX
static const uint8_t PIN_MP3_RX = 10; // Connects to module's TX
SoftwareSerial softwareSerial(PIN_MP3_RX, PIN_MP3_TX);
DFRobotDFPlayerMini player;

// NeoPixel setup - primary drum strip on pin 6
#define NEOPIXEL_PIN 6
#define NUM_PIXELS 19
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Ambient NeoPixel strip (separate) - change pin/count as needed
#define AMBIENT_PIN 5
#define NUM_PIXELS_AMBIENT 32
#define AMBIENT_BRIGHTNESS 180         // 0-255
#define AMBIENT_COLOR_R 255           // Soft warm white by default
#define AMBIENT_COLOR_G 214
#define AMBIENT_COLOR_B 170
Adafruit_NeoPixel ambientStrip = Adafruit_NeoPixel(NUM_PIXELS_AMBIENT, AMBIENT_PIN, NEO_GRB + NEO_KHZ800);

// LED diode setup - pin 3 for eyes indicator
#define LED_EYES 3

// Button setup - pin 2 for mode switching
#define BUTTON_PIN 2
Button2 button;

// Mode control variables
bool isShowMode = false;
bool showRunning = false;

// Idle animation variables
unsigned long lastIdleUpdate = 0;
int idleAnimationDelay = 150; // Milliseconds between animation updates
int idleWavePosition = 0;
int idleColorIndex = 0;

// Fade-in variables for smooth idle mode transition
float idleFadeBrightness = 0.0;
bool idleFadeActive = false;
float fadeStep = 0.02; // How much to increase brightness each frame


// ---- Calm Idle Configuration (added) ----
// Deep ocean + teal palette
uint32_t IDLE_COLOR_A = 0xDE35E6; // deep ocean
uint32_t IDLE_COLOR_B = 0x00E6CF; // teal

// Periods (ms)
const uint32_t IDLE_BREATHE_MS = 4000;   // overall “breathing” cycle
const uint32_t IDLE_DRIFT_MS   = 16000;  // how long the band takes to loop end-to-end

// Band softness/radius (as fraction of strip length)
const float    IDLE_BAND_WIDTH = 0.35f;  // 0.2..0.5 works well; larger = softer, wider glow

// Floor/ceiling brightness (0..1) to keep it calm
const float    IDLE_MIN_BRIGHT = 0.4f;
const float    IDLE_MAX_BRIGHT = 0.9f;

unsigned long  idleStartMillis = 0;
// ---- End Calm Idle Configuration (added) ----
// Servo setup for single drummer
Servo drummerServo;
int pos = 0;
int servoMin = 1000;
int servoMax = 1900;

// Global variables
int numSongs = 1; // Number of tracks on the SD card

// TIMING VARIABLES - Adjust these to sync lighting effects with your music
// Introduction timing
int introLightDuration = 11000;     // How long intro white lights stay on (milliseconds)
int preShowDelay = 1000;           // Pause before main show starts (milliseconds)

// Main show timing
int firstHalfCycles = 53;          // Number of drum cycles for first half
int secondHalfCycles = 92;         // Number of drum cycles for second half
int betweenHalvesDelay = 800;      // Pause between first and second half (milliseconds)

// LED update frequency during drumming
int ledUpdateFreq1stHalf = 5;      // Update LEDs every Nth drum cycle (first half)
int ledUpdateFreq2ndHalf = 3;      // Update LEDs every Nth drum cycle (second half)
int ledSpeed1stHalf = 4;           // LED color cycling speed (first half)
int ledSpeed2ndHalf = 1;           // LED color cycling speed (second half)

// Lightning effect timing
int lightningFlashes = 40;         // Number of lightning flashes
int lightningOnTime = 100;         // How long each flash stays on (milliseconds)
int lightningOffTime = 10;         // Brief pause between flashes (milliseconds)

// Finale timing
int finaleArmDelay = 1000;         // How long to wait for arm to reach final position

// Idle animation function - lights up pixels one at a time with fade-in
void updateIdleAnimation() {
// Calm idle: render on both strips
renderCalmIdle(strip);
renderCalmIdle(ambientStrip);
}


// Cleanup function to ensure proper state reset
void cleanupShow() {
  // Stop audio playback and reset player
  player.stop();
  delay(100); // Give DFPlayer time to process stop command
  
  // Turn off all NeoPixel LEDs and refresh strip
  for(uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  strip.show();
  delay(50);
  
  // Turn off ambient strip
  for (uint16_t i = 0; i < ambientStrip.numPixels(); i++) {
    ambientStrip.setPixelColor(i, ambientStrip.Color(0, 0, 0));
  }
  ambientStrip.show();
  
  // Turn off eyes LED
  digitalWrite(LED_EYES, LOW);
  
  // Detach servo to stop any movement
  if (drummerServo.attached()) {
    drummerServo.detach();
  }
  delay(100); // Give servo time to detach
  
  // Reset show state
  showRunning = false;
  isShowMode = false;
  
  // Initialize fade-in for smooth transition to idle mode
  idleFadeBrightness = 0.0;
  idleFadeActive = true;
  
  // Small delay to ensure all systems are reset
  delay(200);
}

// Button callback function
void buttonHandler(Button2& btn) {
  if (!isShowMode) {
    // Switch from idle to show mode
    isShowMode = true;
    showRunning = true;
    runTikiDrummersShow();
  } else {
    // Signal to stop the show (let the show function handle cleanup)
    showRunning = false;
    
    // Immediate visual feedback that button was pressed during show
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_EYES, HIGH);
      delay(100);
      digitalWrite(LED_EYES, LOW);
      delay(100);
    }
  }
}

void setup() {
  // Initialize LED eyes pin
  pinMode(LED_EYES, OUTPUT);
  digitalWrite(LED_EYES, LOW); // Start with eyes off
  
  // Initialize Button2 with callback handler
  button.begin(BUTTON_PIN);
  button.setTapHandler(buttonHandler);
  
  // Initialize NeoPixel strip
  strip.begin();
  strip.setBrightness(128); // Set global brightness (0-255, default is 255)
  strip.show(); // Initialize all pixels to 'off'
  
  // Initialize Ambient NeoPixel strip
  ambientStrip.begin();
  ambientStrip.setBrightness(AMBIENT_BRIGHTNESS);
  // Ensure ambient is off at boot
  for (uint16_t i = 0; i < ambientStrip.numPixels(); i++) {
    ambientStrip.setPixelColor(i, ambientStrip.Color(0, 0, 0));
  // Initialize calm idle timer
  idleStartMillis = millis();
}
  ambientStrip.show();
  
  // Initialize serial communication for DFPlayer Mini
  softwareSerial.begin(9600);
  player.begin(softwareSerial);
  player.volume(25); // Set volume to maximum (0 to 30)
  
  // Wait 2 seconds after power on
  delay(2000);
  
  // System starts in idle mode - initialize fade-in for smooth startup
  idleFadeBrightness = 0.0;
  idleFadeActive = true;
}

void loop() {
  // Handle button events using Button2 library
  button.loop();
  
  // Run idle animation when not in show mode
  if (!isShowMode && !showRunning) {
    unsigned long currentTime = millis();
    if (currentTime - lastIdleUpdate >= idleAnimationDelay) {
      updateIdleAnimation();
      lastIdleUpdate = currentTime;
    }
  }
  
  // Small delay to prevent excessive CPU usage
  delay(10);
}

void runTikiDrummersShow() {
  // Ensure clean state before starting
  if (drummerServo.attached()) {
    drummerServo.detach();
    delay(100);
  }
  
  // Clear any existing NeoPixel state
  for(uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  strip.show();
  delay(100);
  
  // Turn on eyes LED
  digitalWrite(LED_EYES, HIGH);
  
  // Start the single audio track
  player.play(1);
  delay(100); // Give DFPlayer time to start
  
  // Turn ON ambient strip for the duration of intro, first and second halves
  for (uint16_t i = 0; i < ambientStrip.numPixels(); i++) {
    ambientStrip.setPixelColor(i, ambientStrip.Color(AMBIENT_COLOR_R, AMBIENT_COLOR_G, AMBIENT_COLOR_B));
  }
  ambientStrip.show();
  
  // INTRODUCTION SEQUENCE - Brief intro with white lights
  if (!showRunning) { cleanupShow(); return; } // Check if show was stopped
  intro(strip.Color(255, 255, 255), 5);
  
  // Interruptible delay for intro lights
  for (int i = 0; i < introLightDuration && showRunning; i += 100) {
    delay(100);
    button.loop(); // Check for button presses during delay
  }
  if (!showRunning) { cleanupShow(); return; }
  
  // Turn off intro lights
  lightning(strip.Color(0, 0, 0), 1);
  
  // Interruptible delay before show
  for (int i = 0; i < preShowDelay && showRunning; i += 100) {
    delay(100);
    button.loop(); // Check for button presses during delay
  }
  if (!showRunning) { cleanupShow(); return; }
  
  // Ensure servo is detached first, then attach for drummer arm movement
  if (drummerServo.attached()) {
    drummerServo.detach();
  }
  drummerServo.attach(7);
  
  // FIRST HALF - Slower drum beat with color cycling
  for (int x = 0; x < firstHalfCycles && showRunning; x++) { // Use timing variable
    if (!showRunning) { cleanupShow(); return; } // Check if show was stopped
    
    // Move drummer arm (slower beat)
    for (pos = servoMin; pos <= servoMax && showRunning; pos += 40) {
      drummerServo.write(pos);
      delay(20);
      button.loop(); // Check for button presses during servo movement
    }
    for (pos = servoMax; pos >= servoMin && showRunning; pos -= 40) {
      drummerServo.write(pos);
      delay(20);
      button.loop(); // Check for button presses during servo movement
    }
    
    // Update LED colors during drummer arm movement
    if (x % ledUpdateFreq1stHalf == 0 && showRunning) { // Use timing variable
      chantStep(ledSpeed1stHalf, x * ledUpdateFreq1stHalf); // Use timing variables
    }
  }
  if (!showRunning) { cleanupShow(); return; }
  
  // Brief pause and LED reset between halves
  lightning(strip.Color(0, 0, 0), 1);
  
  // Interruptible delay between halves
  for (int i = 0; i < betweenHalvesDelay && showRunning; i += 100) {
    delay(100);
    button.loop(); // Check for button presses during delay
  }
  if (!showRunning) { cleanupShow(); return; }
  
  // SECOND HALF - Faster drum beat with faster color cycling
  for (int x = 0; x < secondHalfCycles && showRunning; x++) { // Use timing variable
    if (!showRunning) { cleanupShow(); return; } // Check if show was stopped
    
    // Move drummer arm (faster beat)
    for (pos = servoMin; pos <= servoMax && showRunning; pos += 60) {
      drummerServo.write(pos);
      delay(20);
      button.loop(); // Check for button presses during servo movement
    }
    for (pos = servoMax; pos >= servoMin && showRunning; pos -= 60) {
      drummerServo.write(pos);
      delay(20);
      button.loop(); // Check for button presses during servo movement
    }
    
    // Update LED colors during drummer arm movement
    if (x % ledUpdateFreq2ndHalf == 0 && showRunning) { // Use timing variable
      chantStep(ledSpeed2ndHalf, x * ledUpdateFreq2ndHalf); // Use timing variables
    }
  }
  if (!showRunning) { cleanupShow(); return; }
  
  // Turn off LEDs
  lightning(strip.Color(0, 0, 0), 1);
  
  // Turn OFF ambient just before lightning event starts
  for (uint16_t i = 0; i < ambientStrip.numPixels(); i++) {
    ambientStrip.setPixelColor(i, ambientStrip.Color(0, 0, 0));
  }
  ambientStrip.show();
  
  // LIGHTNING EFFECT
  for (int x = 0; x < lightningFlashes && showRunning; x++) { // Use timing variable
    if (!showRunning) { cleanupShow(); return; } // Check if show was stopped
    lightning(strip.Color(255, 255, 255), 1); // Flash white
    delay(lightningOnTime); // Use timing variable
    button.loop(); // Check for button presses during lightning
    if (!showRunning) { cleanupShow(); return; }
    lightning(strip.Color(0, 0, 0), lightningOffTime); // Use timing variable
    button.loop(); // Check for button presses during lightning
  }
  if (!showRunning) { cleanupShow(); return; }
  
  // FINALE
  // Set drummer arm to final position
  drummerServo.write(97);
  
  // Interruptible delay for finale
  for (int i = 0; i < finaleArmDelay && showRunning; i += 100) {
    delay(100);
    button.loop(); // Check for button presses during delay
  }
  if (!showRunning) { cleanupShow(); return; }
  
  drummerServo.detach();
  
  // Final LED shutdown
  lightning(strip.Color(0, 0, 0), 1);
  
  // Turn off eyes LED
  digitalWrite(LED_EYES, LOW);
  
  // Show completed naturally, clean up and return to idle mode
  cleanupShow();
}

void stopShow() {
  // Use the centralized cleanup function
  cleanupShow();
}

// ---- Calm Idle Renderer (added) ----
void renderCalmIdle(Adafruit_NeoPixel &s) {
  const int N = s.numPixels();
  if (N <= 0) return;

  const unsigned long now = millis();
  const float tBreathe = fmodf((now - idleStartMillis), (float)IDLE_BREATHE_MS) / (float)IDLE_BREATHE_MS; // 0..1
  const float tDrift   = fmodf((now - idleStartMillis), (float)IDLE_DRIFT_MS)   / (float)IDLE_DRIFT_MS;   // 0..1

  const float breathe = lerp(IDLE_MIN_BRIGHT, IDLE_MAX_BRIGHT, easeInOutSine(tBreathe));
  const float center = tDrift * (N - 1);
  const float radius = max(1.0f, IDLE_BAND_WIDTH * (float)N);
  const float paletteMix = 0.5f + 0.5f * cosf(2.0f * PI * (tBreathe + 0.25f));
  const uint32_t baseColor = lerpColor(IDLE_COLOR_A, IDLE_COLOR_B, paletteMix);

  for (int i = 0; i < N; ++i) {
    float d = fabsf((float)i - center);
    d = min(d, (float)N - d);
    float w = 0.0f;
    if (d < radius) {
      float x = d / radius;
      w = 0.5f * (1.0f + cosf(x * PI));
    }
    float k = breathe * w;
    uint32_t c = scaleColor(baseColor, k);
    s.setPixelColor(i, c);
  }
  s.show();
}
// ---- End Calm Idle Renderer (added) ----

// LED CONTROL FUNCTIONS

// Introduction LEDs - fills LEDs one after another with specified color
void intro(uint32_t c, uint8_t wait) {
  for(uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(20);
  }
}

// Lightning LEDs - instantly sets all LEDs to specified color
void lightning(uint32_t c, uint8_t wait) {
  for(uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
  }
  strip.show();
  delay(wait);
}

// Chant step - single step of color cycling for integration with drummer servo movement
void chantStep(uint8_t wait, uint16_t colorOffset) {
  for(uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, Wheel((i + colorOffset) & 255));
  }
  strip.show();
  delay(wait);
}

// Original chant function - continuous color cycling (kept for reference)
void chant(uint8_t wait) {
  uint16_t i, j;
  for(j = 0; j < 256; j++) {
    for(i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

// Color wheel function - generates rainbow colors
// Input a value 0 to 255 to get a color value
// The colors are a transition r - g - b - back to r
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
