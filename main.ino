/*  University of Florida                                      (All rights Reserved) 
 *  Department of Chemical Engineering
 *  Project    : Taylor-Couette Cell 
 *  Supervisor : Dr. Ranga Narayanan
 *  Coded by   : Ronald Wilson (Student Assistant)
 *  Build : 1.1
 *  NOTE: Operations can be viewed through serial out
 */

 /* Build 1.1 NOTE: The system has been limited to operate between 0 and 5000 RPM as required by the experiment.
    Operating speed can be increased to upto 10000 RPM if required with tradeoff in resolution. 
    Set Resolution : 0.01V-> 10RPM */

#include<math.h>                                            //for isnan()

int DAQ_input_RPM=10;                                       //defualt value assigned if DAQ is inoperational

// Pin Maps
const int directionControlPin=8;                            // Controls direction of rotation for stepper HIGH->Clockwise LOW->Anti-Clockwise
const int stepperStepPin=9;                                 // Step pulses makes the motor turn LOW->HIGH transition for step
const int stopSensePin=7;                                   // HIGH->initializes the stepper to startPoint. LOW->resets stepper motor to zero 
const int encoderM1ReadInPin=11;                            // Encoder read in for Motor1
const int encoderM2ReadInPin=10;                            // Encoder read in for Motor2
const int rpmM2ReadIn=A2;                                   // RPM input value for Motor2               

/*
   Includes two classes:
   class encoder        ->Handles all encoder functions
   class stepperCustom  ->Handles stepper motor functions along with the PID controller
*/  

void setup() 
{
 pinMode(directionControlPin,OUTPUT);
 pinMode(stepperStepPin,OUTPUT);
 pinMode(stopSensePin,INPUT);
 pinMode(encoderM1ReadInPin,INPUT);
 pinMode(encoderM2ReadInPin,INPUT);
 pinMode(rpmM2ReadIn,INPUT);
 Serial.begin(9600);
}

int readFromDAQ()                                                  			   //Reads the input RPM from DAQ. RPM returned as int
{
  double buff=0;
  buff=analogRead(rpmM2ReadIn);
  buff=5000*(buff/1024);
  return (int)buff+27;                                             				// '27' is the fixed error offset
}

class encoder                           
{
  public:
  long pulse_count;
  int motorID=0, readInPin=0, timeout=0;
  double RPM=0, duration, total_duration, noise_offset=1;          				//constant noise offset
  encoder()
  {
    this->pulse_count=0;
    this->duration=0;
    this->total_duration=0;
  } 
  void readEncoder();                                     						   // Reads current value from both encoders
  void timeoutReset()                                                    // Resets all variable to change RPM to 0
  {
    this->RPM=0;
    this->pulse_count=0;
    this->total_duration=1;
    this->timeout=0;
  }
};

void encoder::readEncoder()
{
  bool flag=true;
  while(flag==true)
  {   
    this->duration = pulseIn(this->readInPin, HIGH);
    if(this->duration!=0)
    {
      this->total_duration+=(this->duration*0.000001);
      this->pulse_count+=1;
      this->timeout=0;
    }
    else
    {
      this->timeout++;                                              			//timeout helps output RPM as 0 if the motor stops instead of waiting   
      if((this->motorID==1)&&(this->timeout==3))                     			//for total_duration to reach 1 second in line 92
      {                                                           	 			//Motor 1 has lower timeout since it is controlled by the pid()
        this->timeoutReset();
        flag=false;
      }
      else if((this->motorID==2)&&(this->timeout==5))               		    //Motor 2 has a higher timout limit to prevent outputting false RPM values
      {                                                              	 			//RPM for motor 2 is critical for the experiment
        this->timeoutReset();
        flag=false;
      }
    }
    if(this->total_duration>=1)                                                 //Ends loop after reading for a total duration of 1 second
       flag=false;
  }
  this->RPM=(this->noise_offset*(this->pulse_count/this->total_duration));      //Calculates RPM here
  this->total_duration=0;         
}

class stepperCustom                                               
{
  private:
  const int startPoint=35;                                      //startPoint determines the RPM at which the motor is initialized
  public: 
  int count=0;                                                  // Keeps track of current position of servo
  void startupM(int delay_value);                               // Initializes stepper motor to starting position (startPoint)
  void shutdownM(int delay_value);                              // Resets stepper to zero position
  bool pid(int rpm, int readOut);                          		// Maintains control using PID w.r.t encoder 
  void stepForward(int n);                                    	// Steps forward for 'n' steps
  void stepBackward(int n);                                   	// Steps backward for 'n' steps
};

void stepperCustom::startupM(int delay_value)
{
    Serial.println("Motor Starting");
    digitalWrite(directionControlPin,HIGH);
    digitalWrite(stepperStepPin,LOW);
    while(this->count<=startPoint)
    {
      digitalWrite(stepperStepPin,HIGH);
      delay(delay_value);
      digitalWrite(stepperStepPin,LOW);
      delay(delay_value);
      //Serial.println(this->count);
      this->count+=1;
    }
    delay(1);
}

void stepperCustom::shutdownM(int delay_value)
{
    Serial.println("Motor Shutdown");
    digitalWrite(directionControlPin,LOW);
    digitalWrite(stepperStepPin,LOW);
    while(this->count>0)
    {
      digitalWrite(stepperStepPin,HIGH);
      delay(delay_value);
      digitalWrite(stepperStepPin,LOW);
      delay(delay_value);
      //Serial.println(this->count);
      this->count-=1;
    }
    delay(1);
    this->count=0;
}

void stepperCustom::stepForward(int steps)
{
    //Serial.println("FWD");
    digitalWrite(directionControlPin,HIGH);
    for(int i=0;i<steps;i++)
    {
      digitalWrite(stepperStepPin,LOW);
      delay(10);
      digitalWrite(stepperStepPin,HIGH);
      delay(10);
      //Serial.println(this->count);
      this->count+=1;
    }
    delay(1);
}

void stepperCustom::stepBackward(int steps)
{
  //Serial.println("BWD");
  digitalWrite(directionControlPin,LOW);
  for(int i=0;i<steps;i++)
  {
    digitalWrite(stepperStepPin,LOW);
    delay(10);
    digitalWrite(stepperStepPin,HIGH);
    delay(10);
    //Serial.println(this->count);
    this->count-=1;
  }
  delay(1);
}

bool stepperCustom::pid(int rpm, int readOut)
{
  int error=rpm-readOut,steps=0;
  int delta=30;                                               //minimum error range. Stops PID controller if error is within delta
  if(abs(error)<=150)                                         //control singal limited to promote slower convergence
	  steps=1;
  else
	  steps=2;
  
  if(error>delta)
  {
    stepForward(steps);
    delay(50);
    return true; 
  }
  else if(error<(-delta))
  {
    stepBackward(steps);
    delay(50);
    return true;
  }
  else
  {
    delay(1);
    return false;
  }
}

void loop()                                                		//main() starts here
{
  bool flag;
  stepperCustom M3;                                            	//M3->Motor 3: refers to stepper motor
  encoder M1,M2;                                                //M1->Motor 1, M2->Motor 2, high speed motor
  
  M1.motorID=1;
  M1.readInPin=encoderM1ReadInPin;
  M1.noise_offset=0.88;                                         //Constant noise offset parameters for encoders
  M2.motorID=2;
  M2.readInPin=encoderM2ReadInPin;
  M2.noise_offset=1.25;
  
  if((M3.count==0)&&(digitalRead(stopSensePin)==HIGH))          //Startup sequence
  {
    M3.startupM(20);
    delay(100);
  }
  
  if((M3.count>0)&&(digitalRead(stopSensePin)==HIGH))
  {
      DAQ_input_RPM=readFromDAQ();
      if(DAQ_input_RPM!=0)
      {
        M1.readEncoder();
        Serial.println("M1 RPM:");
        Serial.println(M1.RPM);
        flag=M3.pid(DAQ_input_RPM,M1.RPM);                      //PID controller assigned to motor 1
        if(flag==false)
        {
          Serial.println("Motor 1 Stabilized");
          M1.readEncoder();
          Serial.println("M1 RPM:");
          Serial.println(M1.RPM);
          Serial.println("M2 RPM:");
          M2.readEncoder();
          Serial.println(M2.RPM);
          Serial.println(" ");
        }
      }
  }

  if((M3.count>0)&&(digitalRead(stopSensePin)==LOW))           //Shutdown sequence
     M3.shutdownM(20);
}
