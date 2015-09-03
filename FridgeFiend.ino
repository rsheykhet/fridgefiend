#include "types.h"

const long INTERVAL_IDLE_CHECKS_MS=6000L;
const long INTERVAL_AJAR_CHECKS_MS=3000L;
const long INTERVAL_WARN_CHECKS_MS=1000L;
const long INTERVAL_ALRM_CHECKS_MS=1000L;

const long DOOR_AJAR_WARN_THRESHOLD_MS=30000L;
const long DOOR_AJAR_ALRM_THRESHOLD_MS=30000L;
const long DOOR_AJAR_GVUP_THRESHOLD_MS=300000L;

const byte DOOR_AJAR_SOUND_PATTERN_WARN_500_MS=B00000001;
const int DOOR_AJAR_SOUND_PATTERN_WARN_500_MS_SIZE=6;
const byte DOOR_AJAR_SOUND_PATTERN_ALRM_125_MS=B01010101;
const int DOOR_AJAR_SOUND_PATTERN_ALRM_125_MS_SIZE=8;
const byte DOOR_AJAR_LIGHT_PATTERN_AJAR_125_MS=B00000001;
const int DOOR_AJAR_LIGHT_PATTERN_AJAR_125_MS_SIZE=8;
const int LIGHT_SENSOR_READ_INTERVAL=10;
const int SNS_AJAR_THRESHOLD = 1;
long millisToSleep=0L;
long millisInCurrentState=0L;


//#define _DEBUG 
#define _TARGET_ATTINY85


#ifdef _TARGET_ATTINY85
const int LED_PIN = 0;
const int SPK_PIN = 1;
const int SNS_PIN = A1;
const int SNS_ACTIVATION_PIN=3;
#else
const int LED_PIN = 12;
const int SPK_PIN = 13;
const int SNS_PIN = A0;
const int SNS_ACTIVATION_PIN=11;
#endif

#ifdef _DEBUG
# define SERIAL_DEBUG(S) Serial.print((S))
# define SERIAL_DEBUGLN(S) Serial.println((S))
#else
# define SERIAL_DEBUG(S)
# define SERIAL_DEBUGLN(S)
#endif
/* Current state of the door alarm.*/
AjarState ajarState;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(SNS_ACTIVATION_PIN, OUTPUT);
  pinMode(SPK_PIN,OUTPUT);
#ifdef _DEBUG
  Serial.begin(9600);           // set up Serial library at 9600 bps
#endif
}

void loop() {
  doorStateTransition();
  doorStateAction();
  sleepRemainingTime();
}

void sleepRemainingTime() {
  long nMillisToSleepInState;
  switch(ajarState) {
     case AJAR:
          nMillisToSleepInState = INTERVAL_AJAR_CHECKS_MS;
          break;
      case AJAR_WARN:
         nMillisToSleepInState = INTERVAL_WARN_CHECKS_MS;
         break;
      case AJAR_ALARM:
         nMillisToSleepInState = INTERVAL_ALRM_CHECKS_MS;
         break;
      case CLOSED:
      case GAVE_UP:
      default:
        nMillisToSleepInState = INTERVAL_IDLE_CHECKS_MS;
        break;
  }
  long remainingMillis = nMillisToSleepInState - (millisInCurrentState % nMillisToSleepInState); 
  
  SERIAL_DEBUG("Needed to sleep for ");
  SERIAL_DEBUG(nMillisToSleepInState);
  SERIAL_DEBUG(" in state ");
  SERIAL_DEBUG(outputState(ajarState));
  SERIAL_DEBUG(", now remaining: ");
  SERIAL_DEBUGLN(remainingMillis);
  
  doSleep(remainingMillis);
}

void doSleep(long nMillis) {
  if(nMillis > 0) 
  {
    delay(nMillis);
    millisInCurrentState += nMillis;
  }
}

void speakerSound(byte pattern, int unitMs, int patternSize) {
  boolean isSet;
  int pos;
  for(pos=0;pos<patternSize;pos++)
  {
      isSet = (pattern & (1<<(pos))) != 0;
      SERIAL_DEBUG("At position ");
      SERIAL_DEBUG(pos);
      SERIAL_DEBUG(" of ");
      SERIAL_DEBUG(patternSize);
      SERIAL_DEBUG(" in ");
      SERIAL_DEBUG(pattern);
      SERIAL_DEBUG(" bit ");
      SERIAL_DEBUG(1<<(pos));
      SERIAL_DEBUG(" masks to ");
      SERIAL_DEBUG( pattern & (1<<(pos)));
      SERIAL_DEBUG(". Value: ");
      SERIAL_DEBUGLN(isSet);
      if(isSet) {
        SERIAL_DEBUGLN("Tone!");
        tone(SPK_PIN,800);
      }
      else {
        SERIAL_DEBUGLN("Silence!");
        noTone(SPK_PIN);
      }
      doSleep(unitMs);
  }
  SERIAL_DEBUGLN("Silence All!");
  noTone(SPK_PIN);
}

void flashLed(int pattern, int unitMs, int patternSize) {
  boolean isSet;
  int pos;
  for(pos=0;pos<patternSize;pos++)
  {
      isSet = (pattern & (1<<(pos))) != 0;
      digitalWrite(LED_PIN,isSet);
      doSleep(unitMs);
  }
  digitalWrite(LED_PIN,LOW);
}


void doorStateAction() {
     switch(ajarState) {
      case AJAR:
          flashLed(DOOR_AJAR_LIGHT_PATTERN_AJAR_125_MS,125, DOOR_AJAR_LIGHT_PATTERN_AJAR_125_MS_SIZE);
          break;
      case AJAR_WARN:
         speakerSound(DOOR_AJAR_SOUND_PATTERN_WARN_500_MS,500,DOOR_AJAR_SOUND_PATTERN_WARN_500_MS_SIZE);
         break;
      case AJAR_ALARM:
         speakerSound(DOOR_AJAR_SOUND_PATTERN_ALRM_125_MS,125,DOOR_AJAR_SOUND_PATTERN_ALRM_125_MS_SIZE);
         break;
      case CLOSED:
      case GAVE_UP:
      default:
         break;
    }
}

void doorStateTransition() {
  if(!checkDoor()) {
    SERIAL_DEBUGLN("Door is closed");
    setAjarState(CLOSED);
  }
  else {
    switch(ajarState) {
      case CLOSED:
          setAjarState(AJAR);
          break;
      case AJAR:
         moveToStateOnThreshold(AJAR_WARN,DOOR_AJAR_WARN_THRESHOLD_MS);
         break;
      case AJAR_WARN:
         moveToStateOnThreshold(AJAR_ALARM,DOOR_AJAR_ALRM_THRESHOLD_MS);
         break;
      case AJAR_ALARM:
         moveToStateOnThreshold(GAVE_UP,DOOR_AJAR_GVUP_THRESHOLD_MS);
         break;
      case GAVE_UP:
      default:
         break;
    }
  }
}

boolean checkDoor() {
  int lightSensorValue;
  SERIAL_DEBUG("Checking light levels ...");
  digitalWrite(SNS_ACTIVATION_PIN,HIGH);
  doSleep(LIGHT_SENSOR_READ_INTERVAL);
  lightSensorValue = analogRead(SNS_PIN);    
  digitalWrite(SNS_ACTIVATION_PIN,LOW);
  SERIAL_DEBUGLN(lightSensorValue);
  return lightSensorValue >= SNS_AJAR_THRESHOLD;
}

void moveToStateOnThreshold(AjarState nextState, long thresholdMs) {
  if(millisInCurrentState >= thresholdMs)
  {
    SERIAL_DEBUG("Threshhold of ");
    SERIAL_DEBUG(thresholdMs);
    SERIAL_DEBUG("ms exceeded: ");
    SERIAL_DEBUG(millisInCurrentState);
    setAjarState(nextState);
  }
}

void setAjarState(AjarState newState) {
  if(newState != ajarState) 
  {
    SERIAL_DEBUG("Switching to new state: ");
    SERIAL_DEBUG(outputState(newState));
    SERIAL_DEBUG(" from ");
    SERIAL_DEBUGLN(outputState(ajarState));
    
    flashLed(B101, 100, 3);
    
    millisInCurrentState=0;
    ajarState = newState;
  }
}
#ifdef _DEBUG
const char* outputState(AjarState state)
{
  switch(state)
  {
      case AJAR:
       return "AJAR";
      case AJAR_WARN:
       return "WARN";
      case AJAR_ALARM:
       return "ALARM";
      case CLOSED:
       return "CLOSED";
      case GAVE_UP:
       return "GAVE_UP";
      default:
       return "UNKNOWN";
  }
}
#endif

