#include "AssemblyButton.h"
#include "AssemblyLED.h"
#include "LEDMatrixController.h"

#define LED_ROWS 16
#define LED_COLS 16
#define NUM_LEDS 256
#define LED_DATA_PIN 13
#define CENTER_X 8
#define CENTER_Y 8
CRGB leds[NUM_LEDS];
LEDMatrixController mat = LEDMatrixController(LED_ROWS, LED_COLS, leds);

#define UP_BUTTON_PIN 3
#define DOWN_BUTTON_PIN A0
#define LEFT_BUTTON_PIN 2
#define RIGHT_BUTTON_PIN 4
AssemblyButton up = AssemblyButton(UP_BUTTON_PIN);
AssemblyButton down = AssemblyButton(DOWN_BUTTON_PIN);
AssemblyButton left = AssemblyButton(LEFT_BUTTON_PIN);
AssemblyButton right = AssemblyButton(RIGHT_BUTTON_PIN);

#define LED_PIN 12
AssemblyLED led = AssemblyLED(LED_PIN);

typedef enum direction_enum {
  UP,
  DOWN,
  LEFT,
  RIGHT,
  NONE,
} Direction;

typedef struct coordinate_struct {
  uint8_t x;
  uint8_t y;
} Coordinate;

// define colors
CRGB SNAKE = CRGB::Green;
CRGB APPLE = CRGB::Red;
CRGB OFF = CRGB::Black;

// gameplay information
bool dead;
bool should_add_apple;
Direction current_dir;
Direction next_dir;
int timeSinceLastUpdate;

// snake information
Coordinate snake_trail[NUM_LEDS];
int snake_head_index;
int snake_length;

void setup() {
  FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(32);
  resetSnake();
}

void loop() {
  // do nothing if snake is dead, wait for reset
  if (dead) return;

  // read button inputs
  readInputs();

  // only update game once every 150 millseconds (avoid delay(150) since we can't sample the inputs while the MCU sleeps)
  int time = millis();
  if (time - timeSinceLastUpdate < 150) return;
  timeSinceLastUpdate = time;

  // if no buttons have been pressed, the user has not started the game, so do nothing
  if (next_dir == NONE) return;

  // update screen
  FastLED.show();

  // update direction
  current_dir = next_dir;

  // determine where the snake's head would move
  Coordinate next_point = snake_trail[snake_head_index];
  switch (current_dir) {
    case UP:
      next_point.y = (next_point.y + 1) % 16;
      break;
    case DOWN:
      next_point.y = (next_point.y - 1 + 16) % 16;
      break;
    case RIGHT:
      next_point.x = (next_point.x + 1) % 16;
      break;
    case LEFT:
      next_point.x = (next_point.x - 1 + 16) % 16;
      break;
  }

  // if the snake would move on to an LED that shows a snake part, then the snake collides with itself and dies
  if (mat.isColor(next_point.x, next_point.y, SNAKE)) {
    dead = true;
    led.turnOn();
  }

  // remove tail, if snake has not grown
  if (mat.isColor(next_point.x, next_point.y, APPLE)) {
    // if the snake collides with the apple, do not turn off its tail, and allow its length to increase by 1
    snake_length += 1;
    should_add_apple = true;
  } else {
    // if the snake has not collided with the apple, turn off the last led in the snake (to simulate movement of the snake)
    // the snake_trail array is being used circularly, and the snake grows towards positive indexes
    // snake head - (snake length - 1) gives the last led that corresponds to an active snake part
    // ... + num_leds) % num_leds so we always have a positive number
    int tail_index = (snake_head_index - (snake_length - 1) + NUM_LEDS) % NUM_LEDS;
    Coordinate tail = snake_trail[tail_index];
    mat.setColor(tail.x, tail.y, OFF);
  }

  // move snake head
  snake_head_index = (snake_head_index + 1) % NUM_LEDS; // snake grows toward positive indexes in the circular snake_trail array
  snake_trail[snake_head_index] = next_point;  
  mat.setColor(next_point.x, next_point.y, SNAKE);

  // add the apple after moving the snake over the apple (for aesthetic reasons only really)
  if (should_add_apple) {
    add_apple();
  }
}

void resetSnake() {
  // seed random number generator (used for randomizing apple locations)
  randomSeed(millis());

  // turn off all LEDs
  for (int i = 0; i < 256; i++)
    leds[i] = OFF;
  
  // reset game variables for game start    
  dead = false;
  should_add_apple = false;
  current_dir = NONE;
  next_dir = NONE;
  snake_head_index = 0;
  snake_length = 1;
  timeSinceLastUpdate = 0;
  snake_trail[snake_head_index] = {
    .x = CENTER_X,
    .y = CENTER_Y
  };
  led.turnOff();
  mat.setColor(CENTER_X, CENTER_Y, SNAKE);
  add_apple();
  FastLED.show();
}

void add_apple() {
  // to avoid placing an apple over the snake, and also avoiding having to check collisions with the snake
  // randomly decide how many non-snake LEDs (blanks) we will count before placing the apple
  int blanks_to_count = random(255 - snake_length) + 1; // 1 to 255 maximum blanks depending on snake length
  int blanks_counted = 0;
  
  int i = 0; // linear index in the LED strip where the apple will be placed
  
  // count blanks, advance linear index each time, even if the LED is not blank (skip past SNAKE LEDs so APPLE never ends up on SNAKE)
  while (blanks_counted < blanks_to_count) {
    if (leds[i] == OFF) {
      blanks_counted += 1;
    }
    
    i += 1;
  }
  
  leds[i - 1] = APPLE;
  should_add_apple = false;
}

void readInputs() {
  // do not allow user to go back on themselves and instantly die
  // user can send multiple inputs between refreshes, and only the most recent input will count
  if (up.isPressed() && current_dir != DOWN) {
    next_dir = UP;
  } else if (down.isPressed() && current_dir != UP) {
    next_dir = DOWN;
  } else if (left.isPressed() && current_dir != RIGHT) {
    next_dir = LEFT;
  } else if (right.isPressed() && current_dir != LEFT) {
    next_dir = RIGHT;
  }
}
