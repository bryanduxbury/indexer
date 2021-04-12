// include the library code:
#include <LiquidCrystal.h>
#include "A4988.h"

// LCD
#define RS 10
#define EN 11
#define DB4 A0
#define DB5 A1
#define DB6 A2
#define DB7 A3
LiquidCrystal lcd(RS, EN, DB4, DB5, DB6, DB7);

// stepper motor
#define STEPS_PER_REV 200
#define MICROSTEPS 16
#define WORM_RATIO 72l
#define STEPS_PER_FULL_REV (WORM_RATIO * STEPS_PER_REV * MICROSTEPS)
#define DEG_PER_STEP (float) (360.0 / STEPS_PER_FULL_REV)

#define DIR_PIN 4
#define STEP_PIN 5

A4988 stepper(STEPS_PER_REV, DIR_PIN, STEP_PIN);

// rotary encoder
#define ENCODER_P1 3
#define ENCODER_P2 2
#define ENCODER_SW_PIN 9
int lastMSB = 0;
int lastLSB = 0;
volatile int lastEncoded = 0;
volatile long encoderValue = 0;

volatile long minEncoderValue = 0;
volatile long maxEncoderValue = 0;
volatile long encoderStepValue = 1;

long getEncoderValue() {
  return encoderValue / 4;
}

void configureEncoder(long minval, long maxval, long curval, long step) {
  minEncoderValue = minval;
  maxEncoderValue = 4 * maxval;
  encoderValue = 4 * curval;
  encoderStepValue = step;
}

#define R_BUTTON_PIN 6
#define L_BUTTON_PIN 7

// system config
const int minDivs = 1;
const int maxDivs = 720;

#define UNSELECTED_MODE -1
#define DIV_MODE 0
#define DIV_MENU_MODE 2
#define JOG_MODE 1

int currentMode = UNSELECTED_MODE;

void setup() {
  lcd.begin(20, 4);
  
  stepper.begin(240, MICROSTEPS);

  // all the rotary pins are pulled high
  pinMode(ENCODER_P1, INPUT);
  digitalWrite(ENCODER_P1, HIGH);
  pinMode(ENCODER_P2, INPUT);
  digitalWrite(ENCODER_P2, HIGH);
  pinMode(ENCODER_SW_PIN, INPUT);
  digitalWrite(ENCODER_SW_PIN, HIGH);
  
  // call updateEncoder() when any high/low changed seen
  // on interrupt 0 (pin 2), or interrupt 1 (pin 3)
  attachInterrupt(0, updateEncoder, CHANGE);
  attachInterrupt(1, updateEncoder, CHANGE);

  // forward switch (pulled high)
  pinMode(R_BUTTON_PIN, INPUT);
  digitalWrite(R_BUTTON_PIN, HIGH);
  // reverse switch
  pinMode(L_BUTTON_PIN, INPUT);
  digitalWrite(L_BUTTON_PIN, HIGH);
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

    break;
  }
}

void menu() {
  lcd.clear();
  lcd.setCursor(0, 3);
  lcd.print("SELECT |       |    ");

  configureEncoder(0, 2, 0, 1);

  while (!isRotaryPressed()) {
    lcd.setCursor(0,0);
    if (getEncoderValue() == 0) {
      lcd.print("[Div Mode]");
    } else {
      lcd.print(" Div Mode ");
    }

    lcd.setCursor(0,1);
    if (getEncoderValue() == 1) {
      lcd.print("[Jog Mode]");
    } else {
      lcd.print(" Jog Mode ");
    }

    lcd.setCursor(0,2);
    if (getEncoderValue() == 2) {
      lcd.print("[Config]");
    } else {
      lcd.print(" Config ");
    }  
    delay(100);
  }

  waitForRotaryRelease();

  switch (getEncoderValue()) {
    case DIV_MODE:
    configureDivMode();
    break;

    case JOG_MODE:
    configureJogMode();
    break;
    
    default:
    ;
  }
}

// div mode state
int numDivs = -1;
int curDiv = -1;
long curStep = 0;

bool isRotaryPressed() {
  return digitalRead(ENCODER_SW_PIN) == LOW;
}

void waitForRotaryRelease() {
  while (isRotaryPressed()) {
    delay(100);  
  }
}


void configureDivMode() {
  lcd.clear();
  lcd.print("Number of divisions?");

  lcd.setCursor(0,3);
  lcd.print("SELECT |       |    ");

  configureEncoder(1, 720, 72, 1);

  // make sure we don't consider the rotary button's low value as user action until it's become high again
  waitForRotaryRelease();
  
  while (!isRotaryPressed()) {
    lcd.setCursor(0,1);
    lcd.print(getEncoderValue());
    lcd.print(" (");
    float degPerDiv = 360.0 / getEncoderValue();
    float stepsPerDiv = degPerDiv / DEG_PER_STEP;
    lcd.print(degPerDiv, 5);
    lcd.print(" deg)");

    lcd.setCursor(0,2);
    lcd.print("Err:");
    lcd.print(stepsPerDiv, 5);

    delay(100);
  }

  waitForRotaryRelease();
  
  numDivs = getEncoderValue();
  curDiv = 0;
  curStep = 0;
  setupDivMode();
}

void setupDivMode() {
  currentMode = DIV_MODE;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Divs:      Deg:     ");

  lcd.setCursor(0,3);
  lcd.print("MENU  | PREV |  NEXT");
}

int normalizeCurDiv() {
  if (curDiv < 0) {
    return numDivs - ((curDiv + 1) % numDivs);
  } else {
    return (curDiv + 1) % numDivs;
  }
}

void divMode() {  
  lcd.setCursor(0, 1);
  lcd.print(normalizeCurDiv());
  lcd.print("/");
  lcd.print(numDivs);
  lcd.print("  ");

  lcd.setCursor(11, 1);
  lcd.print(360.0 / numDivs * curDiv, 5);

  int moveDir = 0;
  
  if (digitalRead(R_BUTTON_PIN) == LOW) {
    // advance button pressed? step forward
    moveDir = 1;
  } else if (digitalRead(L_BUTTON_PIN) == LOW) {
    // prev button pressed? step backwards
    moveDir = -1;
  }

  if (moveDir != 0)  {
    long desiredStep = round((float) STEPS_PER_FULL_REV / numDivs * (curDiv + moveDir));
    long requiredSteps = desiredStep - curStep;

    lcd.setCursor(0,2);
    lcd.print(desiredStep);
    lcd.print(" ");
    lcd.print(curStep);

    stepper.move(requiredSteps);
    curDiv += moveDir;
    curStep += requiredSteps;
  }

  // rotary button pressed == div mode menu
  if (isRotaryPressed()) {
    currentMode = DIV_MENU_MODE;
  }
}

void divMenu() {
  lcd.clear();
  
  configureEncoder(0, 2, 0, 1);

  waitForRotaryRelease();

  while (!isRotaryPressed()) {
    lcd.setCursor(0,0);
    if (getEncoderValue() == 0) {
      lcd.print("[Back]");
    } else {
      lcd.print(" Back ");
    }

    lcd.setCursor(0,1);
    if (getEncoderValue() == 1) {
      lcd.print("[Zero Divs]");
    } else {
      lcd.print(" Zero Divs ");
    }

    lcd.setCursor(0,2);
    if (getEncoderValue() == 2) {
      lcd.print("[Abort]");
    } else {
      lcd.print(" Abort ");
    }  
    delay(100);
  }

  waitForRotaryRelease();

  switch (getEncoderValue()) {
    case 0:
    setupDivMode();
    break;

    case 1:
    curDiv = 0;
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
  lcd.print("Target:     Current:");
  lcd.setCursor(0,3);
  lcd.print(" MENU | ZERO | ???");

  configureEncoder(0, STEPS_PER_FULL_REV, 0, STEPS_PER_REV * MICROSTEPS * 0.1);

  currentMode = JOG_MODE;
}

float stepsToDegrees(long step) {
  return (float) step / STEPS_PER_FULL_REV * 360.0;
}

void jogMode() {
  long currentStepOfFullRev = 0;
  
  while (true) {
    lcd.setCursor(0,1);
    lcd.print(stepsToDegrees(getEncoderValue()), 5);

    lcd.setCursor(12,1);
    lcd.print(stepsToDegrees(currentStepOfFullRev), 5);

    lcd.setCursor(0,2);
    lcd.print("Res:");
    lcd.print(360.0 / STEPS_PER_FULL_REV, 5);

    long targetStepOfFullRev = getEncoderValue();
    if (currentStepOfFullRev != targetStepOfFullRev) {
      long stepDelta = currentStepOfFullRev - targetStepOfFullRev;

      stepper.move(stepDelta);
      currentStepOfFullRev = targetStepOfFullRev;
    }

    delay(100);
    
  }

  
}


void updateEncoder(){
  int MSB = digitalRead(ENCODER_P1); 
  int LSB = digitalRead(ENCODER_P2);

  //converting the 2 pin value to single number 
  int encoded = (MSB << 1) | LSB; 

  //adding it to the previous encoded value 
  int sum = (lastEncoded << 2) | encoded; 
  
  if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) 
    encoderValue += encoderStepValue; 
  if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) 
    encoderValue -= encoderStepValue; 

  if(encoderValue < minEncoderValue) encoderValue = minEncoderValue;
  if(encoderValue > maxEncoderValue) encoderValue = maxEncoderValue;

  //store this value for next time
  lastEncoded = encoded;  
}
