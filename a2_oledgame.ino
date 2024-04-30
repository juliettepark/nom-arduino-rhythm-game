// Juliette Park
// April, 2024
// A2: OLED Game

// NOM: A rhythm game where pancakes fall from the sky and must be caught/eaten 
// by a husky controlled by the user at the bottom of the screen.
// Upon consumption, pancakes release musical notes that together play the Mario Theme Song.
// Every full iteration causes the pancakes to fall faster, and 3 "misses" are allowed as the husky
// will be very displeased if the pancakes go to waste. Haptic feedback is provided when the user misses a pancake.

// SOURCES
// Mario Melody - Modified from https://www.hackster.io/techarea98/super-mario-theme-song-with-piezo-buzzer-and-arduino-2cc461 

// This sketch uses libraries and contains parts based on Jon Froehlich's Flappy Bird OLED Game: 
// https://github.com/makeabilitylab/arduino/blob/master/OLED/FlappyBird/FlappyBird.ino 
// As well as Shape.hpp library: https://github.com/makeabilitylab/arduino/blob/master/MakeabilityLab_Arduino_Library/src/Shape.hpp



#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Shape.hpp>
#define SCREEN_WIDTH 128 // OLED _display width, in pixels
#define SCREEN_HEIGHT 64 // OLED _display height, in pixels

// Declaration for an SSD1306 _display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// JOYSTICK
const int JOY_X_PIN = A2;
const int JOY_Y_PIN = A1;
const int JOY_MAXVAL = 1023;
const int BUTTON_INPUT_PIN = 5;
int pastState = 1; // default unpressed
const int NUMBER_OF_MODES = 2;
const int OFF = 1;
const int ON = 0;

// HAPTIC FEEDBACK
const int VIBROMOTOR_PIN = 6;
const int VIBROMOTOR_DURATION_MS = 200;
unsigned long _vibroTimeStamp = -1;

// SCORE + STATS
int _totalPoints = 0;
const int PANCAKE_POINT = 1;
int _lives = 3;
int _cycle = 1;

// DOUBLE POINTS LED
const int LED_OUTPUT_PIN = 11;
unsigned long _led_prev_millis = 0;
const int DOUBLE_PT_DURATION = 5000; // 4 sec
int double_pt_interval = random(10000, 20000); // every 10 - 20 sec, double pt begins
bool is_double_pt = false;

// SOUND
const int OUTPUT_PIEZO_PIN = 9;
const int NUM_NOTES = 41;
int currNote = 0;
#include "pitches.h"

int melody[] = {
  NOTE_E7, NOTE_E7, NOTE_E7,
  NOTE_C7, NOTE_E7,
  NOTE_G7,
  NOTE_G6,

  NOTE_C7, NOTE_G6,
  NOTE_E6,
  NOTE_A6, NOTE_B6,
  NOTE_AS6, NOTE_A6,

  NOTE_G6, NOTE_E7, NOTE_G7,
  NOTE_A7, NOTE_F7, NOTE_G7,
  NOTE_E7, NOTE_C7,
  NOTE_D7, NOTE_B6,

  NOTE_C7, NOTE_G6,
  NOTE_E6,
  NOTE_A6, NOTE_B6,
  NOTE_AS6, NOTE_A6,

  NOTE_G6, NOTE_E7, NOTE_G7,
  NOTE_A7, NOTE_F7, NOTE_G7,
  NOTE_E7, NOTE_C7,
  NOTE_D7, NOTE_B6
};

int noteDurations[] = {
  12, 12, 12,
  12, 12,
  12,
  12,

  12, 12,
  12,
  12, 12,
  12, 12,

  9, 9, 9,
  12, 12, 12,
  12, 12,
  12, 12,

  12, 12,
  12,
  12, 12,
  12, 12,

  9, 9, 9,
  12, 12, 12,
  12, 12,
  12, 12
};

// PANCAKE PLATE (SPRITE)
#define PLATE_WIDTH 19
#define PLATE_HEIGHT 13
const int SPEED_DIVISOR = 1; // if want 5x as slow, set to 5
int platex = SCREEN_WIDTH / 2 - PLATE_WIDTH / 2; // start center of screen
int platey = SCREEN_HEIGHT - PLATE_HEIGHT; // put at bottom of screen

class Plate : public Rectangle {
  public:
    Plate(int x, int y, int width, int height) : Rectangle(x, y, width, height)
    {
    }
};

Plate _plate(platex, platey, PLATE_WIDTH, PLATE_HEIGHT);

// PANCAKES
const int NUM_PANCAKES = 8;
const int PANCAKE_HEIGHT = 3;
const int PANCAKE_WIDTH = 5;
const int FALL_SPEED = 1;

// Creates pancakes proportionally to notes (ex. after 4 beats, create next pancake)
const double GENERATION_SPEED_MULTIPLIER = 1;
// Use blink without delay logic to track melody time
unsigned long previousMillis = 0;

class Pancake : public Rectangle {
  protected:
    bool _onscreen;
    int _note = 0; // THIS IS INDEX IN MELODY
    bool _touchplate = false;

  public:
    Pancake(int x, int y, int width, int height) : Rectangle(x, y, width, height)
    {
    }

    void setOnScreen(bool onscreen) {
      _onscreen = onscreen;
    }

    bool getOnScreen() {
      return _onscreen;
    }

    bool getTouchPlate() {
      return _touchplate;
    }

    void setTouchPlate(bool touched) {
      _touchplate = touched;
    }

    int getNote() {
      return _note;
    }

    void setNote(int note) {
      _note = note;
    }
};

// Cycles through array of pancakes
// Once off screen, will reassign location and re-release from top
Pancake _fallingPancakes[NUM_PANCAKES] = { Pancake(0, 0, 0, 0),
                              Pancake(0, 0, 0, 0),
                              Pancake(0, 0, 0, 0),
                              Pancake(0, 0, 0, 0),
                              Pancake(0, 0, 0, 0),
                              Pancake(0, 0, 0, 0),
                              Pancake(0, 0, 0, 0),
                              Pancake(0, 0, 0, 0)
                            };



// SPRITE
const unsigned char cherry_sprite [] PROGMEM = {
	0x39, 0xc0, 0x00, 0x29, 0x40, 0x00, 0x6f, 0x40, 0x00, 0x4e, 0x60, 0x00, 0xc4, 0x30, 0x00, 0x91, 
	0x18, 0x00, 0x80, 0x18, 0x00, 0x8e, 0x0c, 0x00, 0x84, 0x07, 0xc0, 0x80, 0x00, 0xc0, 0xc0, 0x1f, 
	0xc0, 0x60, 0x30, 0x00, 0x3f, 0xe0, 0x00
};

// HOMESCREEN / ENDSCREEN
int16_t x, y;
uint16_t textWidth, textHeight;
const char strTitle[] = "nom";
const char strplay[] = "press to play";
const char strEnd[] = "press to try again";
const int TOPOFFSET = 1;
const unsigned char homescreen [] PROGMEM = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x9c, 0x3d, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x9c, 0x3d, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x9f, 0xf0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x0f, 0xc0, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x0f, 0xc0, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x03, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x03, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x30, 0x30, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x30, 0x30, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x18, 0x30, 0x30, 0x07, 0x80, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0xf1, 0xfc, 0x00, 0x18, 0x00, 0x00, 0x07, 0x80, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x01, 0x9f, 0x07, 0x00, 0x18, 0x0f, 0xc0, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00, 0x18, 0x0f, 0xc0, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x07, 0xf0, 0x07, 0x00, 0x18, 0x03, 0x00, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x04, 0x1f, 0xfd, 0x00, 0x18, 0x03, 0x00, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x60, 0xfc, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x07, 0xc0, 0x03, 0x00, 0x06, 0x00, 0x00, 0x00, 0x67, 0xcc, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x3c, 0x7f, 0xff, 0xe0, 0x06, 0x00, 0x00, 0x00, 0x67, 0xcc, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x24, 0x00, 0x01, 0x20, 0x07, 0x80, 0x00, 0x00, 0x78, 0x3c, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x27, 0xe0, 0x03, 0x20, 0x07, 0x80, 0x00, 0x00, 0x78, 0x3c, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x20, 0x3f, 0xfe, 0x20, 0x01, 0xfc, 0x00, 0xff, 0xe7, 0xf0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x20, 0x01, 0xfc, 0x00, 0xff, 0xe7, 0xf0, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x60, 0x00, 0x0f, 0xff, 0x80, 0x7e, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// GAME STATE
enum GameState {
  NEW_GAME,
  PLAYING,
  GAME_OVER,
};

GameState _gameState = NEW_GAME;

void setup(){
  Serial.begin(9600);
  pinMode(BUTTON_INPUT_PIN, INPUT_PULLUP); // Joystick button
  pinMode(VIBROMOTOR_PIN, OUTPUT); // Vibromotor
  
  // Initialize the display. If it fails, print failure to Serial
  // and enter an infinite loop
  if (!_display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  randomSeed(analogRead(A5));
  initializePancakes();
  homepageState();
}

// Give each pancake random x-value and ready to release from top of screen
void initializePancakes() {
  for (int i = 0; i < NUM_PANCAKES; i++) {
    resetPancake(i);
  }
}

// Reassign random place for pancake
void resetPancake(int pancakeNum) {
  _fallingPancakes[pancakeNum].setDimensions(PANCAKE_WIDTH, PANCAKE_HEIGHT);
  _fallingPancakes[pancakeNum].setLocation(random(0, SCREEN_WIDTH - PANCAKE_WIDTH), 0); // random x, start at top
  _fallingPancakes[pancakeNum].setOnScreen(false); // default not shown
  _fallingPancakes[pancakeNum].setTouchPlate(false);
}

void loop(){
  
  if(_gameState == NEW_GAME) {

    // Press joystick to trigger game
    if(checkButton()) {
      Serial.print("Button pressed, ");
      Serial.println(_gameState);
      _gameState = PLAYING;
    }
  } else if(_gameState == PLAYING) {
    playState();
  } else {
    gameOverState();
  }

  _display.display();
}

// Display homepage
void homepageState() {
  _display.clearDisplay();
  _display.drawBitmap(0, 0, homescreen, 128, 64, WHITE);
  // Setup text rendering parameters
  _display.setTextSize(1);
  _display.setTextColor(WHITE, BLACK);

  // NOM TITLE
  _display.getTextBounds(strTitle, 0, 0, &x, &y, &textWidth, &textHeight);
  // Center the text on the display (both horizontally and vertically)
  _display.setCursor(_display.width() / 2 - textWidth / 2, TOPOFFSET);
  // _display.setCursor(_display.width() / 2 - textWidth / 2, TOPOFFSET);
  // Print out the string
  _display.print(strTitle);

  // INSTRUCTIONS
  _display.getTextBounds(strplay, 0, 0, &x, &y, &textWidth, &textHeight);
  // Center the text on the display (both horizontally and vertically)
  _display.setCursor(_display.width() / 2 - textWidth / 2, (TOPOFFSET + textHeight / 2)*2);
  // Print out the string
  _display.print(strplay);
  
}

// Read input to see if button has been pressed
bool checkButton() {
  int button = digitalRead(BUTTON_INPUT_PIN);
  bool pressed = (pastState == OFF && button == ON);
  pastState = button; // log last button state
  return pressed;
}

void playState() {
  _display.clearDisplay();
  checkReleasePancake(); // Do I release another pancake?
  displayInGamePancakes(); // Display all in-game pancakes
  joystickMovement(); // Move sprite according to input
  checkDoublePts();
  drawScore(); // display game stats
}

// Checks if next pancake is ready to be released
// Checks timings and releases pancakes "on beat" - proportionally to noteDurations -
// so that the melody plays on beat when caught at the bottom.
void checkReleasePancake() {
  unsigned long currentMillis = millis();
  unsigned int interval = 10000 / (noteDurations[currNote] * GENERATION_SPEED_MULTIPLIER); // convert beats -> millisec

  // Do I generate new pancake?
  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    currNote++;
    if(currNote > NUM_NOTES-1) {
      currNote = 0;
      _cycle++; // completed a level
    }

    // Show the next pancake in line
    int i = currNote % NUM_PANCAKES;
    if(_fallingPancakes[i].getOnScreen()) {
    }

    // Get new location for pancake
    resetPancake(i);
    _fallingPancakes[i].setOnScreen(1);
    _fallingPancakes[i].setNote(currNote);
  }
}

// Draw in-game pancakes to the OLED display
// Check for new off-screen pancakes (missed and eaten)
// Play melody for eaten pancakes
void displayInGamePancakes() {
  for(int i = 0; i < NUM_PANCAKES; i++) {
    
    if(_fallingPancakes[i].getOnScreen()) {
      // Increment cakes
      _fallingPancakes[i].setY(_fallingPancakes[i].getY() + _cycle); // more cycles means faster pancakes

      if(_fallingPancakes[i].getTop() >= SCREEN_HEIGHT) { // OFFSCREEN
        _fallingPancakes[i].setOnScreen(false);
        missPancake();
        
      } else if(_fallingPancakes[i].overlaps(_plate) && !_fallingPancakes[i].getTouchPlate()) { // EATEN

        // Remove pancake and increment score
        _fallingPancakes[i].setTouchPlate(true);
        _fallingPancakes[i].setOnScreen(false);
        _totalPoints += PANCAKE_POINT;
        if(is_double_pt) {
          // Give double points!
          _totalPoints += PANCAKE_POINT;
        }

        // Play sound
        int duration = 1000 / noteDurations[_fallingPancakes[i].getNote()];
        tone(OUTPUT_PIEZO_PIN, melody[_fallingPancakes[i].getNote()], duration*1.3);
        // delay(duration * 1.30);
      } else {
        _fallingPancakes[i].draw(_display);
      }
    }
  }

  // Check to turn off vibromotor
  if(_vibroTimeStamp != -1){
    if(millis() - _vibroTimeStamp > VIBROMOTOR_DURATION_MS){
      _vibroTimeStamp = -1;
      digitalWrite(VIBROMOTOR_PIN, LOW);
    }
  }
}

// Trigger consequences for missed pancake
void missPancake() {
  // Decrement life
  _lives--;
  if(_lives == 0) {
    _gameState = GAME_OVER;
  }

  // Set haptic feedback
  digitalWrite(VIBROMOTOR_PIN, HIGH);
  _vibroTimeStamp = millis();
}

// Moves sprite within bounds of screen using joystick input
void joystickMovement() {
  int joyxval = analogRead(JOY_X_PIN);
  int moveAmt = 0;

  // If joystick pushed in R/L
  if(joyxval != 512) {
    joyxval = map(joyxval, 0, JOY_MAXVAL, 0, SCREEN_WIDTH-PLATE_WIDTH); // scale into screen width
    moveAmt = joyxval - (SCREEN_WIDTH-PLATE_WIDTH) / 2; // find displacement from center
  }

  int platex = _plate.getX() + moveAmt / SPEED_DIVISOR; // adjust movement speed and increment plate

  // Prevent from going off screen
  if(platex > SCREEN_WIDTH - PLATE_WIDTH) {
    platex = SCREEN_WIDTH - PLATE_WIDTH;
  } else if(platex < 0) {
    platex = 0;
  }

  _plate.setX(platex);
  int spriteX = _plate.getX();
  int spriteY = _plate.getY();
  _display.drawBitmap(spriteX, spriteY, cherry_sprite, PLATE_WIDTH, PLATE_HEIGHT, WHITE);
}

// Display score, level, and lives left
void drawScore() {
  _display.setTextSize(1);
  _display.setTextColor(WHITE, BLACK);
  _display.getTextBounds("score: ", 0, 0, &x, &y, &textWidth, &textHeight);
  _display.setCursor(2, TOPOFFSET);

  // Print out game over message
  _display.print("score: ");
  _display.println(_totalPoints);

   // LEVEL
   _display.setCursor(2, TOPOFFSET + textHeight + 2);
  _display.print("lvl: ");
  _display.print(_cycle);

  // DISPLAY LIVES
  _display.setCursor(SCREEN_WIDTH - 25, TOPOFFSET);
  _display.write(3); // for heart
  _display.print(": ");
  _display.print(_lives);
}

void checkDoublePts() {
  unsigned long currMillis = millis();

  if(!is_double_pt) {
    if(currMillis - _led_prev_millis >= double_pt_interval) {
      Serial.print("Interval was: ");
      Serial.println(double_pt_interval);
      // Set new interval
      double_pt_interval = random(10000, 20000);
      _led_prev_millis = currMillis;

      // Turn on blue LED
      digitalWrite(LED_OUTPUT_PIN, HIGH);

      // Turn on boolean
      is_double_pt = true;
    }
  } else {
    // Currently double pt
    // Turn off after duration
    if(currMillis >= _led_prev_millis + DOUBLE_PT_DURATION) {

      // Reset timestamp
      _led_prev_millis = currMillis;

      // Turn off LED
      digitalWrite(LED_OUTPUT_PIN, LOW);

      // Turn off boolean
      is_double_pt = false;
    }
  }
  

}

// Display game over bitmap and check to button press to reset game
void gameOverState() {

  // If vibromotor stuck
  if(_vibroTimeStamp != -1){
    if(millis() - _vibroTimeStamp > VIBROMOTOR_DURATION_MS){
      _vibroTimeStamp = -1;
      digitalWrite(VIBROMOTOR_PIN, LOW);
    }
  }

  // If LED stuck
  // reset timestamps
  previousMillis = 0;
  _led_prev_millis = 0;

  // Turn off LED
  digitalWrite(LED_OUTPUT_PIN, LOW);
  is_double_pt = false;

  _display.clearDisplay();
  _display.drawBitmap(0, 0, homescreen, 128, 64, WHITE);
  // Setup text rendering parameters
  _display.setTextSize(1);
  _display.setTextColor(WHITE, BLACK);
  _display.getTextBounds(strEnd, 0, 0, &x, &y, &textWidth, &textHeight);

  // Center horizontally
  _display.setCursor(_display.width() / 2 - textWidth / 2, TOPOFFSET + 3);

  // Print out game over message
  _display.println(strEnd);
  if(checkButton()) {
    resetEntitiesAndStats();
    _gameState = PLAYING;
  }
}

void resetEntitiesAndStats() {
  // reset stats
  _lives = 3;
  _totalPoints = 0;
  currNote = 0; // also resets durations (indexed with )
  _cycle = 1;

  // reset pancakes
  initializePancakes();
}
