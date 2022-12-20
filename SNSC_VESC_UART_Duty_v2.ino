#include <buffer.h>
#include <crc.h>
#include <datatypes.h>
#include <VescUart.h>

//ADC counts ref to 5.0V
#define THROTTLE_LOW_THRESH 190
#define THROTTLE_HIGH_THRESH 872
#define BRAKE_LOW_THRESH 207
#define BRAKE_HIGH_THRESH 700

//Board
#define TIMER_PIN 10

//Taillight dimming reuses Timer1 normally used for PWM throttle signal generation
#define TIMER_OCR_FULL 1020
#define TIMER_OCR_DIM 400

//Duty control parameters
#define MIN_DUTY_COMMAND 0.01f
#define DUTY_MARGIN 0.005f
//0.5% per-cycle voltage command decrement allowed to prevent chattering between zero current and setDuty modes

//Brake currents
#define BRAKE_CURRENT_MIN 1.0f
#define BRAKE_CURRENT_MAX 45.0f

//Current rampdown (anti-Clonk)
#define CURRENT_RAMPDOWN_INCREMENT 2.0f

//Throttle rate limiting
#define DUTY_INCREMENT_UL 12
//is 1.2% - voltage command increase per cycle (anti-Clonk and smoother torque apply, mainly; note high gain from duty to current)

//Variables
unsigned long throttle_adc;
unsigned long brake_adc;
float         current_command = 0.0;
float         brake_current_command = 0.0;
unsigned long duty_command_ul;
unsigned long duty_command_ul_throttle;
float         duty_command = 0.0;
float         duty_actual = 0.0;
float         current_actual = 0.0;
bool          motor_release_active;
bool          were_braking;

VescUart UART;

void setup() {
  pinMode(TIMER_PIN, OUTPUT);
  //Timer1
  TCCR1A = _BV(COM1B1) | _BV(WGM11);
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS11) | _BV(CS10);
  ICR1 = TIMER_OCR_FULL;
  OCR1B = TIMER_OCR_DIM;
  Serial.begin(250000);
  while(!Serial) {;}
  UART.setSerialPort(&Serial);

}

void loop() {
  
  //Update inverter state
  while(!UART.getVescValues()) {UART.setCurrent(0.0);} //last ditch try to make safe, but we may have lost contact with the inverter if this returns false
  duty_actual = UART.data.dutyCycleNow;
  current_actual = UART.data.avgMotorCurrent;
  
  //Get brake and throttle position
  brake_adc = analogRead(A6);
  throttle_adc = analogRead(A7);
  
  //Most critical safety aspect here
  if(brake_adc > BRAKE_LOW_THRESH) {
    //Brakes on. Ignore throttle, create brake current command and turn brake light on.
    OCR1B = TIMER_OCR_FULL;
    brake_current_command = constrain(map((float)brake_adc, BRAKE_LOW_THRESH, BRAKE_HIGH_THRESH, BRAKE_CURRENT_MIN, BRAKE_CURRENT_MAX), BRAKE_CURRENT_MIN, BRAKE_CURRENT_MAX);
   UART.setBrakeCurrent(brake_current_command);
   were_braking = 1;       //So reenable on brake release doesn't skip rate limit and clunk immediately to full current 
  } else {
    OCR1B = TIMER_OCR_DIM; //Not brake light
    //Convert throttle to duty command
    duty_command_ul_throttle = constrain(map(throttle_adc, THROTTLE_LOW_THRESH, THROTTLE_HIGH_THRESH, 0, 1000), 0, 1000);
    //If we weren't setting duty before, start from the actual duty right now. This prevents "windup" so the rate limit is not laggy on throttle increase.
    if(motor_release_active) {duty_command_ul = (unsigned long)constrain((long)(duty_actual * 1000.0), 0, 1000);}
    //If we were braking, force the command to start ramping from zero. This is because copying the braking duty to driving is undesired.
    if(were_braking) {duty_command_ul = 0;}
    //Otherwise, these are both skipped and duty_command_ul stays at its last updated state.
    //Rate limit duty_command_ul on increases only (decreases are handled by the DUTY_MARGIN zero current release)
    if(duty_command_ul_throttle > duty_command_ul) {
      if(duty_command_ul_throttle - duty_command_ul < DUTY_INCREMENT_UL) {
        duty_command_ul = duty_command_ul_throttle;
      } else {
        duty_command_ul += DUTY_INCREMENT_UL;
      }
    } else { // duty_command_ul_throttle <= duty_command_ul
        duty_command_ul = duty_command_ul_throttle; //Overwrites preload of duty_actual when throttle duty is less than actual duty
    }
    //Convert to floating point duty 0.0-1.0
    duty_command = (float)duty_command_ul / 1000.0;
    //No duty commands smaller than minimum (likely noise), convert to closed throttle
    if(duty_command < MIN_DUTY_COMMAND) {duty_command = 0.0;}
    //Here we reconcile requested and actual duty.
    //If the commanded duty is larger than the actual duty or not less than it by a small margin, we can send that straight to the inverter, and it will limit current.
    //Otherwise, release the motor by ramping the current down to zero.
    if((duty_command >= (duty_actual - DUTY_MARGIN)) && (duty_command > MIN_DUTY_COMMAND)) {
      UART.setDuty(duty_command);
      motor_release_active = 0;
    } else {
      if(!motor_release_active) {
        current_command = current_actual;  //Copy existing current to start ramp
        motor_release_active = 1;          //Don't reload this again, keep open loop ramping to zero amps
      }
      current_command -= CURRENT_RAMPDOWN_INCREMENT;
      current_command = constrain(current_command, 0.0, 60.0);
      UART.setCurrent(current_command);
    }
    were_braking = 0;
  }
}
