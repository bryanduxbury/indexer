// include the library code:
#include <LiquidCrystal.h>
#include "A4988.h"
#include "Encoder.h"
#include "Button.h"

#define DEGREE (char) 223

// LCD
#define RS 10
#define EN 11
#define DB4 A0
#define DB5 A1
#define DB6 A2
#define DB7 A3
LiquidCrystal lcd(RS, EN, DB4, DB5, DB6, DB7);

// stepper motor
#define STEPS_PER_REV 200l
#define MICROSTEPS 16l
#define WORM_RATIO 72l
#define STEPS_PER_FULL_REV (long) (WORM_RATIO * STEPS_PER_REV * MICROSTEPS)
#define DEG_PER_STEP (float) (360.0 / STEPS_PER_FULL_REV)

#define DIR_PIN 4
#define STEP_PIN 5

A4988 stepper(STEPS_PER_REV, DIR_PIN, STEP_PIN);

// rotary encoder
#define ENCODER_P1 3
#define ENCODER_P2 2
#define ENCODER_SW_PIN 9

#define R_BUTTON_PIN 6
#define L_BUTTON_PIN 7

Encoder encoder(ENCODER_P1, ENCODER_P2);
Button encoderButton(ENCODER_SW_PIN);
Button rButton(R_BUTTON_PIN);
Button lButton(L_BUTTON_PIN);

// system config
#define UNSELECTED_MODE -1
#define DIV_MODE 0
#define DIV_MENU_MODE 2
#define JOG_MODE 1

int currentMode = UNSELECTED_MODE;

void encoderInterrupt() {
  encoder.interruptRoutine();
}

void setup() {
  lcd.begin(20, 4);

  stepper.begin(240, MICROSTEPS);

  encoder.init();

  // call updateEncoder() when any high/low changed seen
  // on interrupt 0 (pin 2), or interrupt 1 (pin 3)
  attachInterrupt(0, encoderInterrupt, CHANGE);
  attachInterrupt(1, encoderInterrupt, CHANGE);

  encoderButton.init();
  rButton.init();
  lButton.init();
}

void loop() {
  switch (currentMode) {
    case UNSELECTED_MODE:
    menu();
    break;

    case DIV_MODE:
    divMode();
    break;

    case DIV_MENU_MODE:
    divMenu();
    break;

    case JOG_MODE:
    jogMode();
    break;
  }
}

void menu() {
  lcd.clear();
  lcd.setCursor(0, 3);
  lcd.print("SELECT|     |      ");

  encoder.configure(0, 1, 0, 1);

  while (!encoderButton.isPressedAndReleased()) {
    lcd.setCursor(0,0);
    if (encoder.value() == 0) {
      lcd.print("     [Div Mode]     ");
    } else {
      lcd.print("      Div Mode      ");
    }

    lcd.setCursor(0,1);
    if (encoder.value() == 1) {
      lcd.print("     [Jog Mode]     ");
    } else {
      lcd.print("      Jog Mode      ");
    }
    delay(100);
  }

  switch (encoder.value()) {
    case DIV_MODE:
    configureDivMode();
    break;

    case JOG_MODE:
    configureJogMode();
    break;
  }
}

// div mode state
long numDivs = -1;
long curDiv = -1;
long curStep = 0;

void configureDivMode() {
  lcd.clear();
  lcd.print("Number of divisions?");

  lcd.setCursor(0,3);
  lcd.print("SELECT|      |      ");

  encoder.configure(1, 720, 72, 1);

  while (!encoderButton.isPressedAndReleased()) {
    lcd.setCursor(0,1);
    lcd.print(encoder.value());
    lcd.print(" (");
    float degPerDiv = 360.0 / encoder.value();
    float stepsPerDiv = degPerDiv / DEG_PER_STEP;
    printDegrees(degPerDiv);
    lcd.print(")  ");

    lcd.setCursor(0,2);
    lcd.print("Err: +/-");
    printDegrees((stepsPerDiv - (long) stepsPerDiv) * DEG_PER_STEP);
    lcd.print("  ");

    delay(100);
  }

  numDivs = encoder.value();
  curDiv = 0;
  curStep = 0;
  setupDivMode();
}

void setupDivMode() {
  currentMode = DIV_MODE;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Divs:");
  lcd.setCursor(0, 1);
  lcd.print("Angle:");

  lcd.setCursor(0,3);
  lcd.print(" MENU | PREV | NEXT ");
}

long normalizeCurDiv() {
  if (curDiv < 0) {
    return numDivs + ((curDiv + 1) % numDivs);
  } else {
    return (curDiv % numDivs) + 1;
  }
}

void divMode() {
  lcd.setCursor(7, 0);
  lcd.print(normalizeCurDiv());
  lcd.print("/");
  lcd.print(numDivs);
  lcd.print("  ");

  lcd.setCursor(7, 1);
  printDegrees(360.0 / numDivs * (normalizeCurDiv() - 1));
  lcd.print("  ");

  long moveDir = 0;

  if (rButton.isPressed()) {
    // advance button pressed? step forward
    moveDir = 1;
  } else if (lButton.isPressed()) {
    // prev button pressed? step backwards
    moveDir = -1;
  }

  if (moveDir != 0)  {
    long desiredStep = round((float) STEPS_PER_FULL_REV / numDivs * (curDiv + moveDir));
    long requiredSteps = desiredStep - curStep;

    stepper.move(requiredSteps);
    curDiv += moveDir;
    curStep += requiredSteps;
  }

  if (moveDir != 0)
    delay(500);

  // rotary button pressed == div mode menu
  if (encoderButton.isPressedAndReleased()) {
    currentMode = DIV_MENU_MODE;
  }
}

void divMenu() {
  lcd.clear();

  encoder.configure(0, 2, 0, 1);

  while (!encoderButton.isPressedAndReleased()) {
    lcd.setCursor(0,0);
    if (encoder.value() == 0) {
      lcd.print("[Back]");
    } else {
      lcd.print(" Back ");
    }

    lcd.setCursor(0,1);
    if (encoder.value() == 1) {
      lcd.print("[Zero Divs]");
    } else {
      lcd.print(" Zero Divs ");
    }

    lcd.setCursor(0,2);
    if (encoder.value() == 2) {
      lcd.print("[Abort]");
    } else {
      lcd.print(" Abort ");
    }
    delay(100);
  }

  switch (encoder.value()) {
    case 0:
    setupDivMode();
    break;

    case 1:
    curDiv = 0;
    curStep = 0;
    setupDivMode();
    break;

    case 2:
    currentMode = UNSELECTED_MODE;
    break;
  }
}

void configureJogMode() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Current:");
  lcd.setCursor(0,3);
  lcd.print(" EXIT | ZERO | STEP ");

  currentMode = JOG_MODE;
}

float stepsToDegrees(long step) {
  return (float) step / STEPS_PER_FULL_REV * 360.0;
}

long stepSizes [13] = {1l, 2l, 3l, 4l,
                       5l, 6l, 7l, 8l,
                       16l,
                       STEPS_PER_REV * MICROSTEPS / 50l /* 0.1 deg */,
                       STEPS_PER_REV * MICROSTEPS / 10l /* 0.5 deg */,
                       STEPS_PER_REV * MICROSTEPS / 5l /* 1 deg */,
                       STEPS_PER_REV * MICROSTEPS /* 5 deg */};

#define JOGGING 0
#define STEP_ADJUST 1

void printDegrees(float degs) {
  lcd.print(degs, 5);
  lcd.print(DEGREE);
}

void jogMode() {
  long currentStepOfFullRev = 0;

  int stepSizeNum = 10;
  int rotaryMode = JOGGING;

  encoder.configure(-5l * STEPS_PER_FULL_REV, 5l*STEPS_PER_FULL_REV, 0, stepSizes[stepSizeNum]);

  lcd.setCursor(0,1);
  lcd.print("Step: ");
  printDegrees(stepsToDegrees(stepSizes[stepSizeNum]));
  lcd.print("  ");

  while (true) {
    lcd.setCursor(9,0);
    printDegrees(stepsToDegrees(currentStepOfFullRev));
    lcd.print("  ");

    long targetStepOfFullRev = encoder.value();
    // move if we're not at the target position
    if (currentStepOfFullRev != targetStepOfFullRev) {
      long stepDelta = currentStepOfFullRev - targetStepOfFullRev;

      stepper.move(-1l * stepDelta);
      currentStepOfFullRev = targetStepOfFullRev;
    }

    // zero current step
    if (lButton.isPressedAndReleased()) {
      currentStepOfFullRev = 0;
      encoder.setValue(0);
      delay(500);
    }

    // switch over to adjusting step size
    if (rButton.isPressedAndReleased()) {
      stepSizeNum = adjustJogSize(stepSizeNum, currentStepOfFullRev);
    }

    if (encoderButton.isPressedAndReleased()) {
      currentMode = UNSELECTED_MODE;
      return;
    }
  }
}

int adjustJogSize(int currentStepSize, long currentStepOfFullRev) {
  encoder.configure(0, 12, currentStepSize, 1);
  while (true) {
    lcd.setCursor(0,1);
    lcd.print("Step:[");
    lcd.print(stepsToDegrees(stepSizes[encoder.value()]), 5);
    lcd.print(DEGREE);
    lcd.print("]");

    if (rButton.isPressedAndReleased()) {
      int stepSizeNum = encoder.value();

      lcd.setCursor(0,1);
      lcd.print("Step: ");
      lcd.print(stepsToDegrees(stepSizes[stepSizeNum]), 5);
      lcd.print(DEGREE);
      lcd.print("   ");

      encoder.configure(-5l * STEPS_PER_FULL_REV, 5l*STEPS_PER_FULL_REV, currentStepOfFullRev, stepSizes[stepSizeNum]);
      return stepSizeNum;
    }
  }
}
