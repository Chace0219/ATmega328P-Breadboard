/*
Red mode should not have any events, frequency of events should increase from there through progression of colors (white event every 10 seconds)

// Confirm that breathing profile matches client supplied numbers (in 1.66, out 3.66)

When event triggers, a slow pulsing light (with vibration, like is currently implemented) burns until the user holds the cap touch.  This starts the actual breathing event. 10 minute timeout if user does not start breathing event (let's make this 1 minute for now).  We can probably have vibration for only the first couple light cycles if it makes power management easier.

After changing color mode, 3 second pause before selected light goes off
*/

#include <Wire.h>
#include "Adafruit_DRV2605.h"
#include "FiniteStateMachine.h"
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include "LowPower.h"
#include "TimeLib.h"
#include "FBD.h"
#include <timer-api.h>

#define DEBUGSERIAL

#define CONFIG_VERSION "ls1"
#define CONFIG_START 32

uint32_t touchCnt = 0;
uint32_t eventTime = 5000;

//interupt color system - Ben
#define RED 0
#define ORANGE 1
#define YELLOW 2
#define GREEN 3
#define BLUE 4
#define PURPLE 5
#define VIOLET 5
#define WHITE 6

byte LEDcolor = 0;

//*********************************

// Example settings structure
struct StoreStruct {
    // This is for mere detection if they are your settings
    char version[4];
    // The variables of your settings
    uint8_t colorNum;
} setting = {
    CONFIG_VERSION,
    0
};

void loadConfig() {
    if (EEPROM.read(CONFIG_START + 0) == CONFIG_VERSION[0] &&
        EEPROM.read(CONFIG_START + 1) == CONFIG_VERSION[1] &&
        EEPROM.read(CONFIG_START + 2) == CONFIG_VERSION[2])
        for (unsigned int t = 0; t<sizeof(setting); t++)
            *((char*)&setting + t) = EEPROM.read(CONFIG_START + t);
}

void saveConfig() {
    for (unsigned int t = 0; t<sizeof(setting); t++)
        EEPROM.write(CONFIG_START + t, *((char*)&setting + t));
}

SoftwareSerial mySerial(255, 12); // RX, TX

#define CHECKINGTIME 2000 // ms

const uint8_t LED6_RED = 4;
const uint8_t LED6_GREEN = 1;
const uint8_t LED6_BLUE = 2;
const uint8_t LED6DRV = 3;

const uint8_t SENSORIN = A3;
const uint8_t AT24INT = A2;
const uint8_t AT24VDD = 9;

uint8_t ledSel = 0;
uint32_t lastPWMTime = millis();
Adafruit_DRV2605 drv;

State startup(NULL);
State powerSleep(NULL);
State wakeupSOL(NULL);
State notificationState(NULL);
State breathingState(NULL);
State colorRestState(NULL);
State colorSelectState(NULL);
FiniteStateMachine eventManager(startup);

// 
void touchHapticLRAEnter();
void touchHapticLRAExit();
void touchHapticLRAUpdate();
State touchHapticLRA(touchHapticLRAEnter, touchHapticLRAUpdate, touchHapticLRAExit);
State realtimeLRA(NULL);
State drvError(NULL);
FiniteStateMachine lraManager(drvError);

TON touchTON(50);
Rtrg touchTrg;
Rtrg touchRelTrg;
TON touchHoldTON(1000);
Rtrg touchHoldTrg;
Ftrg touchHoldRel;

TON battLowTON(500);
Rtrg sleepTrg;
Ftrg WakeTrg;

Rtrg eventPeakTrg;

uint8_t notifyCnt = 0;
uint8_t breathingCnt = 0;
#define BATTTHRESHOLD 3100 //3200

void setup()
{
    // init gpio pins
    pinMode(LED6_RED, OUTPUT);
    pinMode(LED6_GREEN, OUTPUT);
    pinMode(LED6_BLUE, OUTPUT);
    pinMode(LED6DRV, OUTPUT);

    pinMode(SENSORIN, OUTPUT);
    digitalWrite(SENSORIN, LOW);

    pinMode(AT24INT, INPUT);

    pinMode(AT24VDD, OUTPUT);
    digitalWrite(AT24VDD, HIGH);

    // 
    setPwmFrequency(3, 8);
    timer_init_ISR_20KHz(TIMER_DEFAULT);

    delay(100);
    // DRV2605 init part
    drv.begin();
    drv.useLRA();
    drv.selectLibrary(6);
    // Set Real-Time Playback mode
    drv.setMode(DRV2605_MODE_REALTIME);
#ifdef DEBUGSERIAL
    mySerial.begin(9600);
    mySerial.println(F("zen board project"));
#endif // DEBUGSERIAL
    // 
    loadConfig();

    turnOnCap(true);
    const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013
    setTime(DEFAULT_TIME);
}


void turnOnCap(bool turn)
{
    if (turn)
        digitalWrite(AT24VDD, HIGH);
    else
        digitalWrite(AT24VDD, LOW);
}

uint8_t effect = 1;

void displayColorLed(uint8_t colorNum, uint8_t pwmValue)
{
    LEDcolor = colorNum;
    if (pwmValue == 0)
        digitalWrite(LED6DRV, LOW);
    else
        analogWrite(LED6DRV, pwmValue);
}

uint32_t getEventDelay()
{
    uint32_t result = 0;
    double rand = random(0, 15000) / 15000.0F;
    if (touchCnt)
        result = sin(rand) * 24000 / touchCnt;
    else
        result = sin(rand) * 24000;
    return result * 3; // I factored by 3 to increase time for testing - ben
}

void loop()
{
    // every sec, checking drv2605 has connected or not
    static uint32_t lastLRACheckTime = millis();
    if (millis() - lastLRACheckTime > 1000)
    {
        if (checkLRA())
        {
            if (lraManager.isInState(drvError))
            { // if drv2605 has connected, it will init module and enter realtime mode
                drv2605_init();
                drv2605_Realtime();
                lraManager.transitionTo(realtimeLRA);
            }
        }
        else
            lraManager.transitionTo(drvError);
        lastLRACheckTime = millis();
    }



    static uint32_t vibratingLastTime = millis();
    static uint32_t battDisplayTime = millis();

    if (!eventManager.isInState(startup))
    {
        uint16_t battVolt = map(readVcc(), 2900, 3700, 3140, 4000);
        if (millis() - battDisplayTime > 3000)
        {
            debugPrint(F("current batt voltage is "));
            debugPrintln(String(battVolt));
            battDisplayTime = millis();
        }

        if (hour() == 0 && minute() == 0)
        {
            if (second() == 0)
            {
                debugPrintln(F("reset day touch count "));
                touchCnt = 0;
            }
        }
        battLowTON.IN = battVolt < BATTTHRESHOLD;
        if (!eventManager.isInState(notificationState))
            battLowTON.update();
        if (battLowTON.Q)
        {
            debugPrintln(F("entered sleep state"));
        }

        if (sleepTrg.Q)
        {
            turnOnCap(false);
            eventManager.transitionTo(powerSleep);
            led6Drive(false, false, false, 0);
        }
        else if (WakeTrg.Q)
        {
            eventManager.transitionTo(wakeupSOL);
            // drv2605_init();
            turnOnCap(true);
        } // */

        sleepTrg.IN = battLowTON.Q;
        sleepTrg.update();
        WakeTrg.IN = battLowTON.Q;
        WakeTrg.update();
    }

    touchTON.IN = digitalRead(AT24INT) == HIGH;
    touchTON.update();
    touchTrg.IN = touchTON.Q;
    touchTrg.update();

    touchRelTrg.IN = touchTON.Q;
    touchRelTrg.update();

    touchHoldTON.IN = digitalRead(AT24INT) == HIGH;
    touchHoldTON.update();
    touchHoldTrg.IN = touchHoldTON.Q;
    touchHoldTrg.update();

    touchHoldRel.IN = touchHoldTON.Q;
    touchHoldRel.update();

#ifdef NNDEBUGSERIAL
    mySerial.print(F("touch Value: "));
    mySerial.print(digitalRead(AT24VDD));
    mySerial.print(F(" "));
    mySerial.println(digitalRead(AT24INT));
#endif

    if (eventManager.isInState(startup))
    {
        displayColorLed(RED, 200);
        if (eventManager.timeInCurrentState() > 50)
        {
            uint16_t battVolt = map(readVcc(), 2900, 3700, 3140, 4000);
            if (battVolt > BATTTHRESHOLD)
                eventManager.transitionTo(wakeupSOL);
            else
                eventManager.transitionTo(powerSleep);
        }
    }
    else if (eventManager.isInState(powerSleep))
    {
        uint32_t tempTime = now();
        LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
        setTime(tempTime + 8);
    }
    else if (eventManager.isInState(wakeupSOL))
    {

        if (eventManager.timeInCurrentState() < 200)
            displayColorLed(RED, 200);
        else if (eventManager.timeInCurrentState() < 400)
            displayColorLed(ORANGE, 200);
        else if (eventManager.timeInCurrentState() < 600)
            displayColorLed(YELLOW, 200);
        else if (eventManager.timeInCurrentState() < 800)
            displayColorLed(GREEN, 200);
        else if (eventManager.timeInCurrentState() < 1000)
            displayColorLed(BLUE, 200);
        else if (eventManager.timeInCurrentState() < 1200)
            displayColorLed(PURPLE, 200);
        else if (eventManager.timeInCurrentState() < 1400)
            displayColorLed(WHITE, 200);
        else
        {
            displayColorLed(WHITE, 0); //OFF
            eventTime = getEventDelay();

            notifyCnt = 0;
            eventManager.transitionTo(notificationState);
            debugPrintln(F("entered into notifiation state"));
            lraManager.immediateTransitionTo(touchHapticLRA);
        }
    }
    else if (eventManager.isInState(notificationState))
    {
        static uint16_t pwmValue;

        const uint32_t FADEINTIME = 1500;
        const uint32_t FADEOUTTIME = 1500;
        const uint32_t TUNROFFTIME = 5000;

        uint16_t elapsedTime = eventManager.timeInCurrentState();
        if (elapsedTime < FADEINTIME)
            pwmValue = map(elapsedTime, 0, FADEINTIME, 0, 255);
        else if (elapsedTime < FADEINTIME + FADEOUTTIME)
            pwmValue = map(elapsedTime - FADEINTIME, 0, FADEOUTTIME, 255, 0);
        else
            pwmValue = 0;
        displayColorLed(WHITE, pwmValue);

        if (touchTrg.Q)
        {
            debugPrintln(F("entered into breathing state"));
            breathingCnt = 0;
            eventManager.transitionTo(breathingState);
        }
        else if (eventManager.timeInCurrentState() > (FADEINTIME + FADEOUTTIME + TUNROFFTIME))
        {
            notifyCnt++;
            if (notifyCnt < 8)
            {
                eventManager.transitionTo(notificationState);
                lraManager.immediateTransitionTo(touchHapticLRA);
            }
            else
            {
                notifyCnt = 0;
                eventTime = getEventDelay();

                debugPrintln(F("transistion to rest "));
                eventManager.transitionTo(colorRestState);
                displayColorLed(WHITE, 0);
                if (!lraManager.isInState(drvError))
                    drv.setRealtimeValue(0);
            }
        }
    }
    else if (eventManager.isInState(breathingState))
    {
        static uint16_t pwmValue;
#define FADEIN 1660
#define FADEOUT 3330
        uint16_t elapsedTime = eventManager.timeInCurrentState();
        if (elapsedTime < FADEIN)
            pwmValue = map(elapsedTime, 0, FADEIN, 0, 255);
        else
            pwmValue = map(elapsedTime - FADEIN, 0, FADEOUT, 255, 0);
        displayColorLed(WHITE, pwmValue);

        if (millis() - vibratingLastTime > 100)
        {
            vibratingLastTime = millis();
            if (!lraManager.isInState(drvError))
            {
                int8_t vibValue = map(pwmValue, 0, 255, 0, 35); // max level of realtime for drv2605 
                drv.setRealtimeValue(vibValue);
            }
        }

        if (touchRelTrg.Q || touchTON.IN == false)
        {
            debugPrintln(F("transistion to rest "));
            eventManager.transitionTo(colorRestState);
            displayColorLed(WHITE, 0);
        }

        if (eventManager.timeInCurrentState() > FADEOUT + FADEIN)
        {
            breathingCnt++;
            if (breathingCnt < 4)
            {
                eventManager.transitionTo(breathingState);
            }
            else
            {
                breathingCnt = 0;
                eventTime = getEventDelay();
                debugPrintln(F("transistion to rest "));
                eventManager.transitionTo(colorRestState);
                displayColorLed(WHITE, 0);
                if (!lraManager.isInState(drvError))
                    drv.setRealtimeValue(0);
            }
        }
    }
    else if (eventManager.isInState(colorRestState))
    {
        if (touchTrg.Q)
        { // touch clicked

          // trigger touch haptic event
            if (checkLRA())
            {
                if (lraManager.isInState(drvError))
                    drv2605_init();
                lraManager.immediateTransitionTo(touchHapticLRA);
            }
            else
                lraManager.transitionTo(drvError);

            // display selected color as 250
            displayColorLed(setting.colorNum, 250);
            eventManager.transitionTo(colorSelectState);
            debugPrintln(F("touch event happend in color select state"));
        }
        if (eventManager.timeInCurrentState() > eventTime && setting.colorNum > 0)
        {
            led6Drive(false, false, false, 0);
            notifyCnt = 0;

            if (checkLRA())
            {
                if (lraManager.isInState(drvError))
                {
                    drv2605_init();
                    drv2605_Realtime();
                    lraManager.immediateTransitionTo(realtimeLRA);
                }
            }
            else
                lraManager.transitionTo(drvError);

            debugPrintln(F("entered into notification state"));
            lraManager.immediateTransitionTo(touchHapticLRA);
            eventManager.transitionTo(notificationState);
        }
    }
    else if (eventManager.isInState(colorSelectState))
    {
        // event if touch has happened, when only color mode is not RED, we need to confirm this part
        // if (touchTrg.Q && setting.colorNum != RED)
        if (touchTrg.Q)
        { // touch clicked
            touchCnt++;

            debugPrintln(F("touch event happend in color select state"));
            debugPrint(F("today pressed touch count is "));
            debugPrintln(String(touchCnt));

            // trigger touch haptic event
            if (checkLRA())
            {
                if (lraManager.isInState(drvError))
                {
                    drv2605_init();
                    drv2605_Effect();
                }
                lraManager.immediateTransitionTo(touchHapticLRA);
            }
            else
                lraManager.transitionTo(drvError);

            setting.colorNum++;
            if (setting.colorNum > 6)
                setting.colorNum = 0;
            saveConfig();
            displayColorLed(setting.colorNum, 250);
            eventManager.transitionTo(colorSelectState);
        }

        if (eventManager.timeInCurrentState() > 3000)
        {
            led6Drive(false, false, false, 0);
            eventTime = getEventDelay();
            eventManager.transitionTo(colorRestState);
            displayColorLed(WHITE, 0);

            debugPrintln(F("enterend into rest state again"));
            debugPrint(F("calculated interval time is "));
            debugPrint(String(eventTime));
            debugPrintln(F("ms"));
        }
    }
    eventManager.update();
    lraManager.update();
}

void led6Drive(uint8_t red, uint8_t green, uint8_t blue, uint8_t value)
{
    return;
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
void setPwmFrequency(int pin, int divisor) {
    byte mode;
    if (pin == 5 || pin == 6 || pin == 9 || pin == 10) {
        switch (divisor) {
        case 1: mode = 0x01; break;
        case 8: mode = 0x02; break;
        case 64: mode = 0x03; break;
        case 256: mode = 0x04; break;
        case 1024: mode = 0x05; break;
        default: return;
        }
        if (pin == 5 || pin == 6) {
            TCCR0B = TCCR0B & 0b11111000 | mode;
        }
        else {
            TCCR1B = TCCR1B & 0b11111000 | mode;
        }
    }
    else if (pin == 3 || pin == 11) {
        switch (divisor) {
        case 1: mode = 0x01; break;
        case 8: mode = 0x02; break;
        case 32: mode = 0x03; break;
        case 64: mode = 0x04; break;
        case 128: mode = 0x05; break;
        case 256: mode = 0x06; break;
        case 1024: mode = 0x07; break;
        default: return;
        }
        TCCR2B = TCCR2B & 0b11111000 | mode;
    }
}

void digitalClockDisplay() {
    // digital clock display of the time
    mySerial.print(hour());
    printDigits(minute());
    printDigits(second());
    mySerial.print(" ");
    mySerial.print(day());
    mySerial.print(" ");
    mySerial.print(month());
    mySerial.print(" ");
    mySerial.print(year());
    mySerial.println();
}

void printDigits(int digits) {
    // utility function for digital clock display: prints preceding colon and leading 0
    mySerial.print(":");
    if (digits < 10)
        Serial.print('0');
    mySerial.print(digits);
}

void touchHapticLRAEnter()
{
    drv.setRealtimeValue(0);
    drv2605_Effect();

    // set the effect to play
    drv.setWaveform(0, 1); // effect 1
    drv.setWaveform(1, 0);
    drv.go();

}

void drv2605_Realtime()
{
    drv.setMode(DRV2605_MODE_REALTIME);
}

void drv2605_Effect()
{
    drv.setMode(DRV2605_MODE_INTTRIG);
}

// init drv2605 module
void drv2605_init()
{
    drv.begin();
    drv.useLRA();
    drv.selectLibrary(6);
}

void touchHapticLRAExit()
{
    drv.stop();
    drv2605_Realtime();
    drv.setRealtimeValue(0);

}

void touchHapticLRAUpdate()
{
    if (lraManager.timeInCurrentState() > 500)
        lraManager.transitionTo(realtimeLRA);
}

bool checkLRA()
{
    int8_t error;
    Wire.beginTransmission(DRV2605_ADDR);
    error = Wire.endTransmission();

    if (error != 0)
    {
        debugPrintln(F("drv2605 does not exist on bus"));
        return false;
    }
    return true;
}

static unsigned long blinkCount = 0; // use volatile for shared variables
void timer_handle_interrupts(int timer) {
    blinkCount = blinkCount + 1;  // increase when LED turns on

                                  //    if(blinkCount & (B0000010 >> 1)) //blinkCount % 2 ==1)
    switch (LEDcolor)
    {
    case WHITE:
        if (blinkCount % 2 == 1)
        {
            pinMode(LED6_RED, OUTPUT);
        }
        else
        {
            pinMode(LED6_RED, INPUT);
            if (blinkCount % 4 == 2)
            {
                pinMode(LED6_BLUE, INPUT);
                pinMode(LED6_GREEN, OUTPUT);
            }
            else
            {
                pinMode(LED6_GREEN, INPUT);
                pinMode(LED6_BLUE, OUTPUT);
            }
        }
        break;
    case RED:
        pinMode(LED6_RED, OUTPUT);
        break;
    case ORANGE:
        if (blinkCount % 4 == 1)
        {
            delayMicroseconds(250);
            pinMode(LED6_RED, INPUT);
            pinMode(LED6_GREEN, OUTPUT);
        }
        else
        {
            pinMode(LED6_GREEN, INPUT);
            pinMode(LED6_RED, OUTPUT);
            pinMode(LED6_BLUE, INPUT);
        }
        break;
    case YELLOW:
        if (blinkCount % 4 == 1)
        {
            pinMode(LED6_RED, INPUT);
            pinMode(LED6_GREEN, OUTPUT);
        }
        else
        {
            delayMicroseconds(250);
            pinMode(LED6_GREEN, INPUT);
            pinMode(LED6_RED, OUTPUT);
            pinMode(LED6_BLUE, INPUT);
        }
        break;
    case GREEN:
        pinMode(LED6_RED, INPUT);
        pinMode(LED6_GREEN, OUTPUT);
        pinMode(LED6_BLUE, INPUT);
        break;
    case BLUE:
        pinMode(LED6_RED, INPUT);
        pinMode(LED6_GREEN, INPUT);
        pinMode(LED6_BLUE, OUTPUT);
        break;
    case PURPLE:
        if (blinkCount % 4 == 1)
        {
            pinMode(LED6_RED, INPUT);
            pinMode(LED6_BLUE, OUTPUT);
        }
        else
        {
            delayMicroseconds(200);
            pinMode(LED6_GREEN, INPUT);
            pinMode(LED6_RED, OUTPUT);
            pinMode(LED6_BLUE, INPUT);
        }
        break;
    default:
        break;
    }
}

void debugPrint(String debugMsg)
{
#ifdef DEBUGSERIAL
    mySerial.print(debugMsg);
#endif
}

void debugPrintln(String debugMsg)
{
#ifdef DEBUGSERIAL
    mySerial.println(debugMsg);
#endif
}