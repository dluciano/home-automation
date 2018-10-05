#include <SoftwareSerial.h>
//======================================
//Communication struct removed for space
//======================================

#define FIRE_BUZZ_PORT 4
#define FLAME_DETECTOR_PORT 5
#define FIRE_LIGHT_PORT 7
#define SWITCH_ALARM_PORT 13

#define REPORT_ALARM_TIME 30

struct FlameDetector{
  FlameDetector(byte flamePort, byte lightPort, byte buzzPort, byte switchAlamPort){
    mFlamePort = flamePort;
    mLightPort = lightPort;
    mBuzzPort = buzzPort;
    mSwitchAlamPort = switchAlamPort;
  }
  
  void setup(){
    pinMode(mFlamePort, INPUT);  
    pinMode(mLightPort, OUTPUT);  
    pinMode(mBuzzPort, OUTPUT);  
    pinMode(mSwitchAlamPort, INPUT);
    Serial.println("FIRE ALARM SETUP DONE. DEVICE 4");    
  }

  void loop(Message m){    
    now = millis();    
    
    switch(m.Command){
      case Commands::ActivateAlarm:
        alarmActive = true;
        break;
      case Commands::DeactivateAlarm:
        alarmActive = false;
      break;
      case GetFireAlarmStatus:{
        if(alarmActive)
          comm.sendCommand(Message(Panic));
        else
          comm.sendCommand(Message(StopPanic));
        break;
      }
      default:
        //Serial.println("UNKNOW COMMAND");
        break;
    }  
    //alarmActive = digitalRead(mFlamePort) == LOW;    
        
    if(digitalRead(mSwitchAlamPort) == HIGH){
        alarmActive = !alarmActive;
    }
    checkFire();
  }

  private:
    void checkFire(){
      if(now - prev >= 200){
        prev = now;
        if(alarmActive){
          reportOff = true;
          beepOn();
          if(sendAlarmCount >= REPORT_ALARM_TIME || sendAlarmCount == 0){
            comm.sendCommand(Message(Commands::Panic));
            sendAlarmCount = 1;
          }
          sendAlarmCount++;
        }
        else{
          sendAlarmCount = 0;
          off();
          if(reportOff){            
            comm.sendCommand(Message(Commands::StopPanic));
            reportOff  = false;
          }
        }
      }      
    }
    
    void beepOn(){
      auxLightOn = !auxLightOn;
      digitalWrite(mLightPort, auxLightOn ? HIGH : LOW);
      digitalWrite(mBuzzPort, auxLightOn ? HIGH : LOW);
    }    
    
    void off(){
      digitalWrite(mLightPort, LOW);
      digitalWrite(mBuzzPort, LOW);
      auxLightOn = false;
    }
    
    byte mFlamePort = 0;
    byte mLightPort =  0;
    byte mBuzzPort = 0;
    byte mSwitchAlamPort = 0;
    bool alarmActive = false;
    unsigned long prev = 0;
    unsigned long now = 0;
    bool auxLightOn = false;
    bool shouldWait = false;
    int waitTime = 0;
    bool manualActivation = false;
    bool reportOff = false;
    int sendAlarmCount = 0;
};

FlameDetector fm = FlameDetector(FLAME_DETECTOR_PORT, FIRE_LIGHT_PORT, FIRE_BUZZ_PORT, SWITCH_ALARM_PORT);

void setup() {
  Serial.begin(9600);
  comm.setup();
  fm.setup(); 
}

void loop() {  
  comm.loop();
  Message m = comm.readMessage();
  fm.loop(m);  
}
