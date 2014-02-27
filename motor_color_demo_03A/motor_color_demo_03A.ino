#include <TimerOne.h>

/************************
  *  Authors: Sam Z     *
  *           Cooper L  *
  *           Chris R   *
  *  Date: 2/4/2014     *
  *  Project:           *
  *    H-Bridge Arduino *
  *    Motor Control    *
  ***********************/

/* TODO:
  - Change color_diff to buffer for average
    * this prevents noise from triggering unwanted state-changes
  - Separate 9 and 6 V power supplies
*/

const int PIN_GREEN_LED = 53;
const int PIN_YELLO_LED = 52;

// Motor Control Pins
const int MOTOR_LEFT_F  = 2;  
const int MOTOR_LEFT_R  = 4;
const int MOTOR_RIGHT_F = 3;
const int MOTOR_RIGHT_R = 6; // comm PWM outputs on pin 5

// State values
const int STATE_STOPPED    = 0;
const int STATE_FORWARD    = 1;
const int STATE_REVERSE    = 2;
const int STATE_PIVOT_CCW  = 3;
const int STATE_PIVOT_CW   = 4;
const int STATE_TURN_CCW   = 5;
const int STATE_TURN_CW    = 6;
const int STATE_SEARCHING  = 7;

// Motor Constants
const float TURN_SPEED_RATIO = .25;
const int STATE_CHANGE_DELAY = 200;
const int PIVOT_TIME_90 = 630;
const int PIVOT_TIME_60 = 420;

// Color Sensor Constants
const int RED_LED_PIN = 15;
const int BLUE_LED_PIN = 14;
const int COLOR_SENSOR_PIN = 0;
const int LED_FLASH_PERIOD = 41000000;
const int MIN_DIFF_THRESHOLD = 20;
const int COLOR_SENSE_BUFFER_SIZE = 2;
const int RED_COLOR  = -1;
const int BLUE_COLOR = 1;
const int NEUTRAL_COLOR = 0;

// Color sensor variables
boolean color_sensor_output = LOW;
int last_red     = -1;
int last_blue    = -1;
int color_diff   = 0;
int calib_offset = 0; 
int color_sense_buffer[COLOR_SENSE_BUFFER_SIZE];
int color_sense_buffer_index = 0;

// Searching algorithm paramters
long last_search_time = 0;

// Current arduino state
int current_state;
float motor_duty_cycle;

void setup() {
  
  // Establish color params
  Serial.begin(9600);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  
  // Calibrate lights
  for(int i = 0; i < COLOR_SENSE_BUFFER_SIZE; ++i)
  {
     color_sense_buffer[i] = 0;
  }
  delay(500);
  int calib_iter  = 2;
  int temp_offset = 0;
  for(int i = 0; i < calib_iter; i++)
  {
    delay(250);
    flash();
    delay(250);
    flash();
    temp_offset = color_diff;
  }
  calib_offset = temp_offset;
  Timer1.initialize(50000);
  Timer1.attachInterrupt(flash);
  
  // Establish H-Bridge inputs as outputs from arduino
  pinMode(MOTOR_LEFT_F,  OUTPUT);
  pinMode(MOTOR_LEFT_R,  OUTPUT);
  pinMode(MOTOR_RIGHT_F, OUTPUT);
  pinMode(MOTOR_RIGHT_R, OUTPUT);
  
  // GREEN/YELLO indicator LEDs
  pinMode(PIN_GREEN_LED, OUTPUT);
  pinMode(PIN_YELLO_LED, OUTPUT);
  
  // Initialize state machine to desired initial state
  set_state(STATE_STOPPED);
  motor_duty_cycle = .18;  
  
  // Searching parameters
  
}

void loop() {
  
  // If RED!
  int c_color = calculate_color();
  //Serial.print("The time diff is: ");
  //Serial.println(millis() - last_search_time);
  
  // if BLUE OR RED
  if((c_color == BLUE_COLOR)
          && current_state != STATE_FORWARD)
    {
      set_state(STATE_FORWARD);
    }
  else if((c_color == RED_COLOR))
   {
     set_state(STATE_STOPPED);
   }
  else if(c_color == NEUTRAL_COLOR && current_state != STATE_SEARCHING)
  {
    set_state(STATE_SEARCHING);
  }
  
  // Performs state actions
  handle_state();
  
  Serial.println("left motor: ");
  Serial.println(MOTOR_RIGHT_R);
 
}

// Sets state and gives delay
void set_state(int new_state)
{
  // prevent shoot-through, set state
  delay(STATE_CHANGE_DELAY);
  stop_motor();
  current_state = new_state;
  
  // state specific event_handling
  if(current_state == STATE_SEARCHING)
    last_search_time = millis();
  
}

void handle_state()
{
    // Iterate through FSM and perform current state
  // Not using else-if's in case interrupt causes 
  // state changes
  if (current_state == STATE_STOPPED)
  {
    stop_motor();
  }
  
  if (current_state == STATE_FORWARD)
  {
    analogWrite(MOTOR_LEFT_F,  duty_cycle_to_byte(motor_duty_cycle));
    analogWrite(MOTOR_LEFT_R,  duty_cycle_to_byte(0));
    analogWrite(MOTOR_RIGHT_F, duty_cycle_to_byte(motor_duty_cycle));
    analogWrite(MOTOR_RIGHT_R, duty_cycle_to_byte(0));
  }
  
  if (current_state == STATE_REVERSE)
  {
    analogWrite(MOTOR_LEFT_F,  duty_cycle_to_byte(0));
    analogWrite(MOTOR_LEFT_R,  duty_cycle_to_byte(motor_duty_cycle));
    analogWrite(MOTOR_RIGHT_F, duty_cycle_to_byte(0));
    analogWrite(MOTOR_RIGHT_R, duty_cycle_to_byte(motor_duty_cycle));
  }  
  
  if (current_state == STATE_PIVOT_CW)
  {
    analogWrite(MOTOR_LEFT_F,  duty_cycle_to_byte(motor_duty_cycle));
    analogWrite(MOTOR_LEFT_R,  duty_cycle_to_byte(0));
    analogWrite(MOTOR_RIGHT_F, duty_cycle_to_byte(0));
    analogWrite(MOTOR_RIGHT_R, duty_cycle_to_byte(motor_duty_cycle)); 
  }
  
  if (current_state == STATE_PIVOT_CCW)
  {
    analogWrite(MOTOR_LEFT_F,  duty_cycle_to_byte(0));
    analogWrite(MOTOR_LEFT_R,  duty_cycle_to_byte(motor_duty_cycle));
    analogWrite(MOTOR_RIGHT_F, duty_cycle_to_byte(motor_duty_cycle));
    analogWrite(MOTOR_RIGHT_R, duty_cycle_to_byte(0)); 
  }
  if (current_state == STATE_TURN_CW)
  {
    analogWrite(MOTOR_LEFT_F,  duty_cycle_to_byte(motor_duty_cycle*TURN_SPEED_RATIO));
    analogWrite(MOTOR_LEFT_R,  duty_cycle_to_byte(0));
    analogWrite(MOTOR_RIGHT_F, duty_cycle_to_byte(motor_duty_cycle));
    analogWrite(MOTOR_RIGHT_R, duty_cycle_to_byte(0));    
  }
  if (current_state == STATE_TURN_CCW)
  {
    analogWrite(MOTOR_LEFT_F,  duty_cycle_to_byte(motor_duty_cycle));
    analogWrite(MOTOR_LEFT_R,  duty_cycle_to_byte(0));
    analogWrite(MOTOR_RIGHT_F, duty_cycle_to_byte(motor_duty_cycle*TURN_SPEED_RATIO));
    analogWrite(MOTOR_RIGHT_R, duty_cycle_to_byte(0));    
  }
  if (current_state == STATE_SEARCHING)
  {
    
    if(millis() - last_search_time < 600)
    {
      // Turn clockwise
      analogWrite(MOTOR_LEFT_F,  duty_cycle_to_byte(motor_duty_cycle));
      analogWrite(MOTOR_LEFT_R,  duty_cycle_to_byte(0));
      analogWrite(MOTOR_RIGHT_F, duty_cycle_to_byte(0));
      analogWrite(MOTOR_RIGHT_R, duty_cycle_to_byte(motor_duty_cycle)); 
    }
    else if(millis() - last_search_time < 650)
      stop_motor();
    else if(millis() - last_search_time < 1850)
    {
      // Turn counter-clockwise
      analogWrite(MOTOR_LEFT_F,  duty_cycle_to_byte(0));
      analogWrite(MOTOR_LEFT_R,  duty_cycle_to_byte(motor_duty_cycle));
      analogWrite(MOTOR_RIGHT_F, duty_cycle_to_byte(motor_duty_cycle));
      analogWrite(MOTOR_RIGHT_R, duty_cycle_to_byte(0)); 
    }
    else if(millis() - last_search_time < 2250)
    {
       // Turn clockwise again
      analogWrite(MOTOR_LEFT_F,  duty_cycle_to_byte(motor_duty_cycle));
      analogWrite(MOTOR_LEFT_R,  duty_cycle_to_byte(0));
      analogWrite(MOTOR_RIGHT_F, duty_cycle_to_byte(0));
      analogWrite(MOTOR_RIGHT_R, duty_cycle_to_byte(motor_duty_cycle)); 

    }
    else if(millis() - last_search_time < 2300)
      stop_motor();
    else
      last_search_time = millis();
  }
}

// Sets analog out for motor to zero
void stop_motor()
{
  analogWrite(MOTOR_LEFT_F,  duty_cycle_to_byte(0));
  analogWrite(MOTOR_LEFT_R,  duty_cycle_to_byte(0));
  analogWrite(MOTOR_RIGHT_F, duty_cycle_to_byte(0));
  analogWrite(MOTOR_RIGHT_R, duty_cycle_to_byte(0));  
}

// Converts duty cycle in decimal (between 0 & 1)
// to byte for analog read
byte duty_cycle_to_byte(float duty)
{
  return (byte) (duty * 255);
}

void flash() {
  
   // Set most recent red/blue 
  if(!color_sensor_output) last_red = analogRead(COLOR_SENSOR_PIN);
  else last_blue = analogRead(COLOR_SENSOR_PIN);
  
  // Toggle between red/blue
  color_sensor_output = !color_sensor_output;
  digitalWrite(BLUE_LED_PIN, color_sensor_output);
  digitalWrite(RED_LED_PIN, !color_sensor_output);
  
  // Sets the color difference if we have values in the buffer for both
  if (last_blue > 0 && last_red > 0)
  {
    color_diff = (last_blue - last_red)- calib_offset;
    color_sense_buffer[color_sense_buffer_index] = color_diff;
    color_sense_buffer_index = 
      (color_sense_buffer_index + 1) % COLOR_SENSE_BUFFER_SIZE;
      
  }
}

int calculate_color()
{
  int i;
  int num_red = 0;
  int num_blue = 0;
  
  // Iterate through buffer, calculate number of red/blue readings
  for(i = 0; i < COLOR_SENSE_BUFFER_SIZE; i++)
  {
    
    if(color_sense_buffer[i] < -1*(MIN_DIFF_THRESHOLD))
    {
      ++num_red;
    }
    
    else if(color_sense_buffer[i] > MIN_DIFF_THRESHOLD)
    {
      ++num_blue;
    }
  }
  
  // return 1 if blue
  if(num_blue > (int)(.8*(float)COLOR_SENSE_BUFFER_SIZE))
  {
    return 1;
  }
  
  // return -1 if red
  if(num_red > (int)(.8*(float)COLOR_SENSE_BUFFER_SIZE))
  {
    return -1;
  }
  
  // return 0 if neutral
  return 0;
  
}

