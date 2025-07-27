# Tiki Drummers Unified Control System

A comprehensive Arduino-based control system for Walt Disney's Enchanted Tiki Room drummer animatronics, featuring synchronized audio, servo movement, NeoPixel lighting effects, and interactive button control.

## Features

### 🎵 Audio & Movement
- **DFPlayer Mini MP3 Module**: Plays synchronized audio tracks
- **Servo Control**: Precise drummer arm movement with variable speed patterns
- **Two-Phase Show**: Slower first half, faster second half drumming sequences

### 💡 LED Effects
- **NeoPixel Strip**: 31 addressable RGB LEDs with multiple animation modes
- **Show Effects**: Introduction lighting, color cycling, lightning effects, and finale sequences
- **Idle Animation**: Beautiful 20-pixel rainbow wave that continuously flows across the strip
- **Eyes LED**: Simple indicator LED that shows when the show is active

### 🎮 Interactive Control
- **Two-Mode System**: Idle mode (ambient lighting) and Show mode (full performance)
- **Button Control**: Single button to start/stop shows with instant response
- **Interruptible Shows**: Press button during performance to immediately return to idle mode
- **Smooth Transitions**: Fade-in effects when entering idle mode

## Hardware Requirements

### Core Components
- **Arduino Nano** (or compatible microcontroller)
- **DFPlayer Mini MP3 Module** with SD card containing audio track
- **Servo Motor** (for drummer arm movement)
- **30+ NeoPixel LED Strip** (WS2812B/WS2811 compatible)
- **Push Button** (momentary, normally open)
- **LED** (for eyes indicator)
- **220Ω Resistor** (for eyes LED)

### Pin Connections
```
Pin 2  → Button (other side to GND, uses internal pull-up)
Pin 3  → Eyes LED (anode, cathode through 220Ω resistor to GND)
Pin 6  → NeoPixel Data Line
Pin 7  → Servo Signal Wire
Pin 10 → DFPlayer RX
Pin 11 → DFPlayer TX
```

### Power Requirements
- **5V Power Supply** capable of handling Arduino + NeoPixels + Servo
- **Recommended**: 5V 3A+ power supply for full brightness operation
- Connect power supply ground to Arduino GND

## Software Dependencies

### Required Libraries
Install these libraries through the Arduino IDE Library Manager:

1. **Servo** (Built-in Arduino library)
2. **SoftwareSerial** (Built-in Arduino library)
3. **DFRobotDFPlayerMini** - For MP3 module control
4. **Adafruit_NeoPixel** - For LED strip control
5. **Button2** - For advanced button handling

### Installation Steps
1. Open Arduino IDE
2. Go to **Sketch → Include Library → Manage Libraries**
3. Search for and install each required library
4. Upload the `Tiki_Drummers_Unified_working.ino` sketch to your Arduino

## Configuration

### Timing Variables
Adjust these values in the code to sync with your audio track:

```cpp
// Show timing (milliseconds)
int introLightDuration = 11000;    // Intro white lights duration
int preShowDelay = 1000;           // Pause before drumming starts
int firstHalfCycles = 53;          // Number of slow drum cycles
int secondHalfCycles = 92;         // Number of fast drum cycles
int betweenHalvesDelay = 800;      // Pause between halves
int lightningFlashes = 40;         // Number of lightning flashes
int finaleArmDelay = 1000;         // Final arm position delay
```

### LED Configuration
```cpp
#define NUM_PIXELS 31              // Number of NeoPixels in your strip
strip.setBrightness(128);          // Global brightness (0-255)
```

### Servo Calibration
```cpp
int servoMin = 1000;               // Minimum servo position
int servoMax = 1900;               // Maximum servo position
```

## Operation

### Power On
1. System starts in **Idle Mode**
2. NeoPixels display flowing rainbow wave animation
3. Eyes LED remains off
4. System waits for button press

### Starting a Show
1. Press the button once
2. Eyes LED turns on
3. Audio begins playing
4. Full light and movement sequence executes

### Stopping a Show
1. Press the button during any part of the show
2. Audio, lights, and movement stop immediately
3. System returns to idle mode with smooth fade-in
4. Eyes LED turns off

### Show Sequence
1. **Introduction** (11 seconds): White light fill effect
2. **First Half**: Slower drumming with color cycling
3. **Transition**: Brief pause and LED reset
4. **Second Half**: Faster drumming with rapid color changes
5. **Lightning Effect**: 40 rapid white flashes
6. **Finale**: Final arm position and LED shutdown

## Customization

### Idle Animation
The idle mode features a 20-pixel rainbow wave. Modify these parameters:
```cpp
int waveLength = 20;               // Number of pixels in wave
int idleAnimationDelay = 150;      // Animation update speed (ms)
float fadeStep = 0.02;             // Fade-in speed
```

### Color Effects
The system uses a color wheel function for smooth rainbow transitions. The `Wheel()` function generates colors from 0-255 for seamless color cycling.

### Audio Setup
1. Format SD card as FAT32
2. Copy your audio file as `0001.mp3` (or adjust `player.play(1)` in code)
3. Ensure audio file matches your timing variables

## Troubleshooting

### Common Issues

**LEDs not working:**
- Check NeoPixel data connection to pin 6
- Verify power supply can handle LED current draw
- Ensure `NUM_PIXELS` matches your actual LED count

**Servo not moving:**
- Check servo connection to pin 7
- Verify servo power supply
- Adjust `servoMin` and `servoMax` values for your servo

**Audio not playing:**
- Check DFPlayer connections (pins 10, 11)
- Verify SD card is formatted as FAT32
- Ensure audio file is named correctly (0001.mp3)

**Button not responding:**
- Check button connection to pin 2 and GND
- Verify Button2 library is installed
- Test button with multimeter for continuity

### Performance Tips
- Use adequate power supply for stable operation
- Keep wiring short and secure to prevent interference
- Test each component individually before full integration

## Credits

- **Original Concept**: Walt Disney's Enchanted Tiki Room
- **Original Arduino Sketches**: Dan Massey (MakerDan) - August 18, 2021
- **Unified System**: Consolidated and enhanced by Cascade AI Assistant
- **Button2 Library**: Lennart Hennigs
- **NeoPixel Library**: Adafruit Industries

## License

This project is provided as-is for educational and hobbyist use. Please respect Disney's intellectual property rights when using this system.

---

*Bring your Tiki Drummer to life with synchronized lights, sound, and movement!* 🥁✨
