

#include <Wire.h>
#include "Adafruit_DRV2605.h"
#include "FiniteStateMachine.h"

#include "LowPower.h"

#include "FBD.h"
#define CHECKINGTIME 2000 // ms

const uint8_t LED1_RED = 12;
const uint8_t LED1_GREEN = 13;
const uint8_t LED1_BLUE = A1;
const uint8_t LED1DRV = 9;

const uint8_t LED6_RED = 4;
const uint8_t LED6_GREEN = 1;
const uint8_t LED6_BLUE = 2;
const uint8_t LED6DRV = 3;

const uint8_t LED2_RED = A0;
const uint8_t LED2_GREEN = 10;
const uint8_t LED2_BLUE = 0;
const uint8_t LED2DRV = 11;

const uint8_t LED3_GREEN = 8; // blue
const uint8_t LED3_RED = 7; // amber
const uint8_t LED3TOPDRV = 5;
const uint8_t LED3BOTDRV = 6;

const uint8_t SENSORIN = A3;
const uint8_t AT24INT = A2;
const uint8_t AT24VDD = 14;

void fadeupEnter();
State fadeup(fadeupEnter, NULL, NULL);
State fadedown(NULL);
FiniteStateMachine controlMachine(fadeup);
uint8_t ledSel = 0;
uint32_t lastPWMTime = millis();
Adafruit_DRV2605 drv;


State checkCapsense(NULL);
State powerSleep(NULL);
FiniteStateMachine stateMachine(checkCapsense);


TON sensorTON(100);
Rtrg sensorTrg;

void setup()
{
    // init gpio pins
    pinMode(LED1_RED, OUTPUT);
    pinMode(LED1_GREEN, OUTPUT);
    pinMode(LED1_BLUE, OUTPUT);
    pinMode(LED1DRV, OUTPUT);

    pinMode(LED6_RED, OUTPUT);
    pinMode(LED6_GREEN, OUTPUT);
    pinMode(LED6_BLUE, OUTPUT);
    pinMode(LED6DRV, OUTPUT);

    pinMode(LED2_RED, OUTPUT);
    pinMode(LED2_GREEN, OUTPUT);
    pinMode(LED2_BLUE, OUTPUT);
    pinMode(LED2DRV, OUTPUT);

    pinMode(LED3_GREEN, OUTPUT);
    pinMode(LED3_RED, OUTPUT);
    pinMode(LED3TOPDRV, OUTPUT);
    pinMode(LED3BOTDRV, OUTPUT);

    pinMode(SENSORIN, OUTPUT);
    digitalWrite(SENSORIN, LOW);

    pinMode(AT24INT, INPUT);

    pinMode(AT24VDD, OUTPUT);
    digitalWrite(AT24VDD, HIGH);

    // DRV2605 init part
    drv.begin();
    drv.useLRA();
    drv.selectLibrary(6);
}


void turnOnCap(bool turn)
{
    if(turn)
        digitalWrite(AT24VDD, HIGH);
    else
        digitalWrite(AT24VDD, LOW);
}

static bool PB6Status = false;
uint8_t effect = 1;
void loop()
{
    /*
    const uint32_t INTERVAL = 2000;
    if (millis() - lastPWMTime > 50)
    {
    lastPWMTime = millis();
    if (controlMachine.isInState(fadeup))
    {
    uint8_t pwmVal = map(controlMachine.timeInCurrentState(), 0, INTERVAL, 0, 255);
    led1Drive(bitRead(ledSel, 0), bitRead(ledSel, 1), bitRead(ledSel, 2), pwmVal);
    }
    else
    {
    uint8_t pwmVal = map(controlMachine.timeInCurrentState(), 0, INTERVAL, 255, 0);
    led1Drive(bitRead(ledSel, 0), bitRead(ledSel, 1), bitRead(ledSel, 2), pwmVal);
    }
    }

    if (controlMachine.isInState(fadeup))
    {
    if (controlMachine.timeInCurrentState() > INTERVAL)
    controlMachine.transitionTo(fadedown);
    }
    else if (controlMachine.isInState(fadedown))
    {
    if (controlMachine.timeInCurrentState() > INTERVAL)
    controlMachine.transitionTo(fadeup);
    }
    controlMachine.update(); //*/

    /*
    // set the effect to play
    drv.setWaveform(0, effect);  // play effect 
    drv.setWaveform(1, 0);       // end waveform
    drv.go();
    // wait a bit
    delay(500);

    effect++;
    if (effect > 117) effect = 1;*/
    

    /*
    byte error, address;
    int nDevices;

    nDevices = 0;
    for (address = 1; address < 127; address++)
    {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        nDevices++;
    }
    if (nDevices == 0)
        led1Drive(true, false, false, 255);
    else
        led1Drive(false, true, false, 255);

    delay(5000);           // wait 5 seconds for next scan //*/

    sensorTON.IN = digitalRead(AT24INT) == HIGH;
    sensorTON.update();

    if (stateMachine.isInState(checkCapsense))
    {
        turnOnCap(true);
        if (sensorTON.Q)
            led6Drive(true, false, false, 255);
        else
            led6Drive(false, true, false, 255);
        
        /*
        byte error, address;
        int nDevices;
        nDevices = 0;
        for (address = 1; address < 127; address++)
        {
            Wire.beginTransmission(address);
            error = Wire.endTransmission();
            nDevices++;
        }
        if (nDevices == 0)
            led6Drive(true, false, false, 255);
        else
            led6Drive(false, true, false, 255);
        delay(1000);
        */

        if (stateMachine.timeInCurrentState() > CHECKINGTIME)
        {
            // led1Drive(false, true, false, 255);
            // stateMachine.transitionTo(powerSleep);
        }
    }
    else if (stateMachine.isInState(powerSleep))
    {
        turnOnCap(false);
        LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
        stateMachine.transitionTo(checkCapsense);
        // led1Drive(true, false, false, 255);
    }
    stateMachine.update();
}

void led1Drive(bool red, bool green, bool blue, uint8_t value)
{
    analogWrite(LED1DRV, value);
    if (red)
        digitalWrite(LED1_RED, LOW);
    else
        digitalWrite(LED1_RED, HIGH);

    if (green)
        digitalWrite(LED1_GREEN, LOW);
    else
        digitalWrite(LED1_GREEN, HIGH);

    if (blue)
        digitalWrite(LED1_BLUE, LOW);
    else
        digitalWrite(LED1_BLUE, HIGH);
}

void led6Drive(uint8_t red, uint8_t green, uint8_t blue, uint8_t value)
{
    analogWrite(LED6DRV, value);
    if (red)
        digitalWrite(LED6_RED, LOW);
    else
        digitalWrite(LED6_RED, HIGH);

    if (green)
        digitalWrite(LED6_GREEN, LOW);
    else
        digitalWrite(LED6_GREEN, HIGH);

    if (blue)
        digitalWrite(LED6_BLUE, LOW);
    else
        digitalWrite(LED6_BLUE, HIGH);
}

void led2Drive(bool red, bool green, bool blue, uint8_t value)
{
    analogWrite(LED2DRV, value);
    if (red)
        digitalWrite(LED2_RED, LOW);
    else
        digitalWrite(LED2_RED, HIGH);

    if (green)
        digitalWrite(LED2_GREEN, LOW);
    else
        digitalWrite(LED2_GREEN, HIGH);

    if (blue)
        digitalWrite(LED2_BLUE, LOW);
    else
        digitalWrite(LED2_BLUE, HIGH);
}

void led3Drive(uint8_t caseNumber, uint8_t pwm1, uint8_t pwm2)
{
    /*
    No,  G,  R,  B,  A, TopDrive, BotDrive, Top, Bot
    0,   1   0   0   0      1       0        0    1
    1,   1   1   0   0      1       0        0    0
    2,   0   1   0   0      1       0        1    0
    3,   0   0   1   1      0       1        0    0
    4,   0   0   1   0      0       1        0    1
    5,   0   0   0   1      0       1        1    0
    6,   1   1   1   1      1       1        0    0
    7,   1   0   1   0      1       1        0    1
    8,   0   1   0   1      1       1        1    0
    */

    /*
    switch (caseNumber)
    {
    case 0:
    {
    digitalWrite(LED3TOPDRV, HIGH);
    digitalWrite(LED3BOTDRV, LOW);
    SoftPWMSet(LED3_GREEN, pwm1);
    SoftPWMSet(LED3_RED, 0);
    }
    break;

    case 1:
    {
    digitalWrite(LED3TOPDRV, HIGH);
    digitalWrite(LED3BOTDRV, LOW);
    SoftPWMSet(LED3_GREEN, pwm1);
    SoftPWMSet(LED3_RED, pwm2);
    }
    break;

    case 2:
    {
    digitalWrite(LED3TOPDRV, HIGH);
    digitalWrite(LED3BOTDRV, LOW);
    SoftPWMSet(LED3_GREEN, 0);
    SoftPWMSet(LED3_RED, pwm2);
    }
    break;

    case 3:
    {
    digitalWrite(LED3TOPDRV, LOW);
    digitalWrite(LED3BOTDRV, HIGH);
    SoftPWMSet(LED3_GREEN, pwm1);
    SoftPWMSet(LED3_RED, pwm1);
    }
    break;

    case 4:
    {
    digitalWrite(LED3TOPDRV, LOW);
    digitalWrite(LED3BOTDRV, HIGH);
    SoftPWMSet(LED3_GREEN, pwm1);
    SoftPWMSet(LED3_RED, 0);
    }
    break;

    case 5:
    {
    digitalWrite(LED3TOPDRV, LOW);
    digitalWrite(LED3BOTDRV, HIGH);
    SoftPWMSet(LED3_GREEN, 0);
    SoftPWMSet(LED3_RED, pwm2);
    }
    break;

    case 6:
    {
    digitalWrite(LED3TOPDRV, HIGH);
    digitalWrite(LED3BOTDRV, HIGH);
    SoftPWMSet(LED3_GREEN, pwm1);
    SoftPWMSet(LED3_RED, pwm1);
    }
    break;

    case 7:
    {
    digitalWrite(LED3TOPDRV, HIGH);
    digitalWrite(LED3BOTDRV, HIGH);
    SoftPWMSet(LED3_GREEN, pwm1);
    SoftPWMSet(LED3_RED, 0);
    }
    break;

    case 8:
    {
    digitalWrite(LED3TOPDRV, HIGH);
    digitalWrite(LED3BOTDRV, HIGH);
    SoftPWMSet(LED3_GREEN, 0);
    SoftPWMSet(LED3_RED, pwm2);
    }
    break;
    }*/
}

void fadeupEnter()
{
    ledSel = random(1, 7);
}

long readVcc()
{
    // Read 1.1V reference against AVcc
    // set the reference to Vcc and the measurement to the internal 1.1V reference
#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
#else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#endif  
    delay(2); // Wait for Vref to settle
    ADCSRA |= _BV(ADSC); // Start conversion
    while (bit_is_set(ADCSRA, ADSC)); // measuring

    uint8_t low = ADCL; // must read ADCL first - it then locks ADCH  
    uint8_t high = ADCH; // unlocks both

    long result = (high << 8) | low;

    result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
    return result; // Vcc in millivolts
}