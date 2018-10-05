#include <SoftwareSerial.h>

//================================
//Communication struct removed for space
//================================
#define KITCHEN_SWITCH_PORT A5
#define KITCHEN_RELAY_OUT_PORT 12

#define ROOM_LED_PORT 11
#define ROOM_SWITCH_PORT A4 
#define ROOM_POT_PORT A3
#define REFRESH_INTERVAL_MS 500

struct ToogleLED{
public:
  ToogleLED(int btn, int relayOut){    
    btnPort = btn;    
    relayOutPort = relayOut;
  }
  void setup(){
    pinMode(btnPort, INPUT);
    pinMode(relayOutPort, OUTPUT);
    isOn = digitalRead(btnPort);
    Serial.println("KITCHEN LED SETUP DONE. DEVICE 3");
  }  
  void loop(Message m){
    if(m.Command!=0)
    {      
      switch(m.Command){
        case Commands::OnKitchenLight:{
          digitalWrite(relayOutPort, HIGH);
          isOn = true;
          break;
        }
        case Commands::OffKitchenLight:{
          digitalWrite(relayOutPort, LOW);          
          isOn = false;
          break;
        }
        case Commands::GetKitchenLightStatus:{
          if(isOn)
            comm.sendCommand(Message(KitchenLightOn));
          else
            comm.sendCommand(Message(KitchenLightOff));          
          break;
        }
        default:
          //Serial.println("UNKNOW COMMAND IN KITCHEN LED");
          break;
      }    
    }
    
    if(digitalRead(btnPort) == 0){
      pushed = false;
      return;
    }
    if(pushed)
      return;
    
    pushed = true;
    isOn = !isOn;
    digitalWrite(relayOutPort, isOn ?  HIGH : LOW);
 }
private:
  bool isOn = false;
  bool pushed = false;
  int btnPort = 0;  
  int relayOutPort = 0;
};

struct ToogleDimmerLED{
public:
  ToogleDimmerLED(int LED, int btn, int pot){
    LED_port = LED;
    btnPort = btn;
    potPort = pot;
  }
  
  void setup(){
    pinMode(LED_port, OUTPUT);
    pinMode(btnPort, INPUT);
    isOn = digitalRead(btnPort);
    readAnal();
    Serial.println("BEDROOM DIMMER LED SETUP DONE. DEVICE 3");
  }  
  //IF the light is dim using the xbee then
  //it will use the automatic settings always
  //for reseting this, reset the arduino or on/off using xbee
  //or press the btn for the bedroom light on in the breadboard
  void loop(Message m){
    if(m.Command!=0)
    {
      switch(m.Command){
        case OnRoomLight:{
          digitalWrite(LED_port, HIGH);
          aut = false;
          isOn = true;
          break;
        }
        case OffRoomLight:{
          digitalWrite(LED_port, LOW);
          aut = false;
          isOn = false;
          break;
        }
        case LightenRoom:{
          changeDim(potVal+10);
          aut = true;
          isOn = true;
          break;
        }
        case DarkenRoom:{
          changeDim(potVal-10);
          aut = true;
          isOn = true;
          break;
        }
        case GetRoomLightStatus:{
          if(isOn)
            comm.sendCommand(Message(RoomLightOn));
          else
            comm.sendCommand(Message(RoomLightOff));
          comm.sendCommand(Message(RoomLightDim, potVal));
          break;
        }
        default:
          //Serial.println("UNKNOW COMMAND IN BEDROOM");
          break;
      }
    }
    if(aut){
      if(digitalRead(btnPort) == HIGH){
        aut = false;
        isOn = !isOn;
        digitalWrite(LED_port, isOn ? HIGH : LOW);
        readAnal();
      }else{
        return;
      }
    }
    
    if((millis() - lastRefreshTime >= REFRESH_INTERVAL) && isOn)
    {      
      lastRefreshTime += REFRESH_INTERVAL;
      readAnal();
    }
    
    if(!isOn){
      digitalWrite(LED_port, LOW);
    }    
    aux = digitalRead(btnPort);    
    
    if(aux == 0){
      pushed = false;     
      return;
    }
    
    if(pushed)
      return;
    pushed = true;
    isOn = !isOn;
 }
private:
  void readAnal(){
    potVal = analogRead(potPort);
    potVal = map(potVal, 0, 1023, 0, 255);
    potVal = constrain(potVal, 0, 255);   
    changeDim(potVal); 
  }

  void changeDim(int val){
    //range: 30 - 245
    //if < 30 then 30
    //if > 245 then 255
    val = ((int)(val / 10)) * 10;     
    val = val >= 30 && val <= 245 ? val :   
             val < 30 ? 30 : 255;
    potVal = val;
    analogWrite(LED_port, potVal);
  }
  bool isOn = false;
  int btnPort = 0;
  int potPort = 0;
  int LED_port = 0;
  int aux = 0;
  bool pushed = false;
  static const unsigned long REFRESH_INTERVAL = REFRESH_INTERVAL_MS; // ms
  unsigned long lastRefreshTime = 0;
  int potVal = 0;   
  bool aut = false;
};

ToogleLED gLed(KITCHEN_SWITCH_PORT, KITCHEN_RELAY_OUT_PORT);
ToogleDimmerLED bLed(ROOM_LED_PORT, ROOM_SWITCH_PORT, ROOM_POT_PORT);

void setup() {  
  Serial.begin(9600);
  comm.setup();
  gLed.setup();
  bLed.setup();
}

void loop() {
  comm.loop();
  Message m = comm.readMessage(); 
  gLed.loop(m);
  bLed.loop(m);
}

