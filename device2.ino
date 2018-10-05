#include <Servo.h>
#include <SoftwareSerial.h>

//Communication struct removed for space

#define PORT_ND -1
#define PWM_PORTS_COUNT 6
const byte PWM_port[PWM_PORTS_COUNT] = {3,5,6,9,10,11}; 

//========================

//=======================

#define GARDEN_LED 8
#define GARDEN_LIGHT_SENSOR A2
#define SUNLIGHT 50

#define SERVO_GARAGE 6
#define IS_DOOR_OPEN 1
#define UP_BTN_PORT A1
#define DOWN_BTN_PORT A0
#define GARAGE_SENSOR_ECHO 9
#define GARAGE_SENSOR_TRIG 10
#define MIN_MICRO_SERVO 800
#define MAX_MICRO_SERVO 2450
#define COLLISION_DISTANCE 30
#define SERVO_SPEED 50

struct ServoWithObjectDetection{
public:
  ServoWithObjectDetection(byte servoPort, byte trigPin, byte echoPin){
    mServoPort = servoPort;
    mTrigPin = trigPin;
    mEchoPin = echoPin;
  }
  void setup(){
    mServo.attach(mServoPort);
    pinMode(mTrigPin, OUTPUT);
    pinMode(mEchoPin, INPUT);
  }
  void loop(){
    now = millis();    
    
    if(now - prev >= 100){      
      prev = now;
      if(moving){        
        digitalWrite(mTrigPin, LOW);
        delayMicroseconds(2);
    
        digitalWrite(mTrigPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(mTrigPin, LOW);
    
        duration = pulseIn(mEchoPin, HIGH);
        distance = duration*0.034/2;
        if(distance <= COLLISION_DISTANCE){
          delayMicroseconds(1000);
          return;
        }
        if(moving180){
          mSecondsMove += SERVO_SPEED;
          if(mSecondsMove >= MAX_MICRO_SERVO){
            moving = moving0 = moving180 = false;
            isOpen = true;
          }else{
            mServo.writeMicroseconds(mSecondsMove);  
          }
        }else if(moving0){          
          mSecondsMove -= SERVO_SPEED;
          if(mSecondsMove <= MIN_MICRO_SERVO){
            moving = moving0 = moving180 = false;
            isOpen = false;
          }else{
            mServo.writeMicroseconds(mSecondsMove);
          }
        }
      }
    }
  }
  
  void to180(){
    moving = moving180 = true;
  }
  
  void to0(){
    moving = moving0 = true;
  }

  Commands getDoorState(){
    if(moving && moving180)
      return OpeningDoor;
    if(moving && moving0)
      return ClosingDoor;
    if(isOpen)
      return DoorOpened;
    return DoorClosed ;
  }
  
  void writeMs(int ms){
    mServo.writeMicroseconds(ms);
    mSecondsMove = ms;
  }
 
private:
  byte mServoPort = PORT_ND;
  Servo mServo;
  bool moving = false;
  bool moving180 = false;
  bool moving0 = false;
  int mSecondsMove = 0;
  bool isOpen = true;//Assuming that the door start open

  unsigned long prev = 0;
  unsigned long now = 0;

  byte mTrigPin = -1;
  byte mEchoPin = -1;
  long duration = 0;
  int distance = 0;
};

struct GarageDoor{
  GarageDoor(int upBtnPort, int downBtnPort, bool defDoorPos){
    mDefDoorPos= defDoorPos;
    mUpBtnPort = upBtnPort;
    mDownBtnPort = downBtnPort;
  }
  
  void setup(){
    mServo.setup();
    if(mDefDoorPos){
        mServo.writeMs(MAX_MICRO_SERVO);
    }else{
      mServo.writeMs(MIN_MICRO_SERVO);    
    }    
    pinMode(mDownBtnPort, INPUT);
    pinMode(mUpBtnPort , INPUT);
  }

  void loop(Message m){
    if(m.Command!=0)
    {
      Serial.println("");
      Serial.print("Garage Door Received: ");
      Serial.print(m.Command);
      Serial.println("");
      switch(m.Command){
        case Commands::OpenDoor:{
          mServo.to180();
          break;
        }
        case Commands::CloseDoor:{       
          mServo.to0();
          break;
        }
        default:
          //Serial.println("UNKNOW COMMAND");
          break;
      }
    }
    if(digitalRead(mUpBtnPort) ==  HIGH){//Open door
      mServo.to180();
    } 
    else if(digitalRead(mDownBtnPort) ==  HIGH){//Close door
      mServo.to0();
    }

    mServo.loop();
  }
  Commands getDoorState() {
    return mServo.getDoorState();
  }
  
private:
  ServoWithObjectDetection mServo = ServoWithObjectDetection(SERVO_GARAGE, GARAGE_SENSOR_TRIG, GARAGE_SENSOR_ECHO);  
  bool mDefDoorPos = false;
  bool switchState = false;
  int garagePort = -1; 
  int mUpBtnPort = -1;
  int mDownBtnPort = -1;
};

struct Phototransistor{
  Phototransistor():Phototransistor(PORT_ND){}
  
  Phototransistor(byte port){
    mPort = port;
  }
    
  void setup(){}
  
  void setPort(byte port){
    mPort = port;
  }
  const bool isLighted(){
    return mIsLighted;
  }
  
  void loop(){
    aux =  analogRead(mPort);
    mIsLighted = aux > SUNLIGHT;
  }
  private:  
    byte mPort = PORT_ND;
    bool mIsLighted = 0;
    int aux = -1;
};

#define DEF_LED_STATE LOW
#define DEF_LED_BRIGHTNESS 0

struct LED{
public:
  LED():LED(PORT_ND){};
  
  LED(byte port, bool state = DEF_LED_STATE, byte brightness = DEF_LED_BRIGHTNESS){
    mPort = port;
    mState = state;
    mBrightness = brightness;
    for(byte i = 0; i < PWM_PORTS_COUNT; i++){
      if(port == PWM_port[i]){
          isPWM = true;
            state = HIGH;
            break;
        }
    }
  }
  void setup(){
    pinMode(mPort, OUTPUT);
  }
  void setPort(byte port){
    mPort = port;
  }
  void setState(bool state){
    this->mState = state;
  }
  bool isOn(){
    return this->mState;
  }
  void setBrightness(byte brightness){
    this->mBrightness = brightness;
  }
  
  void loop(){
    write();
  }

private:
  void write(){
    if(!isPWM){
      digitalWrite(mPort, this->mState);   
      return;
    }
    analogWrite(mPort, this->mBrightness);
  }
  byte mPort = 0;
  bool mState = false;
  byte mBrightness = 0;
  bool isPWM = false;
};

struct LEDSwtichedByLight{
public:
  LEDSwtichedByLight(byte ledPort, byte photoTransPort){
    mGardenLight.setPort(ledPort);
    mPhotoTrans.setPort(photoTransPort);
  }
  void setup(){
    mGardenLight.setup();
    mPhotoTrans.setup();
  }
  void loop(Message m){
    if(m.Command!=0)
    {      
      switch(m.Command){
        case Commands::OnGardenLight:{          
          mGardenLight.setState(HIGH);
          aut = false;
          break;
        }
        case Commands::OffGardenLight :{          
          mGardenLight.setState(LOW);
          aut = true;
          break;
        }
        default:
          //Serial.println("UNKNOW COMMAND");
          break;
      }
    }
    
    if(aut){
      mPhotoTrans.loop();
      if(mPhotoTrans.isLighted())
        mGardenLight.setState(LOW);
      else
        mGardenLight.setState(HIGH);
    }
    mGardenLight.loop();
  } 
 
  bool isLightOn(){
    return mGardenLight.isOn();
 }
  
private:
  LED mGardenLight = LED();
  Phototransistor mPhotoTrans = Phototransistor();  
  bool aut = true;
};

struct HomeAutomation {
public:
  void setup(){
    gardenDev.setup();
    Serial.println("GARDEN LIGHT SETUP DONE. DEVICE 2");
    garageDoor.setup();
    Serial.println("GARAGE AUTO-DOOR SETUP DONE. DEVICE 2");
  }
  void loop(Message m){
    if(m.Command!=0)
    {      
      switch(m.Command){
        case Commands::GetDoorState:{
          Commands st = garageDoor.getDoorState();          
          comm.sendCommand(Message((byte)st));
          break;
        }
        case Commands::GetGardenLightState:{
          if(gardenDev.isLightOn()){
            comm.sendCommand(Message(GardenLightOn));
          }else{
            comm.sendCommand(Message(GardenLightOff));
          }
          break;
        }
        default:
          //Serial.println("UNKNOW COMMAND");
          break;
      }
    }
    
    gardenDev.loop(m);
    garageDoor.loop(m);
  }
private:
  LEDSwtichedByLight gardenDev = LEDSwtichedByLight(GARDEN_LED, GARDEN_LIGHT_SENSOR);
  GarageDoor garageDoor = GarageDoor(UP_BTN_PORT, DOWN_BTN_PORT, IS_DOOR_OPEN);  
};

HomeAutomation devMgr;

void setup()
{
   comm.setup();  
   Serial.begin(9600); //Enable write to monitor for debugging   
   devMgr.setup();
}

void loop()
{
  comm.loop();
  Message m = comm.readMessage(); 
  devMgr.loop(m);
}

