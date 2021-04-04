#include <Wire.h>
#include "NewSoftwareSerial.h"
#include <ArduinoJson.h>

#define pinKLineRX 2
#define pinKLineTX 3

#define ADR_Engine 0x01
#define ADR_Dashboard 0x17

//#define DEBUG 1


NewSoftwareSerial obd(pinKLineRX, pinKLineTX, false); // RX, TX, inverse logic
DynamicJsonDocument package(1024);

uint8_t currAddr = 0;
uint8_t blockCounter = 0;
uint8_t errorTimeout = 0;
uint8_t errorData = 0;
bool connected = false;
int sensorCounter = 0;

uint8_t currPage = 2;


int8_t coolant_temp = 0;
int engine_speed = 0;
float supply_voltage = 0;
uint8_t v_speed = 0;
uint8_t fuelLevel = 0;
float boost =0;
float maf_actuall = 0;
float maf=0;
float pedal_position=0;
float g_load = 0;
unsigned long odometer = 0;

void disconnect(){
  connected = false;
  currAddr = 0;
}

void obdWrite(uint8_t data){
  obd.write(data);
}

uint8_t obdRead(){
  unsigned long timeout = millis() + 1000;  
  while (!obd.available()){
    if (millis() >= timeout) {
      disconnect();      
      errorTimeout++;
      return 0;
    }
  }
  uint8_t data = obd.read();
  return data;
}

// 5Bd, 7O1
void send5baud(uint8_t data){
  // // 1 start bit, 7 data bits, 1 parity, 1 stop bit
  #define bitcount 10
  byte bits[bitcount];
  byte even=1;
  byte bit;
  for (int i=0; i < bitcount; i++){
    bit=0;
    if (i == 0)  bit = 0;
      else if (i == 8) bit = even; // computes parity bit
      else if (i == 9) bit = 1;
      else {
        bit = (byte) ((data & (1 << (i-1))) != 0);
        even = even ^ bit;
      }
//    if (i == 0) Serial.print(F(" startbit"));
//      else if (i == 8) Serial.print(F(" parity"));    
//      else if (i == 9) Serial.print(F(" stopbit"));                    
    bits[i]=bit;
  }
  // now send bit stream    
  for (int i=0; i < bitcount+1; i++){
    if (i != 0){
      // wait 200 ms (=5 baud), adjusted by latency correction
      delay(200);
      if (i == bitcount) break;
    }
    if (bits[i] == 1){ 
      // high
      digitalWrite(pinKLineTX, HIGH);
    } else {
      // low
      digitalWrite(pinKLineTX, LOW);
    }
  }
  obd.flush();
}


bool KWP5BaudInit(uint8_t addr){
  send5baud(addr);
  return true;
}


bool KWPSendBlock(char *s, int size){
//  for (int i=0; i < size; i++){    
//    uint8_t data = s[i];
////    Serial.print(data, HEX);
////    Serial.print(" ");    
//  }  
  for (int i=0; i < size; i++){
    uint8_t data = s[i];    
    obdWrite(data);
    /*uint8_t echo = obdRead();  
    if (data != echo){
      Serial.println(F("ERROR: invalid echo"));
      disconnect();
      errorData++;
      return false;
    }*/
    if (i < size-1){
      uint8_t complement = obdRead();        
      if (complement != (data ^ 0xFF)){
        disconnect();
        errorData++;
        return false;
      }
    }
  }
  blockCounter++;
  return true;
}

// count: if zero given, first received byte contains block length
// 4800, 9600 oder 10400 Baud, 8N1
bool KWPReceiveBlock(char s[], int maxsize, int &size){  
  bool ackeachbyte = false;
  uint8_t data = 0;
  int recvcount = 0;
  if (size == 0) ackeachbyte = true;
  if (size > maxsize) {
    return false;
  }  
  unsigned long timeout = millis() + 1000;  
  while ((recvcount == 0) || (recvcount != size)) {
    while (obd.available()){      
      data = obdRead();
      s[recvcount] = data;    
      recvcount++;      
      if ((size == 0) && (recvcount == 1)) {
        size = data + 1;
        if (size > maxsize) {
//          Serial.println("ERROR: invalid maxsize");
          return false;
        }  
      }
      if ((ackeachbyte) && (recvcount == 2)) {
        if (data != blockCounter){
//          Serial.println(F("ERROR: invalid blockCounter"));
          disconnect();
          errorData++;
          return false;
        }
      }
      if ( ((!ackeachbyte) && (recvcount == size)) ||  ((ackeachbyte) && (recvcount < size)) ){
        obdWrite(data ^ 0xFF);  // send complement ack        
        /*uint8_t echo = obdRead();        
        if (echo != (data ^ 0xFF)){
          Serial.print(F("ERROR: invalid echo "));
          Serial.println(echo, HEX);
          disconnect();
          errorData++;
          return false;
        }*/
      }
      timeout = millis() + 1000;        
    } 
    if (millis() >= timeout){
//      Serial.println(F("ERROR: timeout"));
      disconnect();
      errorTimeout++;
      return false;
    }
  }
//  // show data
//  Serial.print(F("IN: sz="));  
//  Serial.print(size);  
//  Serial.print(F(" data="));  
  for (int i=0; i < size; i++){
    uint8_t data = s[i]; 
  }  
  blockCounter++;
  return true;
}

bool KWPSendAckBlock(){
//  Serial.print(F("---KWPSendAckBlock blockCounter="));
//  Serial.println(blockCounter);  
  char buf[32];  
  sprintf(buf, "\x03%c\x09\x03", blockCounter);  
  return (KWPSendBlock(buf, 4));
}

bool connect(uint8_t addr, int baudrate){  
  blockCounter = 0;  
  currAddr = 0;
  obd.begin(baudrate);       
  KWP5BaudInit(addr);
  // answer: 0x55, 0x01, 0x8A          
  char s[3];
  int size = 3;
  if (!KWPReceiveBlock(s, 3, size)) return false;
  if (    (((uint8_t)s[0]) != 0x55) 
     ||   (((uint8_t)s[1]) != 0x01) 
     ||   (((uint8_t)s[2]) != 0x8A)   ){
//    Serial.println(F("ERROR: invalid magic"));
    disconnect();
    errorData++;
    return false;
  }
  currAddr = addr;
  connected = true;  
  if (!readConnectBlocks()) return false;
  return true;
}
  
bool readConnectBlocks(){  
  String info;  
  while (true){
    int size = 0;
    char s[64];
    if (!(KWPReceiveBlock(s, 64, size))) return false;
    if (size == 0) return false;
    if (s[2] == '\x09') break; 
    if (s[2] != '\xF6') {
//      Serial.println(F("ERROR: unexpected answer"));
      disconnect();
      errorData++;
      return false;
    }
    String text = String(s);
    info += text.substring(3, size-2);
    if (!KWPSendAckBlock()) return false;
  }
//  Serial.print("label=");
//  Serial.println(info);    
  return true;
}

bool readSensors(int group){
  char s[64];
  sprintf(s, "\x04%c\x29%c\x03", blockCounter, group);
  if (!KWPSendBlock(s, 5)) return false;
  int size = 0;
  KWPReceiveBlock(s, 64, size);
  if (s[2] != '\xe7') {
//    Serial.println(F("ERROR: invalid answer"));
    disconnect();
    errorData++;
    return false;
  }
  int count = (size-4) / 3;

  for (int idx=0; idx < count; idx++){
    byte k=s[3 + idx*3];
    byte a=s[3 + idx*3+1];
    byte b=s[3 + idx*3+2];
    String n;
    float v = 0;
    String t = "";
    String units = "";
    char buf[32];    
    switch (k){
      case 1:  v=0.2*a*b;             units=F("rpm"); break;
      case 2:  v=a*0.002*b;           units=F("%%"); break;
      case 3:  v=0.002*a*b;           units=F("Deg"); break;
      case 4:  v=abs(b-127)*0.01*a;   units=F("ATDC"); break;
      case 5:  v=a*(b-100)*0.1;       units=F("°C");break;
      case 6:  v=0.001*a*b;           units=F("V");break;
      case 7:  v=0.01*a*b;            units=F("km/h");break;
      case 8:  v=0.1*a*b;             units=F(" ");break;
      case 9:  v=(b-127)*0.02*a;      units=F("Deg");break;
      case 10: if (b == 0) t=F("COLD"); else t=F("WARM");break;
      case 11: v=0.0001*a*(b-128)+1;  units = F(" ");break;
      case 12: v=0.001*a*b;           units =F("Ohm");break;
      case 13: v=(b-127)*0.001*a;     units =F("mm");break;
      case 14: v=0.005*a*b;           units=F("bar");break;
      case 15: v=0.01*a*b;            units=F("ms");break;
      case 18: v=0.04*a*b;            units=F("mbar");break;
      case 19: v=a*b*0.01;            units=F("l");break;
      case 20: v=a*(b-128)/128;       units=F("%%");break;
      case 21: v=0.001*a*b;           units=F("V");break;
      case 22: v=0.001*a*b;           units=F("ms");break;
      case 23: v=b/256*a;             units=F("%%");break;
      case 24: v=0.001*a*b;           units=F("A");break;
      case 25: v=(b*1.421)+(a/182);   units=F("g/s");break;
      case 26: v=float(b-a);          units=F("C");break;
      case 27: v=abs(b-128)*0.01*a;   units=F("°");break;
      case 28: v=float(b-a);          units=F(" ");break;
      case 30: v=b/12*a;              units=F("Deg k/w");break;
      case 31: v=b/2560*a;            units=F("°C");break;
      case 33: v=100*b/a;             units=F("%%");break;
      case 34: v=(b-128)*0.01*a;      units=F("kW");break;
      case 35: v=0.01*a*b;            units=F("l/h");break;
      case 36: v=((unsigned long)a)*2560+((unsigned long)b)*10;  units=F("km");break;
      case 37: v=b; break; // oil pressure ?!
      // ADP: FIXME!
      /*case 37: switch(b){
             case 0: sprintf(buf, F("ADP OK (%d,%d)"), a,b); t=String(buf); break;
             case 1: sprintf(buf, F("ADP RUN (%d,%d)"), a,b); t=String(buf); break;
             case 0x10: sprintf(buf, F("ADP ERR (%d,%d)"), a,b); t=String(buf); break;
             default: sprintf(buf, F("ADP (%d,%d)"), a,b); t=String(buf); break;
          }*/
      case 38: v=(b-128)*0.001*a;        units=F("Deg k/w"); break;
      case 39: v=b/256*a;                units=F("mg/h"); break;
      case 40: v=b*0.1+(25.5*a)-400;     units=F("A"); break;
      case 41: v=b+a*255;                units=F("Ah"); break;
      case 42: v=b*0.1+(25.5*a)-400;     units=F("Kw"); break;
      case 43: v=b*0.1+(25.5*a);         units=F("V"); break;
      case 44: sprintf(buf, "%2d:%2d", a,b); t=String(buf); break;
      case 45: v=0.1*a*b/100;            units=F(" "); break;
      case 46: v=(a*b-3200)*0.0027;      units=F("Deg k/w"); break;
      case 47: v=(b-128)*a;              units=F("ms"); break;
      case 48: v=b+a*255;                units=F(" "); break;
      case 49: v=(b/4)*a*0.1;            units=F("mg/h"); break;
      case 50: v=(b-128)/(0.01*a);       units=F("mbar"); break;
      case 51: v=((b-128)/255)*a;        units=F("mg/h"); break;
      case 52: v=b*0.02*a-a;             units=F("Nm"); break;
      case 53: v=(b-128)*1.4222+0.006*a;  units=F("g/s"); break;
      case 54: v=a*256+b;                units=F("count"); break;
      case 55: v=a*b/200;                units=F("s"); break;
      case 56: v=a*256+b;                units=F("WSC"); break;
      case 57: v=a*256+b+65536;          units=F("WSC"); break;
      case 59: v=(a*256+b)/32768;        units=F("g/s"); break;
      case 60: v=(a*256+b)*0.01;         units=F("sec"); break;
      case 62: v=0.256*a*b;              units=F("S"); break;
      case 64: v=float(a+b);             units=F("Ohm"); break;
      case 65: v=0.01*a*(b-127);         units=F("mm"); break;
      case 66: v=(a*b)/511.12;          units=F("V"); break;
      case 67: v=(640*a)+b*2.5;         units=F("Deg"); break;
      case 68: v=(256*a+b)/7.365;       units=F("deg/s");break;
      case 69: v=(256*a +b)*0.3254;     units=F("Bar");break;
      case 70: v=(256*a +b)*0.192;      units=F("m/s^2");break;
      default: sprintf(buf, "%2x, %2x      ", a, b); break;
    }
    
    switch (currAddr){
      case ADR_Engine: 
        switch(group){
          case 1: 
            switch (idx){
              case 0: engine_speed = v; break;
              case 3: coolant_temp =v; break;
            }              
            break;
          case 3: 
            switch (idx){
              case 0: engine_speed = v; break;
              case 1: maf = v; break;
              case 2: maf = v; break;
            }              
            break;
          case 6: 
            switch (idx){
              case 0: v_speed = v; break;
            }              
            break;
          case 10: 
            switch (idx){
              case 0: maf=v; break;
              case 1: boost=v; break;
              case 2: boost =v; break;
              case 3: pedal_position=v; break;
            }              
            break;
          case 12: 
            switch (idx){
              case 2: supply_voltage=v; break;
              case 3: coolant_temp =v; break;
            }              
            break;
          case 16: 
            switch (idx){
              case 0: g_load=v; break;
            }              
            break;
        }
        break;
//      case ADR_Dashboard: 
//        switch (group){ 
//          case 1:  
//            switch (idx){
//              case 0: vehicleSpeed = v; break;
//              case 1: engineSpeed = v; break;
//              case 2: oilPressure = v; break;
//            }
//            break;
//          case 2:
//            switch (idx){
//              case 0: odometer = v; break;
//              case 1: fuelLevel = v; break;         
//            }
//            break;
//          case 50:
//            switch (idx){
//              case 1: engineSpeed = v; break;
//              case 2: oilTemp = v; break;
//              case 3: coolantTemp = v; break;
//            }
//            break;
//        }
//        break;
    }
    if (units.length() != 0){
      dtostrf(v,4, 2, buf); 
    }            
  }
  sensorCounter++;
  return true;
}

void updateData(){
  if (!connected){
    if ( (errorTimeout != 0) || (errorData != 0) ){
      package["errors"] = "Timeout: " + String(errorTimeout) + "ErrData: " + String(errorData);
    }
  } else {
    switch (currPage){
      case 1:      
        package["coolant_temp"] = String(coolant_temp);                          
        package["rpm"] = String(engine_speed);        
        package["vechile_speed"] = String(v_speed, 3);        
//        package["fuel_level"] = String(fuel_level);                               
        break;
      case 2:
        package["coolant"] = String(coolant_temp);                             
        package["rpm"] = String(engine_speed);                       
        package["battery"] = String(supply_voltage);  
        package["boost"] = String(boost);
        package["maf"] = String(maf);
        package["v_speed"] = String(int(v_speed));
        package["g_load"] = String(g_load);
        package["pedal_pos"] = String(pedal_position);                                      
        break;
    }    
  }
}

void setup(){         
  pinMode(pinKLineTX, OUTPUT);  
  digitalWrite(pinKLineTX, HIGH);  
  pinMode(13, OUTPUT);
  Serial.begin(9600);    
}


void loop(){  
  switch (currPage){
    case 1:      
      if (currAddr != ADR_Dashboard){        
        connect(ADR_Dashboard, 9600);
      } else  {
        readSensors(1);
        readSensors(2);
        readSensors(50);        
      }      
      break;
    case 2:
      if (currAddr != ADR_Engine) {
        connect(ADR_Engine, 9600);
      } else {
        readSensors(1); //coolant
        readSensors(3); //maf rpm
        readSensors(6); //km/h
        readSensors(10);// boost
        readSensors(12); //battery
        readSensors(16); //generator load
      }    
      break;   
  }
  updateData();
  serializeJson(package, Serial);    
  Serial.println();
  if (!connected){
    digitalWrite(13, LOW);
    digitalWrite(13, HIGH);
    delay(200);
    digitalWrite(13, LOW);
    delay(2000);
  }
  else
    digitalWrite(13, HIGH);            
}
