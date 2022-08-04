// Project e-Silk: synthetic spider silk with electrical conductivity
// by Pau Anta, more info at https://www.pauanta.bio.

// Development of a programmable device consisting of a micro-syringe pump and a spinning machine, based on Arduino microcontroller.
// The micro-syringe pump pushes the spidroins dope into a coagulation bath, at a programmable flowrate,
// and the spinning machine will spin the silk threads into a capture roller, also at the desired speed.

// Hardware inspired by the OpenSyringePump developed by Naroom, https://github.com/manimino/OpenSyringePump. All software is original.
// Started project on March 2020.

// ALERT: The maximum flowrate of this microsyringe pump is 12.4 µL/min with the current specifications (changes with motor speed, thread size, and syringe needle radius)

#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include "utility/Adafruit_MS_PWMServoDriver.h"
#include <math.h>
#define PI

Adafruit_MotorShield AFMS = Adafruit_MotorShield(); 
Adafruit_StepperMotor *myMotorV = AFMS.getStepper(200, 2);      // Stepper motor for the microsyringe pump, facing down
Adafruit_StepperMotor *myMotorH = AFMS.getStepper(200, 1);      // Stepper motor for the capture roller, facing sideways

const int UpButton = 2;                   // Button to move plunge up manually
const int DownButton = 3;                 // Button to move plunge down manually
const int StartStopButton = 4;            // Button to start/stop movement of microsyringe at predetermined flowrate

int DownButtonState = 0;
int UpButtonState = 0;
int StartStopButtonState = 0;

const float SyringeRadius = 4.4;          // Inner radius of the syringe in mm. NEED TO SET UP BASED ON YOUR SYRINGE
float FlowRate = 3;                       // How fast the dope is pushed out of the syringe in µL/min. NEED TO SET UP AT YOUR DESIRED FLOW RATE
float DopeVolume = 0.5;                   // The volume of spidroin dissolved in a chemical in mL
float CaptureSpeed = 0.2;                 // The speed of the capture motor in meters per minute. NEED TO SET UP AT A DESIRED CAPTURE SPEED
float MaxFlowRate;                        // The maximum flow rate, depends on the thread size and the radius of the syringe

const float ThreadSize = 1.25;            // Thread size of the threaded rod in mm
const float TimePerStep = 3.8;            // Time it takes the motor to make 1 step in ms.

const int StepsPerRevolution = 200;       // 200 steps equal a single 360º turn of the stepper motor
const int Rebound = 2000;                 // Need this variable to control the buttons. Average time spent holding down the buttons in ms

float VolumePerStep;                      // The dope volume pushed per 1 step, in µL
float VolumePerRevolution;                // The dope volume pushed per 200 steps, in µL
float FlowRateInSteps;                    // The flowrate in steps
float VTotalSteps;                        // Number of steps to push out all the dope
int StepCounter = 0;
float DopeRemaining;                      // The volume of dope remaining
float DopeProgress;                       // Percentage of dope pumped    
float VTimeForAllSteps;                   // The time it takes to do all microsteps not including the delay in between for the syringe (vertical) motor
float VMotorInterval;

float Progress = 0;
float TimeRemaining;

float HMotorThreshold = 2.5;              // The percentage of dope that must be ejected for the capture motor to begin working

const float DistancePerRevolution = 0.1335;  // The length of spider silk captured per revolution in meters
float DistancePerStep;                    // The length of spider silk captured per step in meters                       
float HTimeForAllSteps;                   // The time it takes to do all microsteps not including the delay inbetween for the capture (horizontal) motor
float CaptureSpeedInSteps;
float HMotorInterval;                  

unsigned long VPreviousMotorTime;
unsigned long HPreviousMotorTime;

unsigned long VCurrentMotorTime;
unsigned long HCurrentMotorTime;

float TimeNow;
float TimeElapsed;


void setup() {
  
  Serial.begin(57600);

  AFMS.begin();
  
  pinMode(DownButton, INPUT);
  pinMode(UpButton, INPUT);
  pinMode(StartStopButton, INPUT);

  Serial.println("Dope volume:  " + String(DopeVolume, 5) + " mL");
  Serial.println("Flow rate in µL:  " + String(FlowRate) + " µL/min");

  unsigned long VPreviousMotorTime = millis();
  unsigned long HPreviousMotorTime = millis();

  //Calculation of total number of steps for pumping the dope volume

  VolumePerRevolution = (M_PI * ThreadSize * sq(SyringeRadius));          //Volume of dope pushed out every revolution, in cubic mm (µL)
  Serial.println("VolumePerRevolution: " + String(VolumePerRevolution, 10) + " µL/rev");
  VolumePerStep = VolumePerRevolution / StepsPerRevolution;               //Volume of dope pushed out every step, in µL / step
  Serial.println("VolumePerStep: " + String(VolumePerStep, 10) + " µL/step");
  VTotalSteps = (DopeVolume * 1000) / VolumePerStep;                      //Total number of steps to push out the entire volume
  Serial.println("VTotalSteps: " + String(VTotalSteps) + " Steps");

  //Calculation for max flowrate
  
  MaxFlowRate = (60000/TimePerStep) * VolumePerStep;                      //The maximum flow rate in µL / min
  Serial.println("Maximum flow rate " + String(MaxFlowRate, 5) + " µL/min");

  //Calculation of flowrate in steps per minute

  FlowRateInSteps = FlowRate / VolumePerStep;                             //Converts flowrate from µL/min to steps/min
  Serial.println("Flow rate in steps:  " + String(FlowRateInSteps) + " steps/min");
  
  VTimeForAllSteps = FlowRateInSteps * TimePerStep;                       //Time to complete all the steps (in ms) in one minute
  Serial.println("VMTimeForAllSteps:  " + String(VTimeForAllSteps, 10) + " miliseconds");
  VMotorInterval = (60000 - VTimeForAllSteps) / FlowRateInSteps;          //The interval between each step in ms           
  Serial.println("VMotorInterval:  " + String(VMotorInterval, 3) + " miliseconds");        
  Serial.println("Total time:  " + String(((DopeVolume * 1000) / FlowRate) / 60) + " hours");

  //Calculation for the interval of time between each step for the capture motor

  DistancePerStep = DistancePerRevolution / StepsPerRevolution;           //The length of spider silk captured by the capture roller per step
  Serial.println("DistancePerStep: " + String(DistancePerStep, 10) + " m/step");
  CaptureSpeedInSteps = CaptureSpeed / DistancePerStep;                   //Converts the capture speed from m/min to steps/min
  Serial.println("CaptureSpeedInSteps: " + String(CaptureSpeedInSteps) + " steps/min");
  HTimeForAllSteps = CaptureSpeedInSteps * TimePerStep;                   //Time to complete all steps (in ms) in one minute
  HMotorInterval = (60000 - HTimeForAllSteps) / CaptureSpeedInSteps;      //The interval between each step in ms
  Serial.println("HMotorInterval: " + String(HMotorInterval, 10) + " steps/min");

}

void loop() {
  
  DownButtonState = digitalRead(DownButton);
  if (DownButtonState == LOW) {                 
    myMotorV->step(1, FORWARD, DOUBLE);          //Makes the motor go forward as long as the Down button is held down
    StepCounter = 0;                             //Up or down buttons also reset the microstep counter
  }
  
 UpButtonState = digitalRead(UpButton);
  if (UpButtonState == LOW) {               
    myMotorV->step(1, BACKWARD, DOUBLE);         //Makes the motor go backward as long as the Up button is being held down
    StepCounter = 0;                             //Up or down buttons also reset the microstep counter
  }
  
 StartStopButtonState = digitalRead(StartStopButton);
  if (StartStopButtonState == LOW) {            //Makes the motor eject the dope at the desired flow rate
    Serial.println("Start!");
    StartPump();
    CaptureStart();  
    }
 
}

void StartPump() {                              //Makes the syringe pump push at the predetermined speed
  
  TimeElapsed = millis();

  Serial.println("Pumping Starts!");
  
  StartStopButtonState = HIGH;
  
  delay(Rebound);

  TimeNow = millis();

  while ((StartStopButtonState == HIGH) and (StepCounter < VTotalSteps)) {
    
    TimeRemaining =  millis() - TimeNow;
    unsigned long VCurrentMotorTime = millis();
    unsigned long HCurrentMotorTime = millis();
    
    if(VCurrentMotorTime - VPreviousMotorTime > (VMotorInterval + TimePerStep)){
      myMotorV->onestep(FORWARD, SINGLE);
      VPreviousMotorTime = VCurrentMotorTime;
      StepCounter = StepCounter + 1;
      DopeRemaining = (DopeVolume - ((StepCounter * VolumePerStep) / 1000));
      DopeProgress = ((DopeVolume - DopeRemaining) / DopeVolume) * 100;
      Serial.println("Dope progress " + String(DopeProgress, 1) + "%");
      Serial.println("Time remaining:  " + String(((DopeVolume * 1000) / FlowRate) - (TimeRemaining / 60000)) + " minutes");

      StartStopButtonState = digitalRead(StartStopButton);

    }

    if(HCurrentMotorTime - HPreviousMotorTime > (HMotorInterval + TimePerStep) and DopeProgress > HMotorThreshold){
      myMotorH->onestep(BACKWARD, SINGLE);
      HPreviousMotorTime = HCurrentMotorTime;
    }

    StartStopButtonState = digitalRead(StartStopButton);

   }
   
    TimeElapsed = millis() - TimeElapsed;
    Serial.println("Time elapsed: " + (String(TimeElapsed / 1000)) + "seconds");
    delay(Rebound);
    Serial.println("Pumping Done!");

  }

void CaptureStart() {

  delay(Rebound);

  StartStopButtonState == HIGH;

  Serial.println("Capture Starts!");

  while (StartStopButtonState == HIGH) {

    unsigned long HCurrentMotorTime = millis();

    if((HCurrentMotorTime - HPreviousMotorTime) > (HMotorInterval + TimePerStep)){
      myMotorH->onestep(BACKWARD, SINGLE);
      HPreviousMotorTime = HCurrentMotorTime;
   
    }
  
  StartStopButtonState = digitalRead(StartStopButton);

  }

  Serial.println("Capture Done!");
  delay(Rebound);

}
