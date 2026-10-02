/* Safety and motion controller: Arduino Nano + L298N.
   Camera protocol from ESP32: V,<B|F|L|R>,<confidence 0..100>\n */
#include <SoftwareSerial.h>

const uint8_t CAM_RX=2, TRIG=3, ECHO=4, ENA=5, ENB=6, IR_LEFT=7, IR_RIGHT=8;
const uint8_t IN1=9, IN2=10, IN3=11, IN4=12;
const uint8_t CRUISE=145, TURN=125, REVERSE_PWM=115;
const uint16_t STOP_CM=25, SLOW_CM=55;
const unsigned long CAMERA_TIMEOUT_MS=1200;
SoftwareSerial camera(CAM_RX, 13); // TX unused

enum Vision { BLOCKED, FORWARD, LEFT, RIGHT, INVALID };
Vision vision=INVALID;
uint8_t confidence=0;
unsigned long lastCamera=0, maneuverUntil=0;
bool backing=false;
char line[24]; uint8_t linePos=0;

void driveOne(uint8_t a, uint8_t b, uint8_t pwm, uint8_t en) {
  digitalWrite(a, pwm ? HIGH : LOW); digitalWrite(b, LOW); analogWrite(en,pwm);
}
void motors(int left, int right) {
  digitalWrite(IN1,left>0); digitalWrite(IN2,left<0); analogWrite(ENA,abs(left));
  digitalWrite(IN3,right>0); digitalWrite(IN4,right<0); analogWrite(ENB,abs(right));
}
void stopMotors(){ motors(0,0); }

uint16_t distanceCm(){
  digitalWrite(TRIG,LOW); delayMicroseconds(2); digitalWrite(TRIG,HIGH);
  delayMicroseconds(10); digitalWrite(TRIG,LOW);
  unsigned long us=pulseIn(ECHO,HIGH,25000UL);
  return us ? (uint16_t)(us/58UL) : 999;
}
void parseCamera(){
  while(camera.available()) {
    char c=camera.read();
    if(c=='\n') {
      line[linePos]=0; char d=0; int q=0;
      if(sscanf(line,"V,%c,%d",&d,&q)==2 && q>=0 && q<=100) {
        vision=d=='B'?BLOCKED:d=='F'?FORWARD:d=='L'?LEFT:d=='R'?RIGHT:INVALID;
        if(vision!=INVALID){confidence=q; lastCamera=millis();}
      }
      linePos=0;
    } else if(c!='\r' && linePos<sizeof(line)-1) line[linePos++]=c;
    else if(linePos>=sizeof(line)-1) linePos=0;
  }
}
void setup(){
  pinMode(TRIG,OUTPUT); pinMode(ECHO,INPUT); pinMode(IR_LEFT,INPUT_PULLUP); pinMode(IR_RIGHT,INPUT_PULLUP);
  for(uint8_t p=IN1;p<=IN4;p++) pinMode(p,OUTPUT); pinMode(ENA,OUTPUT); pinMode(ENB,OUTPUT);
  stopMotors();
  // 9600 is deliberately used: ATmega328P SoftwareSerial is unreliable at 115200.
  camera.begin(9600);
  Serial.begin(115200);
}
void loop(){
  parseCamera();
  static unsigned long lastSense=0; static uint16_t cm=999;
  if(millis()-lastSense>=60){cm=distanceCm(); lastSense=millis();}
  const bool irL=digitalRead(IR_LEFT)==LOW, irR=digitalRead(IR_RIGHT)==LOW;

  // Complete a short non-blocking escape maneuver, but proximity still overrides it.
  if(millis()<maneuverUntil){
    if(backing) motors(-REVERSE_PWM,-REVERSE_PWM);
    else motors(irL ? TURN : -TURN, irL ? -TURN : TURN);
    return;
  }
  if(backing){backing=false; maneuverUntil=millis()+300; return;}

  // Independent safety layer. A close object can never be overruled by ML.
  if(cm<=STOP_CM || (irL && irR)){
    stopMotors(); backing=true; maneuverUntil=millis()+250; return;
  }
  if(irL){motors(TURN,-TURN); return;}
  if(irR){motors(-TURN,TURN); return;}
  if(millis()-lastCamera>CAMERA_TIMEOUT_MS || confidence<70){stopMotors(); return;}

  int speed=(cm<SLOW_CM)?95:CRUISE;
  switch(vision){
    case FORWARD: motors(speed,speed); break;
    case LEFT: motors(70,speed); break;
    case RIGHT: motors(speed,70); break;
    case BLOCKED: motors(-TURN,TURN); break;
    default: stopMotors();
  }
}
