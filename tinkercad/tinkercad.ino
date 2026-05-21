#include <Adafruit_LEDBackpack.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_NeoPixel.h>
#include <Keypad.h>

/////////////////////////////////////////////////////////
// timer.h
/////////////////////////////////////////////////////////

typedef unsigned long timer_ms;

void timer_reset(timer_ms& timer);
bool timer_elapsed(timer_ms& timer, unsigned long limit);

/////////////////////////////////////////////////////////
// cabin.h
/////////////////////////////////////////////////////////

#define PIN_MOTOR_CABIN_A  2
#define PIN_MOTOR_CABIN_B  3
#define PIN_MOTOR_DOORS_A  4
#define PIN_MOTOR_DOORS_B  5

#define CABIN_DOOR_OPEN  -1
#define CABIN_DOOR_CLOSE  1
#define CABIN_DOOR_STOP   0

void cabin_init(int start);
int cabin_current_floor();
int cabin_move(timer_ms& start, int to_floor, unsigned long duration);
void cabin_stop();
void cabin_door(int dir);

/////////////////////////////////////////////////////////
// floor.h
/////////////////////////////////////////////////////////

#define FLOOR_FEEDBACK_PERIOD	1000

#define I2C_LCD         0x20
#define I2C_7SEG_BASE	0x70

#define PIN_LEDS      15
#define PIN_CALLBTNS  A0
#define PIN_KEYMAP_ROWS { 13, 12, 11, 10 }
#define PIN_KEYMAP_COLS {  9,  8,  7,  6 }

#define KEYMAP "123A" "456B" "789C" "*0#D"

#define MAX_LEDS         14
#define MAX_LCD_COLS     16
#define MAX_FLOOR_TITLE  MAX_LCD_COLS+1

struct floor_info {
  char title[MAX_FLOOR_TITLE]; 
  char key; 
  int displayVal; 
  bool def;   
  int ledUp, ledDown; 
  int callUp, callDown; 
  int pressed; 
  Adafruit_7segment upper7seg;
};

int  floor_init(floor_info floors[], size_t floor_num);
void floor_readbtns();
void floor_feedback(int current, const char *status = nullptr);
int  floor_requested(int from);

/////////////////////////////////////////////////////////
// program.cpp
/////////////////////////////////////////////////////////

#define TIME_OPENED 3000
#define TIME_DOORS  850
#define TIME_FLOOR_SHORT 2000
#define TIME_FLOOR_LONG  3000

#define FLOOR_NUM 3

floor_info building[FLOOR_NUM] = {
  //                               --led--  --btn--
  // title              key disp  def    up down  up  down pressed
  { "Parking         " , '*', -1, false ,  9,  0,  84,  -1,   0 },
  { "RDC             " , '0',  0, true  , 11, 10, 101,  93,   0 },
  { "1er etage       " , '1',  1, false , 13, 12, 118, 110,   0 }
};

enum states {
  STATE_OPENED,
  STATE_MOVING,
  STATE_OPENING,
  STATE_CLOSING
};

states state = STATE_OPENED;
timer_ms timer;
int target = -1;

unsigned long movetime();

void setup() {
  Serial.begin(9600);
  cabin_init(
    floor_init(building, FLOOR_NUM)
  );
}

void loop() {
  const char* status = nullptr;

  floor_readbtns();
  switch(state) {
    case STATE_OPENED:
      target = floor_requested(cabin_current_floor());
      status = "(waiting...)    ";
      if(target >= 0 && millis() - timer >= TIME_OPENED) {
        timer_reset(timer);
        cabin_door(CABIN_DOOR_CLOSE);
        state = STATE_CLOSING;
      }
      break;
    case STATE_CLOSING:
      cabin_door(CABIN_DOOR_CLOSE);
      status = "(closing doors) ";
      if(timer_elapsed(timer, TIME_DOORS)) {
        cabin_door(CABIN_DOOR_STOP);
        state = STATE_MOVING;
      }
      break;
    case STATE_MOVING: 
      status = "(moving)        ";
      if(cabin_move(timer, target, movetime()) == target) {
        cabin_stop();
        state = STATE_OPENING;
      }
      break;
    case STATE_OPENING:
      status = "(opening doors) ";
      cabin_door(CABIN_DOOR_OPEN);
      if(timer_elapsed(timer, TIME_DOORS)) {
        cabin_door(CABIN_DOOR_STOP);
        state = STATE_OPENED;  
      }
      break;
  }
  floor_feedback(cabin_current_floor(), status);
}

unsigned long movetime() {
  auto cur = cabin_current_floor();
  auto odd = (cur % 2) != 0;

  if(target < cur && odd || target > cur && !odd) {
    return TIME_FLOOR_SHORT;
  }
  else {
    return TIME_FLOOR_LONG;
  }
}


/////////////////////////////////////////////////////////
// timer.cpp
/////////////////////////////////////////////////////////

void timer_reset(timer_ms& timer) { 
  timer = millis();
}

bool timer_elapsed(timer_ms& timer, unsigned long limit) {
  if(millis()-timer < limit) {
    return false;
  }
  timer_reset(timer);
  return true;
}

/////////////////////////////////////////////////////////
// cabin.cpp
/////////////////////////////////////////////////////////

int _current_floor;

void motor(int pinA, int pinB, int dir); 
int  motor_dir(int to_floor);

void cabin_init(int start) {
  pinMode(PIN_MOTOR_CABIN_A, OUTPUT);
  pinMode(PIN_MOTOR_CABIN_B, OUTPUT);
  pinMode(PIN_MOTOR_DOORS_A, OUTPUT);
  pinMode(PIN_MOTOR_DOORS_B, OUTPUT);
  _current_floor = start;
}

int cabin_current_floor() {
  return _current_floor;
}

void cabin_stop() {
  motor(PIN_MOTOR_CABIN_A, PIN_MOTOR_CABIN_B, 0);
}

void cabin_door(int dir) {
  motor(PIN_MOTOR_DOORS_A, PIN_MOTOR_DOORS_B, dir);
}

int cabin_move(timer_ms& start, int to_floor, unsigned long duration) {
  if(to_floor != _current_floor) {
    auto dir = motor_dir(to_floor);

    if(timer_elapsed(start, duration)) {
      _current_floor += dir;
    }
  }
  motor(PIN_MOTOR_CABIN_A, PIN_MOTOR_CABIN_B, motor_dir(to_floor));
  return _current_floor;
}

int motor_dir(int to_floor) {
    if(_current_floor < to_floor) {
    return 1;
  }
  if(_current_floor > to_floor) {
    return -1;
  }
  return 0;
}

void motor(int pinA, int pinB, int dir) {
  int a = LOW, b = LOW;
  
  if(dir < 0) {
    b = HIGH;
  }
  else if(dir > 0) {
    a = HIGH;
  }
  digitalWrite(pinA, a);
  digitalWrite(pinB, b);
} 

/////////////////////////////////////////////////////////
// floor.cpp
/////////////////////////////////////////////////////////

#define BTN_UP    0b0001 
#define BTN_DOWN  0b0010 
#define BTN_PANEL 0b0100 

#define IS_PRESSED(val, flag) (((val)&(flag))==(flag))

static byte rows[] = PIN_KEYMAP_ROWS;
static byte cols[] = PIN_KEYMAP_COLS;

static Keypad _keys(const_cast<char*>(KEYMAP), rows, cols, sizeof(rows)/sizeof(byte), sizeof(cols)/sizeof(byte));
static Adafruit_NeoPixel _leds(MAX_LEDS, PIN_LEDS, NEO_GRB + NEO_KHZ800);
static LiquidCrystal_I2C _lcd(I2C_LCD , MAX_LCD_COLS, 2);

static floor_info *_floors;
static size_t _floors_size;

bool ladder_pressed(int expected, int actual);
void neopixel_switch(int led, bool on);

int floor_init(struct floor_info floors[], size_t floor_num) {
  int start = 0;

  _floors = (floor_info*)floors;
  _floors_size = floor_num;
  for(int i=0; i<_floors_size; i++) {
    _floors[i].upper7seg.begin(I2C_7SEG_BASE + i);
    if(_floors[i].def) {
      start = i;
    }
  }
  _lcd.init();
  _lcd.backlight();
  _leds.begin();
  floor_feedback(start);
  return start;
}

void floor_feedback(int current, const char* status) {
  static timer_ms refresh = 0;
  
  if(!timer_elapsed(refresh, FLOOR_FEEDBACK_PERIOD)) {
  	return;
  }
  _lcd.setCursor(0, 0);
  _lcd.print(_floors[current].title);
  if(status) {
    _lcd.setCursor(0, 1);
    _lcd.print(status);
  }
  for(int i=0; i<_floors_size; i++) {
    floor_info& info = _floors[i];
    auto btns = info.pressed;

    neopixel_switch(info.ledUp  , IS_PRESSED(btns, BTN_UP));
    neopixel_switch(info.ledDown, IS_PRESSED(btns, BTN_DOWN));
    neopixel_switch(i           , IS_PRESSED(btns, BTN_PANEL));
    info.upper7seg.printNumber(_floors[current].displayVal, 10);
    info.upper7seg.writeDisplay();
  }
  _leds.show();
}

void floor_readbtns() {
  int btn = analogRead(PIN_CALLBTNS);
  char key = _keys.getKey();
  
  for(int i=0; i < _floors_size; i++) {
    if(_floors[i].key == key) {
      _floors[i].pressed |= BTN_PANEL;
    }
    if(ladder_pressed(_floors[i].callDown, btn)) {
      _floors[i].pressed |= BTN_DOWN;
    }
    else if(ladder_pressed(_floors[i].callUp, btn)) {
      _floors[i].pressed |= BTN_UP;
    }
  }
}

int floor_requested(int from) {
  _floors[from].pressed = 0;
  for(int i = 0; i < _floors_size; i++) {
    int index = (from + i)%_floors_size;
    
    if(_floors[index].pressed) {
      return index;
    }
  }
  return -1;
}

bool ladder_pressed(int expected, int actual) {
  return expected !=-1 
      && expected-5 < actual && actual < expected+5;
}

void neopixel_switch(int led, bool on) {
  uint32_t color;

  if(on) {
    color = Adafruit_NeoPixel::Color(255, 64, 0);
  }
  else {
    color = Adafruit_NeoPixel::Color(0, 0, 0);
  }
  _leds.setPixelColor(led, color);
}
