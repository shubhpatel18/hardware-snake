#include "FastLED.h"

#define LED_ROWS 16
#define LED_COLS 16
#define NUM_LEDS 256
#define LED_DATA_PIN 12
#define CENTER_X 8
#define CENTER_Y 8
CRGB leds[NUM_LEDS];

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
bool moved_on_to_apple;
Direction snake_dir;
int timeSinceLastUpdate;

// snake information
Coordinate snake_trail[NUM_LEDS];
int snake_head_index;
int snake_length;

void setup() {
  // seed random number generator (used for randomizing apple locations)
  randomSeed(analogRead(0));
  
  // initialize game variables for game start    
  dead = false;
  moved_on_to_apple = false;
  snake_dir = NONE;
  snake_head_index = 0;
  snake_length = 1;
  timeSinceLastUpdate = 0;
  snake_trail[snake_head_index] = {
    .x = CENTER_X,
    .y = CENTER_Y
  };

  // x x 13 12 11 10 9 8 (PORTB Data Direction - Ox04)
  // 7 6 5  4  3  2  1 0 (PORTD Data Direction - 0x0A)
  asm("CBI 0x0A, 3"); // sets pin 3  to input (up)
  asm("CBI 0x04, 5"); // sets pin 13 to input (down)
  asm("CBI 0x0A, 2"); // sets pin 2  to input (left)
  asm("CBI 0x0A, 4"); // sets pin 4  to input (right)

  asm("SBI 0x04, 3"); // sets pin 11 to output (red led)
  asm("SBI 0x04, 4"); // sets pin 12 to output (led grid)

  // prepare screen
  FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(32);
  
  for (int i = 0; i < 256; i++)
    leds[i] = OFF;
  setColor(CENTER_X, CENTER_Y, SNAKE);
  add_apple();
  FastLED.show();

  // turn off red lED
  asm("CBI 0x05, 3"); // x x 13 12 11 10 9 8 -- sets pin 11 to LOW
}

void loop() {
  // do nothing if snake is dead, wait for reset
  if (dead) return;

  // read button input
  snake_dir = getInput();

  // if no buttons have been pressed, the user has not started the game, so do nothing
  if (snake_dir == NONE) return;

  // only update game once every 150 millseconds (avoid delay(150) since we can't sample the inputs while the microcontroller sleeps)
  int time = millis();
  if (time - timeSinceLastUpdate < 150) return;
  timeSinceLastUpdate = time;

  // determine where the snake's head would move
  Coordinate next_point = snake_trail[snake_head_index];
  switch (snake_dir) {
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
  if (isColor(next_point.x, next_point.y, SNAKE)) {
    dead = true;
    asm("SBI 0x05, 3"); // x x 13 12 11 10 9 8 -- sets pin 11 to HIGH (red LED)
  } else {

    moved_on_to_apple = isColor(next_point.x, next_point.y, APPLE);
    // move snake head
    snake_head_index = (snake_head_index + 1) % NUM_LEDS; // snake grows toward positive indexes in the circular snake_trail array
    snake_trail[snake_head_index] = next_point;  
    setColor(next_point.x, next_point.y, SNAKE);

    if (moved_on_to_apple) {
      // if the snake collides with the apple, do not turn off its tail, and allow its length to increase by 1
      snake_length += 1;
      add_apple();
    }
    else {
      // if the snake has not collided with the apple, turn off the last led in the snake (to simulate movement of the snake)
      int tail_index = (snake_head_index - snake_length + NUM_LEDS) % NUM_LEDS;
      Coordinate tail = snake_trail[tail_index];
      setColor(tail.x, tail.y, OFF);
    }
  }

  // update screen
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
  moved_on_to_apple = false;
}

Direction getInput() {
  // does not allow user to go back on themselves and instantly die
  // user can send multiple inputs between refreshes, and only the most recent input will count

  // x x 13 12 11 10 9 8 (PORTB)
  // 7 6 5  4  3  2  1 0 (PORTD)
  // pin 3: up, pin 13: down, pin 2: left, pin 4: right
  if ((PIND & B00001000) && (snake_dir != DOWN)) {
    return UP;
  } else if ((PINB & B00100000) && (snake_dir != UP)) {
    return DOWN;
  } else if ((PIND & B00000100) && (snake_dir != RIGHT)) {
    return LEFT;
  } else if ((PIND & B00010000) && (snake_dir != LEFT)) {
    return RIGHT;
  } else {
    // no button pressed, keep direction the same
    return snake_dir;
  }
}

int setColor(int row, int col, CRGB color) {
  if (col >= LED_COLS || col < 0 || row >= LED_ROWS || row < 0) {
    return 0;
  }

  leds[linearAddr(row, col)] = color;
  return 1;
}

int isColor(int row, int col, CRGB color) {
  if (col >= LED_COLS || col < 0 || row >= LED_ROWS || row < 0) {
    return 0;
  }

  return leds[linearAddr(row, col)] == color;
}

int linearAddr(int row, int col) {
  int addr;

  if (row & 1) {
    // odd rows run backwards
    int reverseX = (LED_ROWS - 1) - col;
    addr = (row * LED_ROWS) + reverseX;
  } else {
    // even rows run forwards
    addr = (row * LED_ROWS) + col;
  }

  return addr;
}
