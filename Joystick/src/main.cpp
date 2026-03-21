#include <Arduino.h>
#include <WiFi.h>
#include <GyverJoy.h>
#include <Preferences.h>

#include <string>
#include <queue>

#define GRBL_JOG_DEBUG 0

#include "joystick.h"
#include "buttons.h"
#include "wifi_helper.h"
#include "grbl.h"
#include "jog.h"

Joystick joystick;
Buttons buttons;
WiFiHelper wifi_helper;
TelnetGrblParser grblParser;
Jog jog;

Preferences preferences;

void setup() 
{
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);

  jog.setup();
  buttons.setup();
  joystick.setup();
  WiFiHelper::setup();

  grblParser.onGCodeAboutToBeSent = [](std::string command)
  {
      if(command == "?") 
        return;

      Serial.printf("Sending command: %s\n", command.c_str());
  };

  grblParser.onResponseAboutToBeProcessed = [](std::string response)
  {
      Serial.printf("Got response: %s\n", response.c_str());
  };

  grblParser.onPositionUpdated = [](Grbl::MachineState machineState, Grbl::CoordinateMode coordinateMode, const Grbl::Coordinate &coordinate)
  {
      Serial.printf("Machine Coordinate: X: %.2f Y: %.2f Z: %.2f\n", coordinate[0], coordinate[1], coordinate[2]);
  };

  grblParser.setStatusReportInterval(1000);
}

void loop() 
{
  delay(1);

  if(WiFiHelper::telnet_client.connected())
  {
    //readLoop();

    grblParser.update();
  }

  jog.update();
}
