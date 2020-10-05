// Lazeriukas galva demo
// ka turiesi galuteni varianta supasakuosi patvarkyso ka nebutu gieda niekam ruodyt :D
// plius atmynti zinau kap irasyti.. tai yr da veikt kuo 

#include <VarSpeedServo.h>
#include <Arduino.h>


#include <Adafruit_GFX.h>  // Include core graphics library for the display
#include <Adafruit_SSD1306.h>  // Include Adafruit_SSD1306 library to drive the display

#define myubbr (16000000L/16/250000-1)

VarSpeedServo servo1; 
VarSpeedServo servo2;

#define OLED_MOSI   4
#define OLED_CLK   3
#define OLED_DC    6
#define OLED_CS    7
#define OLED_RESET 5
Adafruit_SSD1306 display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

struct rs485 {
  bool Available = true;
  uint32_t LastData = 0;
  uint8_t Data[100];
  uint16_t ChCount=0;
} RS485;
struct eprom {
  uint16_t DmxAddress;
  uint32_t LaserTime;
  uint32_t DeviceTime;
  uint8_t DeviceID[6]={0x12,0x34,0x56,0x78,0x91,0x23};
} Eprom;
struct dmx {
  bool Available = true;
  uint16_t *Address=&Eprom.DmxAddress;
  uint8_t Footprint = 4;
  uint8_t *Data=&RS485.Data[0];
} DMX;
struct rdm {
  bool Available = true;
  bool Mute = false;
  uint8_t Manufacture[]="eray";
  uint8_t *DeviceID=Eprom.DeviceID[0];
  uint8_t *Footprint=&DMX.Footprint;
} RDM;

ISR(USART_RX_vect)
{
  char temp, temp1;
  temp1 = UCSR0A;
  temp = UDR0 & 0xFF;
  if ((temp1 & (1 << FE0)) || temp1 & (1 << DOR0)){
    RS485.ChCount = 0;
    return;
  }
  if (RS485.ChCount < (char)50){
    RS485.Data[RS485.ChCount++] = temp & 0xFF;
    RDM.Available=false;
  } 
  RS485.LastData=millis();
}

void DMXRDM(){
  if(RDM.Available and RS485.Data[0]==0xCC and RS485.Data[1]==1){
    uint8_t ID=0;
    for(uint8_t i=0;i<6;i++){
      if(RS485.Data[i+3]==Eprom.DeviceID[i] or RS485.Data[i+3]==0xFF){
        ID++;
      }
    }
    if(ID==6){
      if(!RDM.Mute and RS485.Data[20]==16 and RS485.Data[21]==0 and RS485.Data[22]==1){ // Discovery
        uint16_t CheckSum=6*0xFF;
        for(byte i=0;i<7;i++)
          RS485.Data[i]=0xFE;
        RS485.Data[7]=0xAA;
        for(byte i=0;i<6;i++){
          RS485.Data[8+i+i]=Eprom.DeviceID[i] | 0xAA;
          RS485.Data[8+i+i+1]=Eprom.DeviceID[i] | 0x55;
          CheckSum+= Eprom.DeviceID[i];
        }
        RS485.Data[14]=(CheckSum >> 8) | 0xAA;
        RS485.Data[15]=(CheckSum >> 8) | 0x55;
        RS485.Data[16]=(CheckSum & 0xFF) | 0xAA;
        RS485.Data[17]=(CheckSum & 0xFF) | 0x55;
        RS485send(18,false);
        return;
      }
      if(RS485.Data[20]==16 and RS485.Data[21]==0 and RS485.Data[22]==3){ // unmute
        RDM.Mute=false;
        RS485.Data[24]=0;
        RS485.Data[25]=0;
        RS485send(26,true);
        return;
      }
      if(RS485.Data[20]==16 and RS485.Data[21]==0 and RS485.Data[22]==2){ // mute
        RDM.Mute=true;
        RS485.Data[24]=0;
        RS485.Data[25]=0;
        RS485send(26,true);
        return;
      }
      
      if(RS485.Data[20]==0x20 and RS485.Data[21]==0x00 and RS485.Data[22]==0x60){ // Get Device info
        RS485.Data[24]=1;
        RS485.Data[25]=0;
        RS485.Data[26]=0xcc;
        RS485.Data[27]=0xcc;
        RS485.Data[28]=0x01;
        RS485.Data[29]=0x00;
        RS485.Data[30]=0x12;
        RS485.Data[31]=0x12;
        RS485.Data[32]=0x12;
        RS485.Data[33]=0x12;
        RS485.Data[34]=0x00;
        RS485.Data[35]=DMX.Footprint;
        RS485.Data[36]=0x01;
        RS485.Data[37]=0x01;
        RS485.Data[38]=(uint8_t)DMX.Address >> 8;
        RS485.Data[39]=(uint8_t)DMX.Address & 0xFF;
        RS485.Data[40]=0x00;
        RS485.Data[41]=0x00;
        RS485.Data[42]=0x12;
        
        RS485send(43,true);
        return;
      }
      if(RS485.Data[20]==0x20 and RS485.Data[21]==0x00 and RS485.Data[22]==0x81){ // Get Manufacturer label
        RS485.Data[23]=sizeof(RDM.Manufacture);
        for(byte i=0;i<RS485.Data[23];i++){
          RS485.Data[24+i]=RDM.Manufacture[i];
        }
        
        RS485send(43,true);
        return;
      }
      if(RS485.Data[21]==0x10 and RS485.Data[22]==0x00){ // Identifay Device //////////////////////////
        if(RS485.Data[20]==0x30){
          RS485.Data[23]=0;
          RS485.Data[24]=0;
          
          RS485send(43,true);
          return;
        }
        if(RS485.Data[20]==0x20){
          RS485.Data[23]=1;
          RS485.Data[24]=0;
          
          RS485send(43,true);
          return;
        }
      }
      if(RS485.Data[20]==0x20 and RS485.Data[21]==0x00 and RS485.Data[22]==0x80){ // Get Model discription
        RS485.Data[23]=2;
        for(byte i=0;i<RS485.Data[23];i++){
          RS485.Data[24+i]=0xcc;
        }
        
        RS485send(43,true);
        return;
      }
      if(RS485.Data[21]==0x00 and RS485.Data[22]==0x82){ //  Device Lable ////////////////////////
        if(RS485.Data[20]==0x30){
          RS485.Data[23]=0;
          RS485.Data[24]=0;
          
          RS485send(43,true);
          return;
        }
        if(RS485.Data[20]==0x20){
          RS485.Data[23]=1;
          RS485.Data[24]=0;
          
          RS485send(43,true);
          return;
        }
      }
      if(RS485.Data[20]==0x20 and RS485.Data[21]==0x00 and RS485.Data[22]==0xC0){ // Get Sotware version
        RS485.Data[23]=4;
        for(byte i=0;i<RS485.Data[23];i++){
          RS485.Data[24+i]=0xcc;
        }
        
        RS485send(43,true);
        return;
      }
      if(RS485.Data[21]==0x00 and RS485.Data[22]==0xF0){ //  Start Address ////////////////////////
        if(RS485.Data[20]==0x30){
          RS485.Data[23]=0;
          RS485.Data[24]=0;
          
          RS485send(43,true);
          return;
        }
        if(RS485.Data[20]==0x20){
          RS485.Data[23]=2;
          RS485.Data[24]=0;
          
          RS485send(43,true);
          return;
        }
      }
      if(RS485.Data[21]==0x00 and RS485.Data[22]==0x50){ //  Supported parameters ////////////////////////
        if(RS485.Data[20]==0x30){
          RS485.Data[23]=0;
          RS485.Data[24]=0;
          
          RS485send(43,true);
          return;
        }
        if(RS485.Data[20]==0x20){
          RS485.Data[23]=6;
          RS485.Data[24]=0x00;
          RS485.Data[25]=0x81;
          RS485.Data[26]=0x00;
          RS485.Data[27]=0x80;
          RS485.Data[24]=0x00;
          RS485.Data[24]=0x82;
          
          RS485send(43,true);
          return;
        }
      }

      
    } // ID
    RDM.Available=false;
  }
}

void RS485send(uint8_t count,bool respond){
  UCSR0B|=((1<<TXEN0) & ~(1<<RXEN0) & ~(1<<RXCIE0));
  if(respond){
    RS485.Data[16]=0;
    RS485.Data[2]=RS485.Data[23]+24;
    for(byte i=0;i<6;i++){
      byte temp=RS485.Data[i+3];
      RS485.Data[i+3]=RS485.Data[i+9];
      RS485.Data[i+9]=temp;
    }
    RS485.Data[20]++;
    count=RS485.Data[2];
    
    uint16_t CheckSum=0;
    for(byte i=0;i<RS485.Data[2];i++){
      CheckSum+=RS485.Data[i];
    }
    RS485.Data[count]=uint8_t(CheckSum >> 8);
    RS485.Data[count+1]=uint8_t(CheckSum & 0xFF);
    count+=2;
  }
  
  for(int i=0;i<count;i++){
    UDR0=RS485.Data[i];
    UCSR0A|=1<<TXC0;
    loop_until_bit_is_set(UCSR0A, TXC0);
  }
  UCSR0B|=1<<RXEN0 | 1<<RXCIE0;
  UCSR0A&=~(1<<TXC0);
}

void draw(){
  for(int j=0;j<7;j++){
    for(int i=0;i<6;i++){
      display.setCursor(20*i,9*j);
      display.println(DMX.Data[j*6+i]);
      //u8g2.print(RS485.Data[j*6+i]);
    }
  }
}

void setup()
{
  pinMode(10, OUTPUT);
  pinMode(9, OUTPUT);
  digitalWrite(5, HIGH);
  digitalWrite(8, HIGH);
  servo1.attach(9); //analog pin 0
  servo2.attach(10); //analog pin 1
 
  
display.begin(SSD1306_SWITCHCAPVCC);  // Initialize display
display.clearDisplay();  // Clear the buffer
display.setTextColor(WHITE);  // Set color of the text
display.setRotation(0);  // Set orientation. Goes from 0, 1, 2 or 3
display.setTextWrap(false);  // By default, long lines of text are set to automatically “wrap” back to the leftmost column.
                               // To override this behavior (so text will run off the right side of the display - useful for
                               // scrolling marquee effects), use setTextWrap(false). The normal wrapping behavior is restored
                               // with setTextWrap(true).
display.dim(0);  //Set brightness (0 is maximun and 1 is a little dim)
display.setTextSize(0);

   delay(100);
  UBRR0H = (unsigned char)(myubbr >> 8);
  UBRR0L = (unsigned char)myubbr;
  UCSR0B |= ((1 << RXEN0) | (1 << RXCIE0)); //Enable Receiver and Interrupt RX
  UCSR0C |= (3 << UCSZ00); //N81

}
void loop()
{
  servo1.write(map(DMX.Data[1], 0, 255, 0, 180),255,false); //9 pians servas    (pozicija,greitis,laukimas)
  servo2.write(map(DMX.Data[2], 0, 255, 0, 180),255,false); //10 pinas servas
  DMXRDM();
  display.clearDisplay();
  draw();
  display.display();
  if(millis()-RS485.LastData>5){
    RDM.Available=true;
  }

}

