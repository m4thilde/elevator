#include <wino.h>

#include "cabin.h"

static int _current_floor;

static void motor(int pinA, int pinB, int dir); 
static int  motor_dir(int to_floor);

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
