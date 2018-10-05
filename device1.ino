#include <SoftwareSerial.h>
#include <IRremote.h>
#include <LiquidCrystal.h>

#define DEBUG_ false

enum Commands {
  ActivateAlarm = 1,
  DeactivateAlarm = 2,
  Panic = 3, //From DV
  StopPanic = 4, //From DV

  OpenDoor = 5,
  CloseDoor = 6,
  GetDoorState = 7,
  DoorOpened = 8, //From DV
  ClosingDoor = 9, //From DV
  OpeningDoor = 10,
  DoorClosed = 11, //From DV

  OnGardenLight = 12,
  OffGardenLight = 13,
  GetGardenLightState = 14,
  GardenLightOn = 15, //From DV
  GardenLightOff = 16, //From DV

  OnKitchenLight = 17,
  OffKitchenLight = 18,
  GetKitchenLightStatus = 19,
  KitchenLightOn = 20, //From DV
  KitchenLightOff = 21, //From DV

  OnRoomLight = 22,
  OffRoomLight = 23,
  LightenRoom = 24,
  DarkenRoom = 25,
  GetRoomLightStatus = 26,
  RoomLightOn = 27, //From DV
  RoomLightOff = 28, //From DV
  RoomLightDim = 29, //From DV


  GetFireAlarmStatus = 30,
  
  UndefinedGarageDoor = 999,
};

struct Message {
  Message() {}
  Message(byte c) {
    Command = c;
  }
  Message(byte c, byte v) {
    Command = c;
    Value = v;
  }
  byte Command = 0;
  byte Value = 0;
};

struct Comm {
    void setup() {
      zigBee.begin(9600);
      Serial.println("COMM SETUP DONE. DEVICE 3");
    }

    void sendCommand(Message m) {
      zigBee.write((byte)0x7E);//Start trans
      zigBee.write((byte)0x00);//length *always 0
      zigBee.write((byte)0x10);//lenght
      zigBee.write((byte)0x10);//frame type
      zigBee.write((byte)0x01);//frame id

      //64bits address. Use 00 00 00 00 00 00 FF FF for broadcasting
      zigBee.write((byte)0x00);
      zigBee.write((byte)0x00);
      zigBee.write((byte)0x00);
      zigBee.write((byte)0x00);
      zigBee.write((byte)0x00);
      zigBee.write((byte)0x00);
      zigBee.write((byte)0xFF);
      zigBee.write((byte)0xFF);
      //

      //16bits address. Use default: 0xFF 0xFE if unknown or broadcasting
      zigBee.write((byte)0xFF);
      zigBee.write((byte)0xFE);
      //

      zigBee.write((byte)0x00); //Broadcast radio. Use 0 for max value
      zigBee.write((byte)0x01); //Options; 01 means: disable acknowledge
      zigBee.write(m.Command); //Message
      zigBee.write(m.Value); //Message

      long chexsum = 0x10 + 0x01 + 0xFF + 0xFF + 0xFF + 0xFE + 0x01 + m.Command + m.Value;
      byte cTotal = (byte)(0xFF - (chexsum & 0xFF));
      zigBee.write(cTotal); // Checksum
    }

    Message readMessage() {
      Message n = Message(message.Command, message.Value);
      message = Message();
      return n;
    }

    void loop() {
      readTrans();
    }

  private:
    void fillMessage(bool withEnd){   
      if(!newCode) 
        return;
      
      if(fields[3] == 144) { //only receive 0x90 (receive package) frames
        message.Command = auxCmd;
        message.Value = auxVal;                
      }
      newCode = false;
      counter = 0;
      barry = 0;
      auxCmd = 0;
      auxVal = 0;
    }
    
    void readTrans() {
      bs = zigBee.available();      
      if (bs <= 0) {
        fillMessage(true);
        return;
      }
      
      for (int i = 0; i < bs; i++) {
        barry = zigBee.read();
        if(barry == 126 /*New message, first header*/){
          int auxBarry = barry;
          fillMessage(false);
          barry = auxBarry;
          newCode = true;      
        }
        if(!newCode){
          return;
        }
                
        if (counter == 15) {//Command field
          auxCmd = barry;

        } else if (counter == 16) { //Value field         
          auxVal = barry;
        }else if(counter == 17){//Checksum field
          fields[17] = barry;
        }
        else{//Common packages fields
          fields[counter] = barry;
        }
        counter++;
      }      
      delay(250);  
    }


  private:
    int bs = 0;
    int newCode = false;

    int counter = 0;
    SoftwareSerial zigBee = SoftwareSerial(2, 3);
    byte barry;
    byte fields[16];

    Message message = Message();
    byte auxCmd = 0;
    byte auxVal = 0;
};

Comm comm = Comm();

//==================================================================

#define TO_IDLE 15 //This will be SLIDE_WAIT*TO_IDLE (miliseconds)
#define SLIDE_WAIT 2000
//=================================================================

enum DisplayStates {  
  IDLE_,
  MAINMENU,
  FIRE_ALARM,
  GARAGE,
  LIGHTS,
  BEDROOM,
  GARDEN,
  KITCHEN,
  HELP,
  PANIC,
  NOT_IMPLEMENTED
};

enum MenuEventType {
  UNKNOWN_KEY,
  NONE,
  KEY1,
  KEY2,
  KEY3,
  KEY4,
  UP,
  DOWN,
  REPEATED
};

enum ViewModelEntities {
  NotView,
  FireAlarm,
  GarageDoor,
  GardenLight,
  KitchenLight,
  BedroomLight,
};

struct ViewModel {
    void loop(Message m, unsigned long now) {
      updateModel(m);
      
      if (CurrentModel == ViewModelEntities::NotView)
        return;
      
      if (!resetTimer && now - prev < 5000)
          return;
      resetTimer  = false;        
      
      prev = now;
      switch (CurrentModel) {
        case ViewModelEntities::FireAlarm:
          comm.sendCommand(Message(Commands::GetFireAlarmStatus));
          break;
        case ViewModelEntities::GarageDoor:
          comm.sendCommand(Message(Commands::GetDoorState));
          break;
        case ViewModelEntities::GardenLight:
          comm.sendCommand(Message(Commands::GetGardenLightState));
          break;
        case ViewModelEntities::KitchenLight: 
            comm.sendCommand(Message(Commands::GetKitchenLightStatus));
            break;
        case ViewModelEntities::BedroomLight: 
            comm.sendCommand(Message(Commands::GetRoomLightStatus));
            break;          
      }
    }

    void reset() {
      GarageDoorState = Commands::UndefinedGarageDoor;
      resetTimer = true;
      GardenLightOnState = false;
      KitchenLightOnState = false;
      RoomLightOnState = false;
      PanicState = false;
      StopPanicState = false;
    }

    void updateModel(Message m){      
      switch (m.Command) {
        case 0:
          break;
        case Commands::Panic:{          
          PanicState = true;
          break;
        }
        case Commands::StopPanic:{
          StopPanicState = true;
          break;
        }
        case Commands::DoorOpened:
        case Commands::ClosingDoor:
        case Commands::DoorClosed:
        case Commands::OpeningDoor:
          GarageDoorState = m.Command;
          break;
        case Commands::GardenLightOn:
          this->GardenLightOnState = true;
          break;
        case Commands::GardenLightOff:
          this->GardenLightOnState = false;
          break;
        case KitchenLightOn:
          this->KitchenLightOnState = true;
          break;
        case KitchenLightOff:
          this->KitchenLightOnState = false;
          break;
        case RoomLightOn:
          this->RoomLightOnState = true;
          break;
        case RoomLightOff:
          this->RoomLightOnState  = false;
          break;
        case Commands::RoomLightDim:
          this->RoomLightDimState  = m.Value;
          break;
      }
    }
    
    bool GardenLightOnState = false;
    ViewModelEntities CurrentModel = ViewModelEntities::NotView;
    Commands GarageDoorState = Commands::UndefinedGarageDoor;
    bool KitchenLightOnState = false;
    bool RoomLightOnState = false;
    byte RoomLightDimState = 0;
    bool PanicState = false;
    bool StopPanicState = false;
    
  private:
    unsigned long prev = 0;
    bool resetTimer = true;
};

struct Display {
    void setup() {
      lcd.begin(16, 2);
      irrecv.enableIRIn();
      Serial.println("DISPLAY SCREEN SETUP DONE. DEVICE 1. V1");
    }
    void loop(Message m) {
      now = millis();
      model.loop(m, now);

      if(model.PanicState && model.StopPanicState){
        state = FIRE_ALARM;
        model.CurrentModel = ViewModelEntities::FireAlarm;
        model.reset();
        reset();
      }
      else if(model.PanicState){
        state = PANIC;
        model.CurrentModel = ViewModelEntities::FireAlarm;
      }
      

      MenuEventType e;
      if (irrecv.decode(&results)) {
        e = translateIR();
        irrecv.resume();
      }
      
      switch (state) {
        case IDLE_:
          idle(e);
          break;
        case MAINMENU:
          mainMenu(e);
          break;
        case FIRE_ALARM:
          fireAlarm(e);
          break;
        case GARAGE:
          garage(e);
          break;
        case LIGHTS:
          lights(e);
          break;
        case BEDROOM:
          bedroom(e);
          break;
        case GARDEN:
          garden(e);
          break;
        case KITCHEN:
          kitchen(e);
          break;
        case HELP:
          help(e);
          break;
        case PANIC:
          panic(e);
          break;
        default:
          none(e);
      }

    }
  private:
    void reset(){
      mainMenuScrollIdx = 0;
      firstMenuScroll = true;
      lcd.clear();
      countToIdle = 0;
    }
    
    void help(MenuEventType e) {
      show("DAWLIN P 1720340");
      show("UoW 2018...", 1);
      delay(050);
      if (now - prev >= 4000) {        
        prev = now;
        state = DisplayStates::MAINMENU;
        lcd.clear();
      }
      if (e <= MenuEventType::NONE || e == MenuEventType::REPEATED)
        return;
      
      state = DisplayStates::MAINMENU;
      reset();
    }

    void panic(MenuEventType e) {
      show("FIRE!!! CALL 999    ");
      show("1 - DEACTIVATE ALARM", 1);

      if (e <= MenuEventType::NONE || e <= MenuEventType::UNKNOWN_KEY)
        return;

      switch (e) {
        case MenuEventType::KEY1: {          
          comm.sendCommand(Message(Commands::DeactivateAlarm));
          model.CurrentModel = ViewModelEntities::FireAlarm;
          state = FIRE_ALARM;          
          model.reset();
          break;
        }
      }
      reset();
    }

    void none(MenuEventType e) {
      show("View not implemented");
      show("BACK TO MAIN MENU", 1);

      if (now - prev >= 2000) {
        prev = now;
        state = DisplayStates::MAINMENU;
        reset();
      }
      if (e <= MenuEventType::NONE)
        return;
      state = DisplayStates::MAINMENU;
      reset();
    }

    void idleActive() {
      state = DisplayStates::IDLE_;
      mainMenuScrollIdx = 0;
      firstMenuScroll = true;
      lcd.clear();
      countToIdle = 0;
      model.CurrentModel = ViewModelEntities::NotView;
      model.reset();
    }

    void idle(MenuEventType e) {
      show("HELLO, PRESS ANY", 0);
      show("KEY TO START...", 1);

      if (e > MenuEventType::NONE) {
        state = DisplayStates::MAINMENU;
        model.CurrentModel = ViewModelEntities::NotView;
        model.reset();
        reset();        
      }
    }

    void fireAlarm(MenuEventType e) {
      show("ALARM - INACTIVE", 0);
      switch (mainMenuScrollIdx) {
        case 0:
          show("1 - ACTIVATE", 1);
          break;
        case 1:
          show("2 - DEACTIVATE", 1);
          break;
        case 2:
          show("3 - BACK", 1);
          break;
        default:
          show("Something goes wrong", 1);
          break;
      }

      if (now - prev >= SLIDE_WAIT) {
        prev = now;
        if (countToIdle >= TO_IDLE) {
          idleActive();
        }
        countToIdle++;
        if (firstMenuScroll) {
          firstMenuScroll = false;
        } else {
          lcd.clear();
          mainMenuScrollIdx = mainMenuScrollIdx + 1 > 2 ? 0 : mainMenuScrollIdx + 1;
        }
      }

      if (e <= MenuEventType::NONE || e <= MenuEventType::UNKNOWN_KEY)
        return;

      switch (e) {
        case MenuEventType::KEY1: {
            comm.sendCommand(Message(Commands::ActivateAlarm));
            state = DisplayStates::PANIC;
            break;
          }
        case MenuEventType::KEY2:
          comm.sendCommand(Message(Commands::DeactivateAlarm));
          break;
        case MenuEventType::KEY3:{
          state = DisplayStates::MAINMENU;
          model.CurrentModel = ViewModelEntities::NotView;
          model.reset();
          break;
        }
      }
      reset();
    }

    void garage(MenuEventType e) {
      switch (model.GarageDoorState) {
        case Commands::DoorOpened:          
          show("GARAGE - OPEN");
          break;
        case Commands::ClosingDoor:          
          show("GARAGE - CLOSING");
          break;
        case Commands::DoorClosed:
          show("GARAGE - CLOSED");
          break;
        case Commands::OpeningDoor:
          show("GARAGE - OPENING");
          break;
        case UndefinedGarageDoor:
          show("GARAGE - ...");          
          break;
        default: {
            Serial.println("ERROR: UNKNOW DOOR STATE");            
            show("GARAGE - ###");         
          }
      }
      
      switch (mainMenuScrollIdx) {
        case 0:
          show("1 - OPEN", 1);
          break;
        case 1:
          show("2 - CLOSE", 1);
          break;
        case 2:
          show("3 - BACK", 1);
          break;
        default:
          show("Something goes wrong", 1);
          break;
      }

      if (now - prev >= SLIDE_WAIT) {
        prev = now;
        if (countToIdle >= TO_IDLE) {
          idleActive();
        }
        countToIdle++;
        if (firstMenuScroll) {
          firstMenuScroll = false;
        } else {
          lcd.clear();
          mainMenuScrollIdx = mainMenuScrollIdx + 1 > 2 ? 0 : mainMenuScrollIdx + 1;
        }
      }

      if (e <= MenuEventType::NONE || e <= MenuEventType::UNKNOWN_KEY)
        return;

      switch (e) {
        case MenuEventType::KEY1:
          comm.sendCommand(Message(Commands::OpenDoor));
          break;
        case MenuEventType::KEY2:
          comm.sendCommand(Message(Commands::CloseDoor));
          break;
        case MenuEventType::KEY3: {
          model.CurrentModel = ViewModelEntities::NotView;
          model.reset();
          state = DisplayStates::MAINMENU;
          break;
        }
      }
      reset();
    }

    void lights(MenuEventType e) {      
      int a = 0;
      for (int i = 0; i < 2; i++) {
        a = mainMenuScrollIdx + i < 4 ? mainMenuScrollIdx + i : 0;
        show(lightMenues[a], i);
      }

      if (now - prev >= SLIDE_WAIT) {
        prev = now;
        if (countToIdle >= TO_IDLE) {
          idleActive();
        }
        countToIdle++;
        if (firstMenuScroll) {
          firstMenuScroll = false;
        } else {
          lcd.clear();
          mainMenuScrollIdx = mainMenuScrollIdx + 1 > 3 ? 0 : mainMenuScrollIdx + 1;
        }
      }

      if (e <= MenuEventType::NONE || e <= MenuEventType::UNKNOWN_KEY)
        return;

      switch (e) {
        case MenuEventType::KEY1: {
            state = DisplayStates::BEDROOM;
            model.CurrentModel = ViewModelEntities::BedroomLight;
            model.reset();
          }
          break;
        case MenuEventType::KEY2: {
            state = DisplayStates::GARDEN;
            model.CurrentModel = ViewModelEntities::GardenLight;
            model.reset();
            break;
          }
        case MenuEventType::KEY3: {
            state = DisplayStates::KITCHEN;
            model.CurrentModel = ViewModelEntities::KitchenLight;
            model.reset();
          }
          break;
        case MenuEventType::KEY4: {
            state = DisplayStates::MAINMENU;
            model.CurrentModel = ViewModelEntities::NotView;
            model.reset();
            break;
          }
        default:
          return;
      }
      reset();
    }

    void bedroom(MenuEventType e) {
      char buf[16];

      if (model.RoomLightOnState) {
        strcpy(buf, "BEDROOM-OFF");
      } else {
        strcpy(buf, "BEDROOM-ON");
      }
      sprintf(buf, "%s-%d", buf, model.RoomLightDimState);
      show(buf);

      switch (mainMenuScrollIdx) {
        case 0:
          show("1 - SWITCH ON", 1);
          break;
        case 1:
          show("2 - SWITCH OFF", 1);
          break;
        case 2:
          show("(^) - LIGHTEN", 1);
          break;
        case 3:
          show("(v) - DARKEN", 1);
          break;
        case 4:
          show("3 - BACK", 1);
          break;
        default:
          show("Something goes wrong", 1);
          break;
      }

      if (now - prev >= SLIDE_WAIT) {
        prev = now;
        if (countToIdle >= TO_IDLE) {
          idleActive();
        }
        countToIdle++;
        if (firstMenuScroll) {
          firstMenuScroll = false;
        } else {
          lcd.clear();
          mainMenuScrollIdx = mainMenuScrollIdx + 1 > 4 ? 0 : mainMenuScrollIdx + 1;
        }
      }

      if (e <= MenuEventType::NONE || e <= MenuEventType::UNKNOWN_KEY)
        return;

      switch (e) {
        case MenuEventType::KEY1:
          comm.sendCommand(Message(Commands::OnRoomLight));
          break;
        case MenuEventType::KEY2:
          comm.sendCommand(Message(Commands::OffRoomLight));
          break;
        case MenuEventType::UP:
          comm.sendCommand(Message(Commands::LightenRoom));
          break;
        case MenuEventType::DOWN:
          comm.sendCommand(Message(Commands::DarkenRoom));
          break;
        case MenuEventType::KEY3: {
            state = DisplayStates::LIGHTS;
            model.CurrentModel = ViewModelEntities::NotView;
            model.reset();
            break;
          }
        default:
          return;
      }
      reset();
    }

    void garden(MenuEventType e) {
      if (model.GardenLightOnState)
        show("GARDEN - ON");        
      else
        show("GARDEN - OFF");    
      
      switch (mainMenuScrollIdx) {
        case 0:
          show("1 - SWITCH ON", 1);
          break;
        case 1:
          show("2 - SWITCH OFF", 1);
          break;
        case 2:
          show("3 - BACK", 1);
          break;
        default:
          show("Something goes wrong", 1);
          break;
      }

      if (now - prev >= SLIDE_WAIT) {
        prev = now;
        if (countToIdle >= TO_IDLE) {
          idleActive();
        }
        countToIdle++;
        if (firstMenuScroll) {
          firstMenuScroll = false;
        } else {
          lcd.clear();
          mainMenuScrollIdx = mainMenuScrollIdx + 1 > 2 ? 0 : mainMenuScrollIdx + 1;
        }
      }

      if (e <= MenuEventType::NONE || e <= MenuEventType::UNKNOWN_KEY)
        return;

      switch (e) {
        case MenuEventType::KEY1:
          comm.sendCommand(Message(Commands::OnGardenLight));
          break;
        case MenuEventType::KEY2:
          comm.sendCommand(Message(Commands::OffGardenLight));
          break;
        case MenuEventType::KEY3: {
            state = DisplayStates::LIGHTS;
            model.CurrentModel = ViewModelEntities::NotView;
            model.reset();
            break;
          }
        default:
          return;
      }
      reset();
    }

    void kitchen(MenuEventType e) {            
      if (model.KitchenLightOnState)
        show("KITCHEN - ON");        
      else
        show("KITCHEN - OFF");
        
      switch (mainMenuScrollIdx) {
        case 0:
          show("1 - SWITCH ON", 1);
          break;
        case 1:
          show("2 - SWITCH OFF", 1);
          break;
        case 2:
          show("3 - BACK", 1);
          break;
        default:
          show("Something goes wrong", 1);
          break;
      }

      if (now - prev >= SLIDE_WAIT) {
        prev = now;
        if (countToIdle >= TO_IDLE) {
          idleActive();
        }
        countToIdle++;
        if (firstMenuScroll) {
          firstMenuScroll = false;
        } else {
          lcd.clear();
          mainMenuScrollIdx = mainMenuScrollIdx + 1 > 2 ? 0 : mainMenuScrollIdx + 1;
        }
      }

      if (e <= MenuEventType::NONE || e <= MenuEventType::UNKNOWN_KEY)
        return;

      switch (e) {
        case MenuEventType::KEY1: {
            comm.sendCommand(Message(Commands::OnKitchenLight ));
            break;
          }
        case MenuEventType::KEY2: {
            comm.sendCommand(Message(Commands::OffKitchenLight));            
            break;
          }
        case MenuEventType::KEY3: {
            state = DisplayStates::LIGHTS;
            model.CurrentModel = ViewModelEntities::NotView;
            model.reset();
            break;
          }
        default:
          return;
      }
      reset();
    }

    void mainMenu(MenuEventType e) {
      int a = 0;

      for (int i = 0; i < 2; i++) {
        a = mainMenuScrollIdx + i < 4 ? mainMenuScrollIdx + i : 0;
        show(menues[a], i);
      }

      if (now - prev >= SLIDE_WAIT) {
        prev = now;
        if (countToIdle >= TO_IDLE) {
          idleActive();
        }
        countToIdle++;
        if (firstMenuScroll) {
          firstMenuScroll = false;
        } else {
          lcd.clear();
          mainMenuScrollIdx = mainMenuScrollIdx + 1 > 3 ? 0 : mainMenuScrollIdx + 1;
        }
      }

      if (e <= MenuEventType::NONE || e <= MenuEventType::UNKNOWN_KEY)
        return;

      switch (e) {
        case MenuEventType::KEY1: {
            state = DisplayStates::FIRE_ALARM;
            model.CurrentModel = ViewModelEntities::FireAlarm;
            model.reset();
            break;
          }
        case MenuEventType::KEY2: {
            state = DisplayStates::GARAGE;
            model.CurrentModel = ViewModelEntities::GarageDoor;
            model.reset();
            break;
          }
        case MenuEventType::KEY3: {
            state = DisplayStates::LIGHTS;
            break;
          }
        case MenuEventType::KEY4:
          state = DisplayStates::HELP;
          break;
        default:
          return;
      }
      reset();
    }

    void show(char* message, int line = 0) {
      lcd.setCursor(0, line);
      lcd.print(message);
    }

    MenuEventType translateIR() {
      MenuEventType type = MenuEventType::UNKNOWN_KEY;
      switch (results.value) {
        case 0x9716BE3F:
        case 0xFF30CF:
          type = MenuEventType::KEY1;
          break;

        case 0x3D9AE3F7:
        case 0xFF18E:
        case 0xFF18E7:
          type = MenuEventType::KEY2;
          break;

        case 0x6182021B:
        case 0xFF7A85:
          type = MenuEventType::KEY3;
          break;

        case 0x8C22657B:
        case 0xFF10EF:
          type = MenuEventType::KEY4;
          break;

        case 0xE5CFBD7F: //20
        case 0xFF906F: //21
          type = MenuEventType::UP;
        break;

        case 0xFFE01F: //31
        case 0xF076C13B: //21
          type = MenuEventType::DOWN;
        break;
        
        case 0xFFFFFFFF:
        case 0xFF:
          MenuEventType::REPEATED;
          break;
          
        default:{
          type = MenuEventType::UNKNOWN_KEY;
        }
      }
      return type;
    }

  private:  
    LiquidCrystal lcd = LiquidCrystal(12, 13, 4, 5, 6, 7);
    byte state = DisplayStates::IDLE_;
    IRrecv irrecv = IRrecv(8);
    decode_results results;

    int mainMenuScrollIdx = 0;
    bool firstMenuScroll = true;
    byte countToIdle = 0;
    unsigned long prev = 0;
    unsigned long now = 0;

    const char *menues[4] = {"1 - FIRE ALARM", "2 - GARAGE DOOR", "3 - LIGHTS", "4 - HELP"};
    const char *lightMenues[4] = {"1 - BEDROOM", "2 - GARDEN", "3 - KITCHEN", "4 - BACK"};
    ViewModel model = ViewModel();
};


Display d;

void setup() {
  Serial.begin(9600);
  comm.setup();
  d.setup();
}

void loop() {
  comm.loop();
  Message m = comm.readMessage();
  d.loop(m);
}
