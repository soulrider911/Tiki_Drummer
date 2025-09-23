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
#define NUM_PIXELS 16 //19 for Original
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Ambient NeoPixel strip (separate) - change pin/count as needed
#define AMBIENT_PIN 5
#define NUM_PIXELS_AMBIENT 28
#define AMBIENT_BRIGHTNESS 150       // 0-255
#define AMBIENT_COLOR_R 255           // Soft warm white by default
#define AMBIENT_COLOR_G 180
#define AMBIENT_COLOR_B 60
Adafruit_NeoPixel ambientStrip = Adafruit_NeoPixel(NUM_PIXELS_AMBIENT, AMBIENT_PIN, NEO_GRB + NEO_KHZ800);

// Use ambient LED count for internal rain effect buffers
#define LED_COUNT NUM_PIXELS_AMBIENT

// ===== Internal frame buffer for ambient rain effect =====
static uint8_t fr[LED_COUNT], fg[LED_COUNT], fb[LED_COUNT]; // per-pixel RGB for ambient

// --- tiny helpers ---
static inline uint8_t qadd8(uint8_t a, uint8_t b) { uint16_t t = a + b; return (t > 255) ? 255 : (uint8_t)t; }
static inline uint8_t scale8(uint8_t v, uint8_t s) { return (uint16_t)v * (uint16_t)s / 255; }

// Random blue/cyan from a soothing palette
static void pickBlueCyan(uint8_t &r, uint8_t &g, uint8_t &b) {
  uint8_t pick = random(0, 100);
  if (pick < 55) {                 // cyan-ish
    r = 0;
    g = 180 + random(0, 76);       // 180–255
    b = 190 + random(0, 66);       // 190–255
  } else {                         // blue-ish with touch of green
    r = 0;
    g = 30 + random(0, 60);        // 30–89
    b = 190 + random(0, 66);       // 190–255
  }
}

// Fade the whole frame by 'fadeBy' (like WLED's fade-out)
static void fadeAll(uint8_t fadeBy) {
  uint8_t keep = 255 - fadeBy;     // 255=keep, 0=drop to black
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    fr[i] = scale8(fr[i], keep);
    fg[i] = scale8(fg[i], keep);
    fb[i] = scale8(fb[i], keep);
  }
}

// Optional background floor so it never looks "empty"
static void applyAmbientFloor(uint8_t floorLevel) {
  if (!floorLevel) return;
  // very dim teal floor
  uint8_t baseG = floorLevel;                           // gentle green
  uint8_t baseB = qadd8(floorLevel, floorLevel/2);      // slightly more blue
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    if ((uint16_t)fr[i] + fg[i] + fb[i] < floorLevel) {
      fr[i] = 0;
      fg[i] = baseG;
      fb[i] = baseB;
    }
  }
}

// Spawn a few single-pixel twinkles (WLED-style random pops)
static void spawnTwinkles(uint8_t density, uint8_t minDarkSum) {
  // Attempts scale with strip length; each attempt spawns with prob ~ density
  uint8_t attempts = LED_COUNT / 6;
  if (attempts < 1) attempts = 1;  // clamp to at least 1
  for (uint8_t k = 0; k < attempts; k++) {
    if (random(0, 256) < density) {
      uint16_t i = random(0, LED_COUNT);
      // Prefer darker spots so twinkles appear distributed
      if ((uint16_t)fr[i] + fg[i] + fb[i] < minDarkSum) {
        uint8_t r, g, b; pickBlueCyan(r, g, b);
        // Random initial brightness (soft cap to avoid harsh whites)
        uint8_t br = 160 + random(0, 96); // 160–255
        r = scale8(r, br);
        g = scale8(g, br);
        b = scale8(b, br);
        fr[i] = qadd8(fr[i], r);
        fg[i] = qadd8(fg[i], g);
        fb[i] = qadd8(fb[i], b);
      }
    }
  }
}

// Draw to ambientStrip using buffers
void ColorTwinklesBlueCyanTick(uint8_t fadeBy, uint8_t spawnDensity, uint8_t minDarkSum, uint8_t floorLevel) {
  fadeAll(fadeBy);
  spawnTwinkles(spawnDensity, minDarkSum);
  applyAmbientFloor(floorLevel);
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    ambientStrip.setPixelColor(i, ambientStrip.Color(fr[i], fg[i], fb[i]));
  }
  ambientStrip.show();
}

// LED diode setup - pin 3 for eyes indicator
#define LED_EYES 3

// Button setup - pin 2 for mode switching
#define BUTTON_PIN 2
// Servo signal pin (hardware wiring uses pin 7)
#define SERVO_PIN 7
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
uint32_t IDLE_COLOR_B = 0xff6e00; // orange
uint32_t IDLE_COLOR_C = 0xd000ff; // purple

// Periods (ms)
const uint32_t IDLE_BREATHE_MS = 10000;   // overall “breathing” cycle
const uint32_t IDLE_DRIFT_MS   = 10000;  // how long the band takes to loop end-to-end

// Band softness/radius (as fraction of strip length)
const float    IDLE_BAND_WIDTH = 0.4f;  // 0.2..0.5 works well; larger = softer, wider glow

// Floor/ceiling brightness (0..1) to keep it calm
const float    IDLE_MIN_BRIGHT = 0.6f;
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
int lightningFlashes = 50;         // Number of lightning flashes
int lightningOnTime = 100;         // How long each flash stays on (milliseconds)
int lightningOffTime = 10;         // Brief pause between flashes (milliseconds)

// Finale timing
int finaleArmDelay = 1000;         // How long to wait for arm to reach final position
int postLightningPause = 7000;     // Pause after final lightning before cleanup (ms)
int postLightningTwinkleGap = 1500; // Gap before ambient twinkles begin (ms)

// Post-lightning rain timing state
unsigned long rainStartMillis = 0; // Set at start of post-lightning pause

// Rain twinkle parameters (ambient strip)
uint8_t rainTwinkleFadeBy = 70;     // Higher = faster fade
uint8_t rainTwinkleDensity = 130;   // 0..255 spawn probability per attempt
uint8_t rainAmbientFloor   = 12;    // 0 for off, small (e.g., 12-20) for faint floor

// Forward declaration for ambient rain effect used during post-lightning pause
void rainAmbientStep();

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
  delay(10); // Short processing time; keep UI responsive
  
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
  pinMode(SERVO_PIN, INPUT);
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
    // Remove blocking blink feedback to allow immediate stop
  }
}

void setup() {
  // Initialize LED eyes pin
  pinMode(LED_EYES, OUTPUT);
  digitalWrite(LED_EYES, LOW); // Start with eyes off
  
  // Initialize Button2 with callback handler
  button.begin(BUTTON_PIN);
  // Trigger immediately on press for faster stop/start response
  button.setPressedHandler(buttonHandler);
  
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
  
  // Seed randomness and clear rain effect frame buffers
  randomSeed(analogRead(A0));
  memset(fr, 0, sizeof(fr));
  memset(fg, 0, sizeof(fg));
  memset(fb, 0, sizeof(fb));
  
  // Initialize serial communication for DFPlayer Mini
  softwareSerial.begin(9600);
  player.begin(softwareSerial);
  player.volume(24); // Set volume to maximum (0 to 30)
  
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
  
  // Interruptible delay for intro lights (finer granularity for faster stop)
  for (int i = 0; i < introLightDuration && showRunning; i += 20) {
    delay(20);
    button.loop(); // Check for button presses during delay
  }
  if (!showRunning) { cleanupShow(); return; }
  
  // Turn off intro lights
  lightning(strip.Color(0, 0, 0), 1);
  
  // Prime show LEDs immediately after intro to avoid a blackout before preShowDelay
  chantStep(ledSpeed1stHalf, 0);
  
  // Interruptible delay before show (finer granularity for faster stop)
  for (int i = 0; i < preShowDelay && showRunning; i += 20) {
    delay(20);
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
    
    // Early LED update at the start of the loop to avoid waiting for servo movement
    if (x % ledUpdateFreq1stHalf == 0 && showRunning) { // Use timing variable
      chantStep(ledSpeed1stHalf, x * ledUpdateFreq1stHalf); // Use timing variables
    }
    
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
  }
  if (!showRunning) { cleanupShow(); return; }
  
  // Brief pause and LED reset between halves
  lightning(strip.Color(0, 0, 0), 1);
  
  // Interruptible delay between halves (finer granularity for faster stop)
  for (int i = 0; i < betweenHalvesDelay && showRunning; i += 20) {
    delay(20);
    button.loop(); // Check for button presses during delay
  }
  if (!showRunning) { cleanupShow(); return; }
  
  // SECOND HALF - Faster drum beat with faster color cycling
  for (int x = 0; x < secondHalfCycles && showRunning; x++) { // Use timing variable
    if (!showRunning) { cleanupShow(); return; } // Check if show was stopped
    
    // Early LED update at the start of the loop to avoid waiting for servo movement
    if (x % ledUpdateFreq2ndHalf == 0 && showRunning) { // Use timing variable
      chantStep(ledSpeed2ndHalf, x * ledUpdateFreq2ndHalf); // Use timing variables
    }
    
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
  }
  if (!showRunning) { cleanupShow(); return; }
  
  // Turn off LEDs
  lightning(strip.Color(0, 0, 0), 1);
  
  // Turn OFF ambient just before lightning event starts
  for (uint16_t i = 0; i < ambientStrip.numPixels(); i++) {
    ambientStrip.setPixelColor(i, ambientStrip.Color(0, 0, 0));
  }
  ambientStrip.show();
  
  // Before lightning: ensure servo is fully detached and its signal line isn't floating
  if (drummerServo.attached()) {
    drummerServo.detach();
    delay(50);
  }
  pinMode(SERVO_PIN, INPUT); // avoid noise on servo signal during power/LED surges
  
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
  // Reattach and set drummer arm to final position
  drummerServo.attach(SERVO_PIN);
  drummerServo.write(97);
  
  // Interruptible delay for finale (finer granularity for faster stop)
  for (int i = 0; i < finaleArmDelay && showRunning; i += 20) {
    delay(20);
    button.loop(); // Check for button presses during delay
  }
  if (!showRunning) { cleanupShow(); return; }
  
  drummerServo.detach();
  
  // Final LED shutdown
  lightning(strip.Color(0, 0, 0), 1);
  
  // Turn off eyes LED
  digitalWrite(LED_EYES, LOW);
  
  // Keep ambient state through the gap; twinkles will start after the configured delay
  // Mark start time of post-lightning pause for twinkle gap control
  rainStartMillis = millis();
  
  // Pause after lightning before cleanup (interruptible)
  for (int i = 0; i < postLightningPause && showRunning; i += 20) {
    // Animate rain twinkles on the ambient strip during the pause
    rainAmbientStep();
    delay(20);
    button.loop(); // Keep button responsive during pause
  }
  if (!showRunning) { cleanupShow(); return; }
  
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
  // Center should traverse the full ring length [0..N) to avoid a tiny jump at wrap
  const float center = tDrift * (float)N;
  const float radius = max(1.0f, IDLE_BAND_WIDTH * (float)N);

  // Tri-color palette cycle: A -> B -> C -> A using the breathing phase
  float phase = tBreathe * 3.0f;         // 0..3
  int seg = (int)phase;                  // 0,1,2, (rarely 3 due to float rounding)
  if (seg >= 3) seg = 0;                 // ensure seamless wrap
  float ft = phase - (float)seg;         // 0..1 within segment
  uint32_t baseColor;
  if (seg == 0) {
    baseColor = lerpColor(IDLE_COLOR_A, IDLE_COLOR_B, ft);
  } else if (seg == 1) {
    baseColor = lerpColor(IDLE_COLOR_B, IDLE_COLOR_C, ft);
  } else {
    baseColor = lerpColor(IDLE_COLOR_C, IDLE_COLOR_A, ft);
  }

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

// WLED-style blue/cyan twinkles for ambient strip
static void ColorTwinklesBlueCyanTick(uint8_t fadeBy, uint8_t density, uint8_t ambientFloor) {
  fadeAll(fadeBy);               // global fade
  spawnTwinkles(density, 70);    // raise minDarkSum for more “pepper”
  applyAmbientFloor(ambientFloor);

  // Push buffer to ambient LEDs
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    ambientStrip.setPixelColor(i, ambientStrip.Color(fr[i], fg[i], fb[i]));
  }
  ambientStrip.show();
}

// Ambient rain twinkle effect step using user's algorithm
void rainAmbientStep() {
  // During the configured gap, don't touch/show ambient at all to avoid a pre-gap fade/flicker
  unsigned long now = millis();
  if (now - rainStartMillis < (unsigned long)postLightningTwinkleGap) {
    return; // keep whatever was on the ambient strip intact
  }
  // After the gap, run the twinkle tick normally
  ColorTwinklesBlueCyanTick(/*fadeBy*/ rainTwinkleFadeBy, /*density*/ rainTwinkleDensity, /*ambientFloor*/ rainAmbientFloor);
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
