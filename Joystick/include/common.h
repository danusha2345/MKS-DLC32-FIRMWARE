#pragma once


#define LED_PIN 2


#define EVERY_N_TIME(loop_name, time) \ 
  static uint64_t ___##loop_name = 0; \
  for(;___##loop_name < cur_time;___##loop_name = cur_time + time)