// Buttons includes
#include <Button.h>
#include <ButtonEventCallback.h>
#include <PushButton.h>
#include <Bounce2.h>

// Compass includes
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>

const int pin_sensor = 2;
const int pin_display_a_cs = 3;
const int pin_display_a_wr = 4;
const int pin_display_a_data = 5;
const int pin_display_b_cs = 6;
const int pin_display_b_wr = 7;
const int pin_display_b_data = 8;
const int pin_joystick_up = 9;
const int pin_joystick_down = 10;
const int pin_motor_output_a = 11;
const int pin_motor_output_b = 12;
const int pin_button_up = 15;
const int pin_button_center = 14;
const int pin_button_down = 16;
const int pin_light = 17;

// IMPORTANT: these are the indexes for menu[] array.
// If new options are added, update these values.
int maxMenuIndex = 6;
int minSubmenuIndex = 7; // Partial
int maxSubmenuIndex = 10; // Speed
int selectedMenuOption = 0;
int selectedSubMenuOption = minSubmenuIndex;
int btnHoldDelay = 1000;
int btnHoldRepeat = 30;

float previousTripTotal = 0;

unsigned long previousMillis = 0;
unsigned long previousMillisSpeed = 0;
unsigned long previousMillisDistance = 0;
const long displayRefreshInterval = 500;
const long autoSaveAfter = 3000;

bool inMenu = false;
bool inSubMenu = false;
bool lightsOn = false;
bool autoCalibrate = false;
bool distanceAlreadySaved = true;

// Interruption variables for wheel sensor
const byte interruptPin = pin_sensor;
volatile byte revs = 0;

// This is to convert float values to int and display them
long int displayValues[] = {
    0, // Partial
    0, // Total
    0, // Heading
    0  // Speed
};

struct Configuration {
  int version;
  int showInDisplay1;
  int showInDisplay2;
  int lightsOn;
  int autoCalibrate;
  int circumference;
  float declinationAngle;
  float trip_partial = 0;
  float trip_total = 0;
} config;

PushButton button_up = PushButton(pin_button_up);
PushButton button_center = PushButton(pin_button_center);
PushButton button_down = PushButton(pin_button_down);

// Assign a unique ID to this sensor at the same time
Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);

void setup() {
  Serial.begin(9600);
  
  loadConfig();

  pinMode(pin_display_a_cs, OUTPUT);
  pinMode(pin_display_a_wr, OUTPUT);
  pinMode(pin_display_a_data, OUTPUT);
  pinMode(pin_display_b_cs, OUTPUT);
  pinMode(pin_display_b_wr, OUTPUT);
  pinMode(pin_display_b_data, OUTPUT);

  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), revsCount, FALLING);

  // Init displays
  Init_1621(0);
  Init_1621(1);

  button_up.onPress(onButtonUpPressed);
  button_up.onHoldRepeat(btnHoldDelay, btnHoldRepeat, onButtonUpPressed);

  button_center.onRelease(onButtonCenterReleased);
  button_center.onHold(btnHoldDelay, onButtonCenterHold);

  button_down.onPress(onButtonDownPressed);
  button_down.onHoldRepeat(btnHoldDelay, btnHoldRepeat, onButtonDownPressed);

  // Initialise the compass
  if(!mag.begin()) {
    Serial.println(F("Ooops, no HMC5883 detected... Check your wiring!"));
  }

  revs = 0;

  updateScreens();
}

void loop() {
  button_up.update();
  button_center.update();
  button_down.update();
  sensorUpdate();
  calculateHeading();
  calculateSpeed();
  autoSave();
}

// Interrupt Service Routine (ISR)
void revsCount() {
  revs++;
}

void sensorUpdate() {

  byte revsTmp = revs;

  if(revsTmp > 0)
  {
    // This will count as many revolutions as added by 
    // the interruptions and reset them for the next loop
    config.trip_partial += (config.circumference / 100000.0) * revsTmp; // Count 1 every 100 meters
    config.trip_total += (config.circumference / 100000.0) * revsTmp; // Count 1 every 100 meters

      
    revs -= revsTmp;  

    distanceAlreadySaved = false;
    previousMillisDistance = millis(); // Renew time for auto-save
    
    Serial.println(F("SENSOR detected"));
    displayValues[0] = config.trip_partial;
    displayValues[1] = config.trip_total;
    Serial.print("trip_partial: ");
    Serial.println(config.trip_partial, 6);
    
    updateScreens();
  }

}

void loadConfig() {
  Serial.println(F("---> Loading saved data..."));

  EEPROM_read(0, config);

  Serial.println(config.version);
  Serial.println(config.showInDisplay1);
  Serial.println(config.showInDisplay2);
  Serial.println(config.lightsOn);
  Serial.println(config.autoCalibrate);
  Serial.println(config.circumference);
  Serial.println(config.declinationAngle, 6);
  Serial.println(config.trip_partial);
  Serial.println(config.trip_total);

  if(config.version != 123){
    Serial.println(F("---> No configuration data found. Setting up default values..."));
    
    config.showInDisplay1 = 0; // Partial distance
    config.showInDisplay2 = 3; // Speed
    config.lightsOn = 0;
    config.autoCalibrate = 0;
    config.circumference = 2040; // KTM 1190 Adventure with Conti Trail Attack
    config.declinationAngle = 0.01425352f; // Madrid
    config.version = 123;
    config.trip_partial = 0;
    config.trip_total = 0;

    saveConfig();
  } else {
    displayValues[0] = config.trip_partial;
    displayValues[1] = config.trip_total;
  }

  Serial.println(F("---> DONE"));
}

void saveConfig(){
  EEPROM_write(0, config);
  Serial.println(F("---> CONFIG SAVED!"));

  Serial.println(config.version);
  Serial.println(config.showInDisplay1);
  Serial.println(config.showInDisplay2);
  Serial.println(config.lightsOn);
  Serial.println(config.autoCalibrate);
  Serial.println(config.circumference);
  Serial.println(config.declinationAngle, 6);
  Serial.println(config.trip_partial);
  Serial.println(config.trip_total);
}

void onButtonUpPressed(Button& btn){
  Serial.println(F("button UP pressed"));

  if(inMenu){

    if(selectedMenuOption > 0){
      HT1621_all_off(16, 0);
      selectedMenuOption--;
    }

    switch(selectedMenuOption){
      case 0:
      case 1:
        minSubmenuIndex = 7;
        maxSubmenuIndex = 10;
        break;
      case 4:
      case 5:
        minSubmenuIndex = 11;
        maxSubmenuIndex = 12;
        break;
      case 6:
        minSubmenuIndex = 13;
        maxSubmenuIndex = 14;
        break;
    }

    selectedSubMenuOption = minSubmenuIndex;
  } else if(inSubMenu){
    if(selectedMenuOption == 2){
      config.circumference++;

      if(config.circumference > 9999){
        config.circumference = 9999;
      }
    } else {
      if(selectedSubMenuOption > minSubmenuIndex){ 
        HT1621_all_off(16, 1);
        selectedSubMenuOption--;
      }
    }
  } else {
    config.trip_partial += 1;
    config.trip_total += 1;
    displayValues[0] = config.trip_partial;
    displayValues[1] = config.trip_total;
  }

  updateScreens();
}

void onButtonCenterReleased(Button& btn, int duration){
  // This avoids calling this callback when releasing
  // the center button after a Hold event
  if(duration >= btnHoldDelay)
    return;

  Serial.println(F("button CENTER pressed"));
  if(inMenu){
    switch(selectedMenuOption){
      case 0:
        selectedSubMenuOption = maxMenuIndex + config.showInDisplay1 + 1;
        break;
      case 1:
        selectedSubMenuOption = maxMenuIndex + config.showInDisplay2 + 1;
        break;
      case 4:
        selectedSubMenuOption = 12 - config.lightsOn; // 12 is the number of the position of the ON / OFF in menu
        break;
      case 5:
        selectedSubMenuOption = 12 - config.autoCalibrate; // 12 is the number of the position of the ON / OFF in menu
        break;
      case 6:
        selectedSubMenuOption = 14;
        break;
    }

    inMenu = false;
    inSubMenu = true;
    Serial.println(F("State changed: inSubMenu"));
  } else if(inSubMenu){
    switch(selectedMenuOption){
      case 0:
        config.showInDisplay1 = selectedSubMenuOption - minSubmenuIndex;
        saveConfig();
        inSubMenu = false;
        break;
      case 1:
        config.showInDisplay2 = selectedSubMenuOption - minSubmenuIndex;
        saveConfig();
        inSubMenu = false;
        break;
      case 2:
        saveConfig();
        inSubMenu = false;
        break;
      case 6:
        if(selectedSubMenuOption == 13){
          resetConfig();
          inSubMenu = false;
        }
        break;
    }
  } else {
    config.trip_partial = 0;
    displayValues[0] = config.trip_partial;
  }
  updateScreens();
}

void onButtonCenterHold(Button& btn){
  Serial.println(F("button CENTER hold"));

  if(inMenu){
    inMenu = false;
    inSubMenu = false;
    Serial.println(F("State changed: none"));
  } else if(inSubMenu){
    inSubMenu = false;
    inMenu = true;
    Serial.println(F("State changed: inMenu"));
  } else {
    inMenu = true;
    Serial.println(F("State changed: inMenu"));
  }

  HT1621_all_off(6, 0);
  HT1621_all_off(6, 1);

  updateScreens();
}

void onButtonDownPressed(Button& btn){
  Serial.println(F("button DOWN pressed"));
  if(inMenu){
    if(selectedMenuOption < maxMenuIndex){
      HT1621_all_off(6, 0);
      selectedMenuOption++;
    }

    switch(selectedMenuOption){
      case 0:
      case 1:
        minSubmenuIndex = 7;
        maxSubmenuIndex = 10;
        break;
      case 4:
      case 5:
        minSubmenuIndex = 11;
        maxSubmenuIndex = 12;
        break;
      case 6:
        minSubmenuIndex = 13;
        maxSubmenuIndex = 14;
        break;
    }

    selectedSubMenuOption = minSubmenuIndex;
  } else if(inSubMenu){
    if(selectedMenuOption == 2){
      config.circumference--;

      if(config.circumference < 0){
        config.circumference = 0;
      }
    } else {
      if(selectedSubMenuOption < maxSubmenuIndex){
        HT1621_all_off(16, 1);
        selectedSubMenuOption++;
      }
    }
  } else {
    config.trip_partial -= 1;
    config.trip_total -= 1;

    if(config.trip_partial < 0){
      config.trip_partial = 0;
    }

    if(config.trip_total < 0){
      config.trip_total = 0;
    }

    displayValues[0] = config.trip_partial;
    displayValues[1] = config.trip_total;
  }
  updateScreens();
}

void calculateHeading() {
  if(config.showInDisplay1 != 2 && config.showInDisplay2 != 2)
    return;

  unsigned long currentMillis = millis();

  // Get a new compass event
  sensors_event_t event; 
  mag.getEvent(&event);

  // Hold the module so that Z is pointing 'up' and you can measure the heading with x&y
  // Calculate heading when the magnetometer is level, then correct for signs of axis.
  float heading = atan2(event.magnetic.y, event.magnetic.x);

  // Once you have your heading, you must then add your 'Declination Angle', which is the 'Error' of the magnetic field in your location.
  // Find yours here: http://www.magnetic-declination.com/
  // Mine is: -13* 2' W, which is ~13 Degrees, or (which we need) 0.22 radians
  // If you cannot find your Declination, comment out these two lines, your compass will be slightly off.
  //float declinationAngle = 0.22;
  heading += config.declinationAngle;
  
  // Correct for when signs are reversed.
  if(heading < 0)
    heading += 2 * PI;

  // Check for wrap due to addition of declination.
  if(heading > 2 * PI)
    heading -= 2 * PI;

  // Update data and display
  if (currentMillis - previousMillis >= displayRefreshInterval) {
    previousMillis = currentMillis;

    // Convert radians to degrees for readability.
    float headingDegrees = heading * 180 / M_PI;
    displayValues[2] = headingDegrees;
 
    updateScreens();
  }
}

void autoSave(){
  if(distanceAlreadySaved == true){
    return;
  }

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillisDistance >= autoSaveAfter) {
    Serial.println(F("Auto-saving distance..."));

    saveConfig();
    distanceAlreadySaved = true;
  }
}

void calculateSpeed(){
  unsigned long currentMillis = millis();

  // Update data and display
  if (currentMillis - previousMillisSpeed >= 1000) {
    previousMillisSpeed = currentMillis;

    float km = (config.trip_total - previousTripTotal) / 10;
    displayValues[3] = (km * 3600) + 0.5; // Adding 0.5 is the easiest way to round up (int to float trims)
    previousTripTotal = config.trip_total;
 
    updateScreens();
  }
}

void resetConfig(){
  config.version = 000;
  saveConfig();
  Serial.println(F("--> CONFIG CLEARED"));
  inMenu = false;
  inSubMenu = false;
}

void updateScreens() {
  if(inMenu){
    display_word(selectedMenuOption, 0);
    HT1621_all_off(6, 1);
  } else if(inSubMenu){
    display_word(selectedMenuOption, 0);

    switch(selectedMenuOption){
      case 0:
      case 1:
      case 4:
      case 5:
      case 6:
        display_word(selectedSubMenuOption, 1);
        break;
      case 2:
        display_number(config.circumference, 0, 0, 0, 1);
        break;
      case 3:
        display_number(config.declinationAngle, 0, 0, 0, 1);
        break;
    }
  } else {
    // Display 1
    switch(config.showInDisplay1){
      case 0:
        display_number(displayValues[0], 1, 1, 0, 0);
        break;
      case 1:
        display_number(displayValues[1], 1, 0, 1, 0);
        break;
      case 2:
        display_degrees(displayValues[2], 0, 0, 0, 0);
        break;
      case 3:
        display_number(displayValues[3], 0, 1, 1, 0);
        break;
    }

    // Display 2
    switch(config.showInDisplay2){
      case 0:
        display_number(displayValues[0], 1, 1, 0, 1);
        break;
      case 1:
        display_number(displayValues[1], 1, 0, 1, 1);
        break;
      case 2:
        display_degrees(displayValues[2], 0, 0, 0, 1);
        break;
      case 3:
        display_number(displayValues[3], 0, 1, 1, 1);
        break;
    }
  }
}

