#include <Arduino.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal.h>

// Define pins
#define FIVE_VOLT_OUTPUT_PIN 1
#define LRD_INPUT_PIN A0
#define POT_INPUT_PIN A1
#define BTN_INPUT_PIN A2
#define LED1_OUTPUT_PIN 9
#define LED2_OUTPUT_PIN 10
#define LCD_RS_PIN 12
#define LCD_E_PIN 13
#define LCD_D4_PIN 5
#define LCD_D5_PIN 4
#define LCD_D6_PIN 3
#define LCD_D7_PIN 2
#define LCD_CONTRAST_PIN 6
#define LCD_BACKLIGHT_PIN 11

// Behaviour defs
#define POT_THRESHOLD 5 // %
#define POT_DELAY 100 // ms
#define BUTTON_SHORT_DELAY 50 // ms
#define BUTTON_LONG_DELAY 500 // ms
#define LCD_SLEEP_DELAY 30 // seconds
#define LCD_UPDATE_DELAY 750 // ms

// Config enum definitions
enum LEDMode {
  LEDInit,
  LEDOff,
  LEDOn,
  LEDOnFade,
  LEDOnStrobe,
  LEDLightLevel,
  LEDLightLevelFade,
  LEDLightLevelFadeInOut
};
char* LEDModeNames[] = { "Init", "Off", "On", "On Fade", "On Strobe", "Auto", "Auto Fade" "Auto Fade*" };

// Config definition
#define CONFIG_VERSION "V5"
typedef struct
{
  char version[5];
  unsigned int lcd_contrast;
  unsigned int lcd_brightness;
  LEDMode operationMode;
  unsigned int led_brightness;
  unsigned int led_min_brightness;
  double light_level;
  unsigned int fade_step;
} configuration_type;

// Global configuration instance
configuration_type CONFIGURATION = {
  CONFIG_VERSION,
  125,
  255,
  LEDLightLevelFadeInOut,
  255,
  15,
  25.0,
  25
};

// Define lcd
enum MenuScreens {
  InitScreen,
  StartScreen,
  LCDContrastScreen,
  LCDBrightnessScreen,
  BrightnessScreen,
  MinBrightnessScreen,
  LightLevelScreen,
  FadeStepScreen
};
LiquidCrystal lcd(LCD_RS_PIN, LCD_E_PIN, LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);

// Global variables
bool lcdOn = false;
bool editingSettings = false;
unsigned long lastUserAction = millis();
LEDMode operationMode;
MenuScreens lcdScreen = StartScreen;

char* getOperationModeName() {
  if(operationMode == LEDOff) {
    return "Off";
  } else if(operationMode == LEDOn) {
    return "On";
  } else if(operationMode == LEDOnFade) {
    return "On(Fade)";
  } else if(operationMode == LEDOnStrobe) {
    return "On(Strobe)";
  } else if(operationMode == LEDLightLevel) {
    return "Auto(LL)";
  } else if(operationMode == LEDLightLevelFade) {
    return "Auto(LLF)";
  } else if(operationMode == LEDLightLevelFadeInOut) {
    return "Auto(LLFIO)";
  }
  return "Init";
}

double getLightLevelPercentage() {
  const int numVals = 10;
  static int values[numVals];
  static int lastValueIndex = 0;
  static int actualValueCount = 0;

  // Read the raw value
  values[lastValueIndex] = analogRead(LRD_INPUT_PIN);
  lastValueIndex++;
  if(lastValueIndex >= numVals) {
    lastValueIndex = 0;
  }
  if(actualValueCount < numVals) {
    actualValueCount++;
  }

  // Average the raw value
  double rawValue = 0;
  for(int i = 0; i < actualValueCount; i++) {
    rawValue += values[i];
  }
  rawValue = rawValue / actualValueCount;

  // Return percentage
  return (rawValue / 1023) * 100.0;
}

double getPotPercentage() {
  const int numVals = 10;
  static int values[numVals];
  static int lastValueIndex = 0;
  static int actualValueCount = 0;

  // Read the raw value
  values[lastValueIndex] = analogRead(POT_INPUT_PIN);
  lastValueIndex++;
  if(lastValueIndex >= numVals) {
    lastValueIndex = 0;
  }
  if(actualValueCount < numVals) {
    actualValueCount++;
  }

  // Average the raw value
  double rawValue = 0;
  for(int i = 0; i < actualValueCount; i++) {
    rawValue += values[i];
  }
  rawValue = rawValue / actualValueCount;

  // Return percentage
  return (rawValue / 1023) * 100.0;
}

LEDMode getNextMode() {
  if(operationMode == LEDInit || operationMode == LEDOff) {
    return LEDOn;
  } else if(operationMode == LEDOn) {
    return LEDOnFade;
  } else if(operationMode == LEDOnFade) {
    return LEDOnStrobe;
  } else if(operationMode == LEDOnStrobe) {
    return LEDLightLevel;
  } else if(operationMode == LEDLightLevel) {
    return LEDLightLevelFade;
  } else if(operationMode == LEDLightLevelFade) {
    return LEDLightLevelFadeInOut;
  }
  return LEDOff;
}

MenuScreens getPrevScreen() {
  if(lcdScreen == InitScreen || lcdScreen == StartScreen) {
    return LightLevelScreen;
  } else if(lcdScreen == LCDContrastScreen) {
    return StartScreen;
  } else if(lcdScreen == LCDBrightnessScreen) {
    return LCDContrastScreen;
  } else if(lcdScreen == BrightnessScreen) {
    return LCDBrightnessScreen;
  } else if(lcdScreen == MinBrightnessScreen) {
    return BrightnessScreen;
  } else if(lcdScreen == LightLevelScreen) {
    return MinBrightnessScreen;
  } else if(lcdScreen == FadeStepScreen) {
    return LightLevelScreen;
  } 
  return InitScreen;
}

MenuScreens getNextScreen() {
  if(lcdScreen == InitScreen || lcdScreen == StartScreen) {
    return LCDContrastScreen;
  } else if(lcdScreen == LCDContrastScreen) {
    return LCDBrightnessScreen;
  } else if(lcdScreen == LCDBrightnessScreen) {
    return BrightnessScreen;
  } else if(lcdScreen == BrightnessScreen) {
    return MinBrightnessScreen;
  } else if(lcdScreen == MinBrightnessScreen) {
    return LightLevelScreen;
  } else if(lcdScreen == LightLevelScreen) {
    return FadeStepScreen;
  } else if(lcdScreen == FadeStepScreen) {
    return StartScreen;
  } 
  return InitScreen;
}

int loadConfig() 
{
  // Check the version, load (overwrite) the local configuration struct
  if (EEPROM.read(0) == CONFIG_VERSION[0] &&
      EEPROM.read(1) == CONFIG_VERSION[1] &&
      EEPROM.read(2) == CONFIG_VERSION[2]) {
    for (unsigned int i = 0; i < sizeof(CONFIGURATION); i++) {
      *((char*)&CONFIGURATION + i) = EEPROM.read(i);
    }
    return 1;
  }
  return 0;
}

void saveConfig() 
{
  // save the CONFIGURATION in to EEPROM
  for (unsigned int i = 0; i < sizeof(CONFIGURATION); i++) {
    EEPROM.write(i, *((char*)&CONFIGURATION + i));
  }
}

void onButtonShortPress() {
  if(lcdScreen == StartScreen && !editingSettings) {
    // Update the config
    CONFIGURATION.operationMode = getNextMode();
    saveConfig();

    // Update the mode
    operationMode = CONFIGURATION.operationMode;
  }
  lastUserAction = millis();
}

void onButtonLongPress() {
  if(editingSettings) {
    saveConfig();
    editingSettings = false;
  } else {
    editingSettings = true;
  }
  lastUserAction = millis();
}

void processButton() {
  static int buttonValue, buttonLastValue;
  static bool buttonShortPress, buttonLongPress;
  static unsigned long lastButtonPress;

  // Read input button state
  buttonLastValue = buttonValue;
  buttonValue = digitalRead(BTN_INPUT_PIN);
  if(buttonValue != buttonLastValue && buttonValue == LOW) {
    lastButtonPress = millis();
  }

  // Determine/perform button action
  if(buttonValue == LOW) {
    if((lastButtonPress + BUTTON_LONG_DELAY) <= millis()) {
      buttonLongPress = true;
      buttonShortPress = false;
    } else if((lastButtonPress + BUTTON_SHORT_DELAY) <= millis()) {
      buttonLongPress = false;
      buttonShortPress = true;
    }
  }
  else
  {
    // Check press flags and perform actions
    if(buttonLongPress) {
      onButtonLongPress();
    }
    else if(buttonShortPress) {
      onButtonShortPress();
    }
    
    // Reset press flag state
    buttonLongPress = false;
    buttonShortPress = false;
  }
}

void onPotUp() {
  lcdScreen = getNextScreen();
}

void onPotDown() {
  lcdScreen = getPrevScreen();
}

void processPotentiometer() {
  static int lastPotValue = getPotPercentage();
  static unsigned long lastPotAction = millis();

  if(editingSettings) {
    if(lcdScreen == LCDContrastScreen) {
      CONFIGURATION.lcd_contrast = ((getPotPercentage() / 100.0) * 255);
    } else if(lcdScreen == LCDBrightnessScreen) {
      CONFIGURATION.lcd_brightness = ((getPotPercentage() / 100.0) * 255);
    } else if(lcdScreen == BrightnessScreen) {
      CONFIGURATION.led_brightness = ((getPotPercentage() / 100.0) * 255);
    } else if(lcdScreen == MinBrightnessScreen) {
      CONFIGURATION.led_min_brightness = ((getPotPercentage() / 100.0) * 255);
    } else if(lcdScreen == LightLevelScreen) {
      CONFIGURATION.light_level = getPotPercentage();
    } else if(lcdScreen == FadeStepScreen) {
      CONFIGURATION.fade_step = ((getPotPercentage() / 100.0) * 1000);
    }
    lastPotValue = getPotPercentage();
    lastPotAction = millis();
  } else {
    if(millis() > (lastPotAction + POT_DELAY)) {
      if(getPotPercentage() >= (lastPotValue + POT_THRESHOLD)) {
        onPotUp();
        lastPotValue = getPotPercentage();
        lastPotAction = millis();
        lastUserAction = millis();
      } else if(getPotPercentage() <= (lastPotValue - POT_THRESHOLD)) {
        onPotDown();
        lastPotValue = getPotPercentage();
        lastPotAction = millis();
        lastUserAction = millis();
      }
    }
  }
}

void processLEDs() {
  static int step = 1;
  static LEDMode lastMode = LEDInit;
  static unsigned long lastStep = millis();
  static unsigned int lastBrightness = CONFIGURATION.led_brightness, currentBrightness = CONFIGURATION.led_min_brightness;

  // Update the operation mode
  if(lastMode != operationMode) {
    analogWrite(LED1_OUTPUT_PIN, 0);
    analogWrite(LED2_OUTPUT_PIN, 0);
      
    if(operationMode == LEDOn) {
      analogWrite(LED1_OUTPUT_PIN, CONFIGURATION.led_brightness);
      analogWrite(LED2_OUTPUT_PIN, CONFIGURATION.led_brightness);
      lastBrightness = CONFIGURATION.led_brightness;
    }
    lastMode = operationMode;
  }

  // Process the operation mode
  if(operationMode == LEDOn && lastBrightness != CONFIGURATION.led_brightness) {
    analogWrite(LED1_OUTPUT_PIN, CONFIGURATION.led_brightness);
    analogWrite(LED2_OUTPUT_PIN, CONFIGURATION.led_brightness);
    lastBrightness = CONFIGURATION.led_brightness;
  } else if(operationMode == LEDLightLevelFade) {
    if(getLightLevelPercentage() <= CONFIGURATION.light_level) {
      if(currentBrightness != lastBrightness) {
        analogWrite(LED1_OUTPUT_PIN, currentBrightness);
        analogWrite(LED2_OUTPUT_PIN, currentBrightness);
        lastBrightness = currentBrightness;
      }
    } else {
      analogWrite(LED1_OUTPUT_PIN, 0);
      analogWrite(LED2_OUTPUT_PIN, 0);
      lastBrightness = 0;
    }

    // Step the LED brightness
    if(millis() > (lastStep + CONFIGURATION.fade_step)) {
      currentBrightness += step;
      if(currentBrightness >= CONFIGURATION.led_brightness || currentBrightness <= CONFIGURATION.led_min_brightness) {
        step *= -1;
      }
      lastStep = millis();
    }
  }
}

void updateLCD() {
  static MenuScreens lastScreen = InitScreen;
  static unsigned long lastUpdate = millis();
  static unsigned int lastContrast = -1, currentContrast = CONFIGURATION.lcd_contrast, lastBrightness = -1, currentBrightness = CONFIGURATION.lcd_brightness;

  // Update the lcd screen
  if(lastScreen != lcdScreen) {
    lcd.clear(); // Clear the lcd

    // Draw the specified screen
    if(lcdScreen == StartScreen) {
      // Print operation mode
      lcd.setCursor(0, 0);
      lcd.print("Mode: ");
      lcd.print(getOperationModeName());

      // Print light level percentage
      lcd.setCursor(0, 1);
      lcd.print("Light: ");
      lcd.print(getLightLevelPercentage());
      lcd.print("%");
    } else if(lcdScreen == LCDContrastScreen) {
      lcd.setCursor(0, 0);
      lcd.print("LCD Contrast");
      lcd.setCursor(0, 1);
      lcd.print(currentContrast);
    } else if(lcdScreen == LCDBrightnessScreen) {
      lcd.setCursor(0, 0);
      lcd.print("LCD Brightness");
      lcd.setCursor(0, 1);
      lcd.print(currentBrightness);
    } else if(lcdScreen == BrightnessScreen) {
      lcd.setCursor(0, 0);
      lcd.print("LED Max");
      lcd.setCursor(0, 1);
      lcd.print(CONFIGURATION.led_brightness);
    } else if(lcdScreen == MinBrightnessScreen) {
      lcd.setCursor(0, 0);
      lcd.print("LED Min");
      lcd.setCursor(0, 1);
      lcd.print(CONFIGURATION.led_min_brightness);
    } else if(lcdScreen == LightLevelScreen) {
      lcd.setCursor(0, 0);
      lcd.print("Light Level");
      lcd.setCursor(0, 1);
      lcd.print(CONFIGURATION.light_level);
      lcd.print("(");
      lcd.print(getLightLevelPercentage());
      lcd.print("%");
      lcd.print(")");
    } else if(lcdScreen == FadeStepScreen) {
      lcd.setCursor(0, 0);
      lcd.print("Step (ms)");
      lcd.setCursor(0, 1);
      lcd.print(CONFIGURATION.fade_step);
    }

    // Update control variables
    lastScreen = lcdScreen;
    lastUpdate = millis();
  }

  // Update the current lcd screen information
  if(millis() > (lastUpdate + LCD_UPDATE_DELAY)) {
    if(lcdScreen == StartScreen) {
      // Update operation mode
      for(int i = 5; i < 16; i++) {
        lcd.setCursor(i, 0);
        lcd.write(254);
      }
      lcd.setCursor(6, 0);
      lcd.print(getOperationModeName());

      // Update light level percentage
      for(int i = 6; i < 16; i++) {
        lcd.setCursor(i, 1);
        lcd.write(254);
      }
      lcd.setCursor(7, 1);
      lcd.print(getLightLevelPercentage());
      lcd.print("%");
    } else if(lcdScreen == LCDContrastScreen) {
      lcd.setCursor(0, 1);
      for(int i = 0; i < 16; i++) {
        lcd.setCursor(i, 1);
        lcd.write(254);
      }
      lcd.setCursor(0, 1);
      lcd.print(currentContrast);
    } else if(lcdScreen == LCDBrightnessScreen) {
      lcd.setCursor(0, 1);
      for(int i = 0; i < 16; i++) {
        lcd.setCursor(i, 1);
        lcd.write(254);
      }
      lcd.setCursor(0, 1);
      lcd.print(currentBrightness);
    } else if(lcdScreen == BrightnessScreen) {
      lcd.setCursor(0, 1);
      for(int i = 0; i < 16; i++) {
        lcd.setCursor(i, 1);
        lcd.write(254);
      }
      lcd.setCursor(0, 1);
      lcd.print(CONFIGURATION.led_brightness);
    } else if(lcdScreen == MinBrightnessScreen) {
      lcd.setCursor(0, 1);
      for(int i = 0; i < 16; i++) {
        lcd.setCursor(i, 1);
        lcd.write(254);
      }
      lcd.setCursor(0, 1);
      lcd.print(CONFIGURATION.led_min_brightness);
    } else if(lcdScreen == LightLevelScreen) {
      lcd.setCursor(0, 1);
      for(int i = 0; i < 16; i++) {
        lcd.setCursor(i, 1);
        lcd.write(254);
      }
      lcd.setCursor(0, 1);
      lcd.print(CONFIGURATION.light_level);
      lcd.print("(");
      lcd.print(getLightLevelPercentage());
      lcd.print("%");
      lcd.print(")");
    } else if(lcdScreen == FadeStepScreen) {
      lcd.setCursor(0, 1);
      for(int i = 0; i < 16; i++) {
        lcd.setCursor(i, 1);
        lcd.write(254);
      }
      lcd.setCursor(0, 1);
      lcd.print(CONFIGURATION.fade_step);
    }
    lastUpdate = millis();
  }

  // Update the contrast
  if(currentContrast != CONFIGURATION.lcd_contrast) {
    currentContrast = CONFIGURATION.lcd_contrast;
  }

  // Output updated contrast
  if(lastContrast != currentContrast) {
    lastContrast = currentContrast;
    analogWrite(LCD_CONTRAST_PIN, lastContrast);
  }

  // Update the brightness
  if(currentBrightness != CONFIGURATION.lcd_brightness) {
    currentBrightness = CONFIGURATION.lcd_brightness;
  }

  // Output updated brightness
  if(lastBrightness != currentBrightness) {
    lastBrightness = currentBrightness;
    analogWrite(LCD_BACKLIGHT_PIN, lastBrightness);
  }

  // Turn the LCD off/on
  unsigned long lcdSleep = lastUserAction + ((long)LCD_SLEEP_DELAY * 1000);
  if(millis() >= lcdSleep && lcdOn) {
    digitalWrite(LCD_BACKLIGHT_PIN, LOW);
    lcdOn = false;
  } else if(millis() < lcdSleep && !lcdOn) {
    analogWrite(LCD_BACKLIGHT_PIN, currentBrightness);
    lcdOn = true;
  }
}

void loop() {
  // Process inputs
  processButton();
  processPotentiometer();

  // Process LEDs
  processLEDs();

  // Update the LCD
  updateLCD();
}

void setup() {
  // Initialize serial 
  Serial.begin(9600);

  // Load config
  EEPROM.begin();
  if(!loadConfig()) {
    saveConfig();
  }

  //Initialize global variables from loaded config
  operationMode = CONFIGURATION.operationMode;

  // Initialize lcd
  lcd.begin(16,2);

  // Initialize pins
  pinMode(FIVE_VOLT_OUTPUT_PIN, OUTPUT);
  digitalWrite(FIVE_VOLT_OUTPUT_PIN, HIGH); // Output 5v
  pinMode(LRD_INPUT_PIN, INPUT);
  pinMode(POT_INPUT_PIN, INPUT);
  pinMode(BTN_INPUT_PIN, INPUT_PULLUP);
  pinMode(LED1_OUTPUT_PIN, OUTPUT);
  pinMode(LED2_OUTPUT_PIN, OUTPUT);
  pinMode(LCD_CONTRAST_PIN, OUTPUT);
  pinMode(LCD_BACKLIGHT_PIN, OUTPUT);
}
