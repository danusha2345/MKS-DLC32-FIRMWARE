# 1 "C:\\Users\\Sergey\\AppData\\Local\\Temp\\tmpwnhlntqv"
#include <Arduino.h>
# 1 "C:/Users/Sergey/Documents/GitHub/MKS-DLC32-FIRMWARE/Firmware/Grbl_Esp32/Grbl_Esp32.ino"
# 21 "C:/Users/Sergey/Documents/GitHub/MKS-DLC32-FIRMWARE/Firmware/Grbl_Esp32/Grbl_Esp32.ino"
#include "src/Grbl.h"
void setup();
void loop();
#line 23 "C:/Users/Sergey/Documents/GitHub/MKS-DLC32-FIRMWARE/Firmware/Grbl_Esp32/Grbl_Esp32.ino"
void setup() {
    grbl_init();
}

void loop() {
  _mc_task_init();

  while(1) {

    run_once();

  }
}