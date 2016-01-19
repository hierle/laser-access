// 
// Name:      laser_access.ino
// Version:   0.1.4-github
// Date:      20160119
// Author:    A.Hierle <github@hierle.com>
// Purpose:   fablab laser access control
// Platform:  arduino mega2560


#include "laser_access.h"

#include <Wire.h>   // RTC and RTC-eeprom
#include <EEPROM.h> // internal eeprom
#include <SPI.h>    // SPI for TFT & SD card
#include <SD.h>     // SD card
#include <Adafruit_STMPE610.h>
#include <Adafruit_NFCShield_I2C.h>
#include <Adafruit_GFX_AS.h>
#include <Adafruit_ILI9341_AS.h>

#include <avr/pgmspace.h> // PROGMEM

time_t now;
time_t then;
//int wifi_status=0;
int enable=0;  // privileged enable mode: default off
time_t enable_start; // start enable mode
time_t power_start; // start ready mode
time_t dim_start; // start light/dim
time_t sense_last;

int counter=0;
int mode = 0;
int dim = 0;
unsigned long key_start=0;
uint8_t nfcid[]  = { 0, 0, 0, 0, 0, 0, 0 };
uint8_t lastid[] = { 0, 0, 0, 0 };
//unsigned long nfctag;
user_t user;
char command[64];
int sense[] = { -1, -1, -1, -1, -1, -1 };

uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; // NFC card default key
uint8_t keyf[6] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 }; // NFC card custom key, CHANGE!!!


// TFT
Adafruit_ILI9341_AS tft = Adafruit_ILI9341_AS(TFT_CS, TFT_DC, -1);
// Touch
Adafruit_STMPE610 touch = Adafruit_STMPE610();
// NFC
Adafruit_NFCShield_I2C nfc(NFC_IRQ, NFC_RESET);


//
// setup, running once, after reset
//
void setup() {
  
  // set default mode: power off
  mode = MODE_OFF;

  // power relais : LOW => ON , HIGH => OFF
  pinMode(PIN_POWER, OUTPUT);
  digitalWrite(PIN_POWER,HIGH);

  // esp reset line
  pinMode(PIN_ESP_RESET, OUTPUT);

  // init sense pins as input
  pinMode(SENSE_0, INPUT);
  pinMode(SENSE_1, INPUT);
  pinMode(SENSE_2, INPUT);
  pinMode(SENSE_3, INPUT);
  pinMode(SENSE_4, INPUT);

  // init I2C
  Wire.begin();

  write_log("booting");

  // init serial PC
  //Serial.begin(9600);
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("booting ..."));
  
  Serial.print(F("init ESP8266..."));
  // init serial to wifi module
  //Serial1.begin(9600);
  Serial1.begin(115200);
  // 500?1000
  //Serial1.setTimeout(1000);
  // reset
  reset_wifi();
  
  Serial.println(F("ok"));

  // get time from DS3232 RTC
  get_time(&now);

  // dim off tft backlight
  analogWrite(PIN_TFT,200);
  get_time(&dim_start);
  dim=1;

  // start tft, blank black
  //tft.begin();
  tft.init();
  //tft.fillScreen(ILI9341_BLACK);
  tft.fillScreen(ILI9341_WHITE);

  if (! touch.begin(I2C_TOUCH_ADDR)) {
    Serial.println(F("touch controller not found!"));
    while(1);
  }
  //Serial.println(F("Waiting for touch sense"));


  // NFC
  Serial.print(F("init NFC controller..."));
  nfc.begin();
  //nfc.setPassiveActivationRetries(0x01);
  nfc.setPassiveActivationRetries(0x05);
  nfc.SAMConfig();
  Serial.println(F("ok"));

  // default: power OFF
  power_on(0);

  // init finished
  Serial.println(F("boot done!"));


  //
  // ONLY ONCE: set root pw ... has changed anyway :)
  //write_enable_pw("itchyiktestus");
  //
  // ONLY ONCE: set date/time
  //time_t n={22,10,0,2016,1,19};
  //set_time(n);

}
// end of setup



//
// the loop over and over
//
void loop() {


  int i;
  int bytesrec=0;
  int msglen=63;
  char usbmsg[msglen];
  char wifimsg[msglen];
  char buffer[16];
  uint16_t x, y;
  uint8_t z;
  
  memset(usbmsg,0,msglen); 
  memset(wifimsg,0,msglen); 

  // get time from rtc
  get_time(&now);
  // TODO: check for dead rtc ... too many things depend on it

  // update time display, once per minute
  if(now.minute!=then.minute) {
    //get_time(&then);
    memcpy((void *)&then,(void *)&now,sizeof(now));
    print_tft_time();
  }

  // update senses every 5 seconds
  if(mode==MODE_ON) {
    if (time_diff(now,sense_last)>5) {
      print_tft_sense();
      get_time(&sense_last);
    }
  }

  // enable mode expired?
  //if (time_diff(now,enable_start)>ENABLE_TIMEOUT) { enable=0; Serial.println("enable timeout reached ... disabling privileged mode."); }
  if(enable==1) {
    if (time_diff(now,enable_start)>ENABLE_TIMEOUT) {
      enable=0; 
      Serial.println(F("enable timeout reached ... disabling privileged mode."));
      write_log("enable off (timeout)");
    }
  }

  // dim timeout  
  if(dim==1) {
    if (time_diff(now,dim_start)>DIM_TIMEOUT) {
      analogWrite(PIN_TFT,10);
      dim=0;
    }
  }
  
  // touched
  if (touch.touched()) {
    analogWrite(PIN_TFT,200);
    get_time(&dim_start);
    dim=1;
    /**
    // read x & y & z;
    while (! touch.bufferEmpty()) {
      touch.readData(&x, &y, &z);
    }
    **/
    touch.writeRegister8(STMPE_INT_STA, 0xFF); // reset all ints
    delay(10);
  }

  if(mode==MODE_ON) {
    if (time_diff(now,power_start)>CARD_TIMEOUT) {
      mode=MODE_CARD;
      print_tft_power(2);
      //Serial.println(F("card timeout reached, re-validating card."));
    }
  }
  // timeout re-reading card
  if((mode==MODE_ON)||(mode==MODE_CARD)) {
    if (time_diff(now,power_start)>OFF_TIMEOUT) {
      power_on(0);
      Serial.println(F("power off (card timeout)"));
      write_log("power off");
    }
  }
  
  // read NFC tag
  if ((mode == MODE_CARD)||(mode == MODE_OFF)) {
    // nfc card detected?
    uint8_t success = 0;
    uint8_t nfclen;  // length of nfc id

    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, nfcid, &nfclen);
    if (success) {
      handle_nfc(nfcid);
      //delay(100);
    } // success
    //Serial.println(" failed.");
  } //mode

  
  // serial (USB) from PC/user
  if(Serial.available()) {
    bytesrec = Serial.readBytes(usbmsg,msglen);
    handle_usb(usbmsg);
  }

  // serial from Wifi
  if(Serial1.available()) {
    bytesrec = Serial1.readBytes(wifimsg,msglen);
    handle_wifi(wifimsg);
  }
  
  delay(100);
}
// end of main loop





//
// functions
//


void reset_wifi() {
  // reset esp
  digitalWrite(PIN_ESP_RESET,LOW);
  //delay(350);
  //delay(800);
  delay(2000);
  digitalWrite(PIN_ESP_RESET,HIGH);
  //delay(2000);

  return;
}

//
// import access db via wifi
//
void import_wifi() {
  //
  // TODO: loop thru pages ...
  //
  int msglen=64;
  char msg[msglen];
  int bytesrec=0;
  user_t user;
  tag_t tag;
  char buffer[64];
  tag_t array[100];
  unsigned long id;
  int ac=0;
  int i;
  int page;
  int imported=0;
  String cmd;

  Serial.println(F("importing user db..."));
  write_log("importing user db...");

  tft.fillRect(0, 55, 310, 144, ILI9341_WHITE);

  tft.setTextColor(ILI9341_BLACK);
  tft.setRotation(3);
  sprintf(buffer,"%s","importing user db...");
  tft.drawString(buffer,20,80,4);

  // AT+CIPSTART="TCP","192.168.10.11",80
  //cmd = "AT+CIPSTART=\"TCP\",\"";  
  //cmd += "192.168.10.11";
  //cmd += "\",80";
  cmd = F("AT+CIPSTART=\"TCP\",\"192.168.10.11\",80");

  Serial1.println(cmd);  //send command to device

  delay(1000);  //wait a little while for 'Linked' response - this makes a difference
  if(Serial1.find("CONNECT")) {
    Serial.println(F("connected to server at 192.168.10.11"));
    sprintf(buffer,"%s","connected");
    tft.drawString(buffer,20,110,2);
  }
  else {
    Serial.println(F("'CONNECT' response not received"));
    sprintf(buffer,"%s","could not connect :(");
    tft.drawString(buffer,20,110,2);
  }

  delay(1000);
  
  // sequence ?page=1...10
  cmd = F("GET /cgi-bin/access\?amlaser HTTP/1.1\r\nHost: 192.168.10.11\r\n\r\n");
  Serial1.print(F("AT+CIPSEND="));
  Serial1.println(cmd.length());

  delay(200);

  //if(Serial1.find(">"))    //prompt offered by esp8266
  if(Serial1.find("OK")) {  //prompt offered by esp8266
    Serial.println(F("sending GET request..."));  //a debug message
    sprintf(buffer,"%s","sending GET request...");
    tft.drawString(buffer,20,125,2);
    Serial1.println(cmd);  //this is our http GET request
  }
  else {
    Serial1.println(F("AT+CIPCLOSE"));  //doesn't seem to work here?
    Serial.println(F("no '>' prompt received after AT+CPISEND"));
  }

  while (Serial1.find("ACCESS|")) {
    //if(Serial1.available()) {
    //bytesrec = Serial1.readBytes(msg,msglen);
    memset(msg,0,msglen);
    bytesrec = Serial1.readBytes(msg,10);
      
    sscanf(&msg[2],"%lx",(long *)&id);
    array[ac].b[3] = (id & 0xFF);
    array[ac].b[2] = ((id >> 8) & 0xFF);
    array[ac].b[1] = ((id >> 16) & 0xFF);
    array[ac].b[0] = ((id >> 24) & 0xFF);
    ac++;
  }    

  sprintf(buffer,"%d users received",ac);
  Serial.println(buffer);
  tft.drawString(buffer,20,140,2);
  write_log(buffer);

  for (i=0;i<ac;i++) {
    if(add_tag(array[i])==0) { imported++; }
  }

  sprintf(buffer,"%d new users imported",imported);
  Serial.println(buffer);
  tft.drawString(buffer,20,155,2);

  write_log(buffer);
  
  Serial1.println(F("AT+CIPCLOSE"));  
  delay(250);
  if(Serial1.find("OK")) {
    sprintf(buffer,"%s","connection closed.");
    tft.drawString(buffer,20,170,2);
  }

  //sprintf(buffer,"total: %d users imported",imported);
  //Serial.println(buffer);
  //tft.drawString(buffer,20,170,2);

   delay(3000);
} 
  

// user help (w/ and w/o enable)
void print_help() {
  time_t now;
  int secondsleft=0;
  
  Serial.println("");
  Serial.print(FIRMWARE);
  Serial.print(F(" v"));
  Serial.println(VERSION);
  Serial.println("");
  if(enable==1) {
    get_time(&now);
    secondsleft = ENABLE_TIMEOUT - time_diff(now,enable_start);
    Serial.println(F("available commands (enable mode):"));
    Serial.println("");
    Serial.println(F("addtag <nfctag>          : add nfc tag (4 byte id FFFFFFFF in hex) plus pin"));
    Serial.println(F("chpw <old_pw> <new_pw>   : change enable (privileged) mode password")); 
    Serial.println(F("cleardb                  : wipe out user db in eeprom"));
    Serial.println(F("clearlog                 : wipe out logs"));
    Serial.println(F("date                     : print current date/time"));
    Serial.println(F("deltag <nfctag>          : delete nfc tag (4 byte id FFFFFFFF in hex) (TODO)"));
    Serial.print  (F("disable                  : end enable mode ... "));Serial.print(secondsleft);Serial.println(F(" seconds left"));
    Serial.println(F("import                   : import access db via wifi"));
    Serial.println(F("list                     : list allowed nfc tags"));
    Serial.println(F("log                      : show log entries"));
    Serial.println(F("resetwifi                : reset esp wifi"));
    Serial.println(F("wifi <command>           : send command to wifi module, e.g. AT :)"));
    Serial.println(F("h (or ?)                 : this help"));
    //Serial.println(F("testlog <count>          : test log"));
    Serial.println("");
  }
  else { // free 4 all commands
    Serial.println(F("available commands:"));
    Serial.println("");
    Serial.println(F("h (or ?)                 : this help"));
    Serial.println(F("log                      : list log entries"));
    Serial.println(F("date                     : print current date / time"));
    Serial.println(F("enable <enable pw>       : switch to privileged mode")); 
    Serial.println("");
  }
}  



// handle serial from PC
void handle_usb(char * msg) {
  int i;
  
  // help
  if((msg[0]=='h')||(msg[0]=='?')) { print_help(); return; }
  // date
  if(strncmp(msg,"date",4)==0) { print_date(); return; }
  // log
  if(strncmp(msg,"log",3)==0) { read_log(); return; }
  // switch to enable mode
  if(strncmp(msg,"enable",6)==0) { start_enable(msg); return; }

  // for the rest of the user commmands, we need to be in enable mode
  if(enable!=1) { Serial.println(F("unknown command, try \"help\"")); return; }

  // add tag
  if(strncmp(msg,"addtag",6)==0) { if(add_nfctag(&msg[7])==0) { Serial.println(F("added.")); } return; }
  // change enable password
  if(strncmp(msg,"chpw",4)==0) { Serial.println(F("changing enable password.")); change_enable_pw(msg); return; }
  // clear eeprom user db
  if(strncmp(msg,"cleardb",7)==0) { clear_db(EE_USER); return; }
  // clear logs
  if(strncmp(msg,"clearlog",8)==0) { clear_log(); return; }
  // reset esp wifi
  if(strncmp(msg,"resetwifi",9)==0) { reset_wifi(); return; }
  // import users via wifi
  if(strncmp(msg,"import",6)==0) { import_wifi(); return; }
  // wifi
  if(strncmp(msg,"wifi",4)==0) { Serial1.println(&msg[4]); return; }
  // list
  if(strncmp(msg,"list",4)==0) { list_all(); return; }
  // leave enable mode
  if(strncmp(msg,"disable",7)==0) { Serial.println(F("leaving privileged mode.")); write_log("enable off"); enable=0; return; }

  // DEBUG
  // short (end of) log
  //if(strncmp(msg,"sl",2)==0) { short_log(); return; }
  // test log
  //if(strncmp(msg,"tl",2)==0) { test_log(&msg[3]); return; }
  // dump log
  //if(strncmp(msg,"dl",2)==0) { dump_log(&msg[3]); return; }


  // if we reached here ... it's an unsupported/unknow command ;)
  Serial.println(F("unknown command, try \"help\""));
  //Serial.println(msg);

}



// handle message from wifi
void handle_wifi(char * msg) {
  int i;
  char buf[64];

  // save wifi status
  //if(strstr(msg,"OK")!=NULL) { wifi_status=0; }
  //if(strstr(msg,"ERROR")!=NULL) { wifi_status=-1; strncpy(wifi_error,msg,sizeof(wifi_error)); }

  //if(strlen(msg) < 2) { return; }

  // debug
  Serial.print(F("debug message from wifi: ->")); Serial.print(msg); Serial.println(F("<-"));

}





//
// handle nfc card
void handle_nfc(uint8_t * nfcid) {
  int i;
  int block; // 1-63
  char buffer[64];
  time_t now;
  uint8_t success;
  uint8_t uidLength = 4;
  uint8_t data[16];
  tag_t tag;

  // update card (hard coded)
  if ((nfcid[0]==0xAA)&&(nfcid[1]==0xBB)&&(nfcid[2]==0xCC)&&(nfcid[3]==0xDD)) { 
    import_wifi();
    return;
  }

  // lookup id in db ...
  for (i=0;i<4;i++) { tag.b[i] = nfcid[i]; }
  if(get_id(tag) < 0) { // tag not on db
    // same unknown card
    if ((nfcid[0]==lastid[0])&&(nfcid[1]==lastid[1])&&(nfcid[2]==lastid[2])&&(nfcid[3]==lastid[3])) { delay(2000); return; }
    Serial.println(F("error: card not on db"));
    sprintf(buffer,"nfc tag 0x%02X%02X%02X%02X not on db",nfcid[0],nfcid[1],nfcid[2],nfcid[3]);
    write_log(buffer);
    // set lastid
    for (i=0;i<4;i++) { lastid[i]=nfcid[i]; }
    delay(1000);
    return;
  }
  // tag is on db
  // same known card again
  if((mode==MODE_ON)||(mode==MODE_CARD)) {
    if ((nfcid[0]==lastid[0])&&(nfcid[1]==lastid[1])&&(nfcid[2]==lastid[2])&&(nfcid[3]==lastid[3])) {
      mode = MODE_ON;
      get_time(&power_start);
      print_tft_power(1);
      return;
    }
  }

  if(read_card(nfcid)==0) {
    // Power ON
    power_on(1);
    get_time(&power_start);
    sprintf(buffer,"power on for 0x%02X%02X%02X%02X",nfcid[0],nfcid[1],nfcid[2],nfcid[3]);
    write_log(buffer);
    Serial.println(buffer);
    // set lastid
    for (i=0;i<4;i++) { lastid[i]=nfcid[i]; }
    print_tft_user();
  }
  
}




//
// read nfc card
//
int read_card(uint8_t * nfcid) {
  int i;
  int block; // 1-63
  char buffer[64];
  //time_t now;
  uint8_t success;
  uint8_t uidLength = 4;
  uint8_t data[16];
  tag_t tag;
  uint8_t data2[64];
  char data3[64];

  memset((void *)&user,0,sizeof(user));

  // name
  block = 8;
  // auth
  success = nfc.mifareclassic_AuthenticateBlock(nfcid, uidLength, block, 0, keyf);
  if (!success) { Serial.print(F("error: can not authenticate block ")); Serial.println(block,DEC); return(-1); }
  // read
  success = nfc.mifareclassic_ReadDataBlock(block, data);
  if (!success) { Serial.print(F("error: can not read block ")); Serial.println(block,DEC); return(-2); }
  //memset(user.name,0,33);
  memcpy(user.name,(const char *)data,16);
  //strncpy(user.name,(const char *)data,strlen((const char *)data));
  delay(250);
  // name part 2
  block = 9;
  // already auth
  // read
  success = nfc.mifareclassic_ReadDataBlock(block, data);
  if (!success) { Serial.print(F("error: can not read block ")); Serial.println(block,DEC); return(-2); }
  memcpy(&user.name[16],(const char *)data,16);
  delay(250);

  // phone
  block = 10;
  // already auth
  // read
  success = nfc.mifareclassic_ReadDataBlock(block, data);
  if (!success) { Serial.print(F("error: can not read block ")); Serial.println(block,DEC); return(-2); }
  strncpy(user.phone,(const char *)data,strlen((const char *)data));
  delay(250);

  // serial
  block = 13;
  // auth
  success = nfc.mifareclassic_AuthenticateBlock(nfcid, uidLength, block, 0, (uint8_t *)keyf);
  if (!success) { Serial.print(F("error: can not authenticate block ")); Serial.println(block,DEC); return(-1); }
  // read
  success = nfc.mifareclassic_ReadDataBlock(block, data);
  if (!success) { Serial.print(F("error: can not read block ")); Serial.println(block,DEC); return(-2); }
  memcpy(user.serial,(const char *)data,4);

  // check for tampered serial
  for (i=0;i<4;i++) {
    if(user.serial[i] != nfcid[i]) {
      // card tampered !!!
      sprintf(buffer,"card tampered : 0x%02X%02X%02X%02X",nfcid[0],nfcid[1],nfcid[2],nfcid[3]);
      Serial.println(buffer);
      write_log(buffer);
      return(-3);
    }
  }

  // write last use timestamp to card
  block = 14;
  // already auth
  // write
  memset(data, 0, sizeof(data));
  //snprintf ((char *)data,sizeof(data),"%02d:%02d %04d%02d%02d",now.hour,now.minute,now.year,now.month,now.day);
  snprintf ((char *)data,sizeof(data),"%04d%02d%02d %02d:%02d",now.year,now.month,now.day,now.hour,now.minute);
  success = nfc.mifareclassic_WriteDataBlock (block, data);
  if (!success) { Serial.print(F("error: can not write block ")); Serial.println(block,DEC); return(-2); }

  return(0);

}


//
// print user name+phone+serial to tft
//
void print_tft_user() {
  char ser[16];

  tft.fillRect(0, 55, 310, 144, ILI9341_WHITE);
  tft.setTextColor(ILI9341_BLACK);
  tft.setRotation(3);
  // name
  tft.drawString(user.name,20,80,4);
  // phone
  tft.drawString(user.phone,20,120,4);
  // serial
  sprintf (ser,"0x%02X%02X%02X%02X",user.serial[0],user.serial[1],user.serial[2],user.serial[3]);
  tft.drawString(ser,20,160,4);
}





// convert hex to decimal ... and vice versa
byte __attribute__ ((noinline)) dec2hex(byte val) {
  return ((val/10*16) + (val%10));
}
byte __attribute__ ((noinline)) hex2dec(byte val) {
  return ( (val/16*10) + (val%16) );
}


// get time/date from RTC via I2C
void get_time (time_t *now ) {
  Wire.beginTransmission(I2C_RTC_ADDR);
  Wire.write(0); // set DS3232 register pointer to 0x00
  Wire.endTransmission();
  Wire.requestFrom(I2C_RTC_ADDR, 7); 
  now->second = hex2dec(Wire.read() & 0x7f);
  now->minute = hex2dec(Wire.read());
  now->hour   = hex2dec(Wire.read() & 0x3f);
  Wire.read(); // dayOfWeek
  now->day    = hex2dec(Wire.read());
  now->month  = hex2dec(Wire.read());
  now->year   = 2000 + hex2dec(Wire.read());

  // debug
//  char date_s[32];
//  sprintf(date_s,"DATE:   %02d:%02d:%02d   %02d.%02d.%04d",now->hour,now->minute,now->second,now->day,now->month,now->year);
//  Serial.println(date_s);
}


// set RTC date/time (needed only once in batteries lifetime)
void set_time(time_t now) {
  Wire.beginTransmission(I2C_RTC_ADDR);
  Wire.write(0); // set DS3232 register pointer to 00h
  Wire.write(dec2hex(now.second));
  Wire.write(dec2hex(now.minute));
  Wire.write(dec2hex(now.hour));  // set 24 hour format (bit 6 == 0)
  Wire.write(4); // day of week 1=sun ... 7=sat
  Wire.write(dec2hex(now.day));
  Wire.write(dec2hex(now.month));
  Wire.write(dec2hex(now.year-2000)); 
  Wire.endTransmission();  
/**
  00h - seconds
  01h - minutes
  02h - hours
  03h - day of the week. The convention is to use 1 for Sunday, 2 for Monday, etc. 
  04h - date (1~31)
  05h - month (1~12)
  06h - year (00~99). 
**/
}


// time diff: difference between two times 
// ... just as 7-3 -> int_diff(7,3) = 4 > 0
// now after  then ->  >0
// now before then ->  <0
long time_diff(time_t now, time_t then) {
  int h,m,s,d;
  long diff;
  d=now.day-then.day;
  h=now.hour-then.hour;
  m=now.minute-then.minute;
  s=now.second-then.second;
  //diff = 3600L*(now.hour-then.hour)+60*(now.minute-then.minute)+now.second-then.second;
  diff = 3600L*h+60*m+s;
  if(d != 0) { diff += 86400; } // day rollover
  return(diff);
}





// find tag entry, return id (int) (or -1 if not found)
int get_id(tag_t tag) {
  unsigned int id,first,next;
  unsigned int counter=0;
  int i=0;
  byte x[4];
  unsigned int addr=0;

  // next tag number
  next = ((EEPROM.read(EE_NEXT)<<0) & 0xFF) + ((EEPROM.read(EE_NEXT+1)<<8) & 0xFFFF);
  if(next==0) { return(-1); }
  for (id=0;id<next;id++) {
    // addr[0123]
    addr=EE_USER+4*id;
    // check id
    for (i=0;i<4;i++) {
      if (EEPROM.read(addr+i) != tag.b[i]) { break; }
      /**
      x[i]=EEPROM.read(addr+i);
      Serial.print("get_id: id:");
      Serial.print(id,DEC);
      Serial.print(" i:");
      Serial.print(i,DEC);
      Serial.print(" x: 0x");
      Serial.print(x[i],HEX);
      Serial.print(" tag: 0x");
      Serial.print(tag.b[i],HEX);
      Serial.println();
      if (x[i] != tag.b[i]) { break; }
      **/
    }
    if(i==4) { return(id); }
  }
  return(-1);
}



void i2c_eeprom_write( int deviceaddress, unsigned int eeaddress, char* data) {
  unsigned char i=0, counter=0;
  unsigned int  address;
  unsigned int  page_space;
  unsigned int  page=0;
  unsigned int  num_writes;
  unsigned int  data_len=0;
  unsigned char first_write_size;
  unsigned char last_write_size;  
  unsigned char write_size;  

  // calculate length of data
  do{ data_len++; } while(data[data_len]);

  // calculate space available in first page
  page_space = int(((eeaddress/64) + 1)*64)-eeaddress;

  // calculate first write size
  if (page_space>16){
     first_write_size=page_space-((page_space/16)*16);
     if (first_write_size==0) first_write_size=16;
  }   
  else 
     first_write_size=page_space; 
    
  // calculate size of last write  
  if (data_len>first_write_size) 
     last_write_size = (data_len-first_write_size)%16;   
  
  // calculate how many writes we need
  if (data_len>first_write_size)
     num_writes = ((data_len-first_write_size)/16)+2;
  else
     num_writes = 1;

  i=0;   
  address=eeaddress;
  for(page=0;page<num_writes;page++) {
     if(page==0) write_size=first_write_size;
     else if(page==(num_writes-1)) write_size=last_write_size;
     else write_size=16;
  
     Wire.beginTransmission(deviceaddress);
     Wire.write((int)((address) >> 8));
     Wire.write((int)((address) & 0xFF));
     counter=0;
     do{ 
        Wire.write((byte) data[i]);
        i++;
        counter++;
     } while((data[i]) && (counter<write_size));  
     Wire.endTransmission();
     address+=write_size;
     delay(8);  // need 5ms for page write
  }
}


void i2c_eeprom_write_null( int deviceaddress, unsigned int eeaddresspage, byte length ) {
  Wire.beginTransmission(deviceaddress);
  Wire.write((int)(eeaddresspage >> 8)); // MSB
  Wire.write((int)(eeaddresspage & 0xFF)); // LSB
  byte i;
  for ( i = 0; i < length; i++) { Wire.write(0); }
  Wire.endTransmission();
  delay(6); // need 5ms for page write
}


void i2c_eeprom_read( int deviceaddress, unsigned int eeaddress, unsigned char* data, unsigned int num_chars) {
  unsigned char i=0;
  Wire.beginTransmission(deviceaddress);
  Wire.write((int)(eeaddress >> 8));   // MSB
  Wire.write((int)(eeaddress & 0xFF)); // LSB
  Wire.endTransmission();
  Wire.requestFrom(deviceaddress,num_chars);
  while(Wire.available()) data[i++] = Wire.read();
}


void i2c_eeprom_write_byte( int deviceaddress, unsigned int eeaddress, byte data ) {
    int rdata = data;
    Wire.beginTransmission(deviceaddress);
    Wire.write((int)(eeaddress >> 8)); // MSB
    Wire.write((int)(eeaddress & 0xFF)); // LSB
    Wire.write(rdata);
    Wire.endTransmission();
    delay(5);  // need 3.5ms single write
}


byte i2c_eeprom_read_byte( int deviceaddress, unsigned int eeaddress ) {
    byte rdata = 0xFF;
    Wire.beginTransmission(deviceaddress);
    Wire.write((int)(eeaddress >> 8)); // MSB
    Wire.write((int)(eeaddress & 0xFF)); // LSB
    Wire.endTransmission();
    Wire.requestFrom(deviceaddress,1);
    if (Wire.available()) rdata = Wire.read();
    return rdata;
}



// encode nfc tag string into 4 byte
int encode_tag(char *tag_s, tag_t *tag) {
  // tag_s[]="0x01020304"
  unsigned long id;

  // tag starts with "0x..."
  if((tag_s[0]!='0')||(tag_s[1]!='x')) {  return(-1); }

  sscanf(&tag_s[2],"%lx",(long *)&id);
  
  // tag
  //  user->b[0] = 0;
  //  user->b[1] = 0;
  tag->b[3] = (id & 0xFF);
  tag->b[2] = ((id >> 8) & 0xFF);
  tag->b[1] = ((id >> 16) & 0xFF);
  tag->b[0] = ((id >> 24) & 0xFF);

  return(0);
}  


// list all users (and pins?)
void list_all() {
  char buffer[32];
  unsigned int addr;
  unsigned int i;
  unsigned int next;
  unsigned int counter=0;
  unsigned long num;
  unsigned int pre;
  unsigned int pin;
  tag_t tag;

  Serial.print(F("list of allowed users (tags): (#"));
  next = ((EEPROM.read(EE_NEXT)<<0) & 0xFF) + ((EEPROM.read(EE_NEXT+1)<<8) & 0xFFFF);
  Serial.print (next);
  Serial.println(")");

  //for (addr=EE_USER;addr<EE_MAX;addr+=8) {
  for (addr=EE_USER;addr<EE_MAX;addr+=4) {
    counter++;
    if (counter>next) { break; }
    for(i=0;i<4;i++) { tag.b[i]=EEPROM.read(addr+i); }
      // convert
      sprintf(buffer,"#%03d 0x%02X%02X%02X%02X",counter,tag.b[0],tag.b[1],tag.b[2],tag.b[3]);
      Serial.println(buffer);
  }  
}


// add a tag (tag_t * tag) 
// to eeprom access list
int add_tag(tag_t tag) {
  unsigned int i;
  unsigned int next;
  char *ptr;
  unsigned int addr;
  
  // get next entry number
  next = ((EEPROM.read(EE_NEXT)<<0) & 0xFF) + ((EEPROM.read(EE_NEXT+1)<<8) & 0xFFFF);
  if(next>(EE_MAX-EE_USER)/4) { Serial.println(F("error: db full, can not add!")); return(-1); }
  // now check if already existent
  if(get_id(tag)>=0) { 
    return(-1);
  }


  // write entry
  addr = EE_USER + 4*next;
  for(i=0;i<4;i++) { EEPROM.write(addr+i,tag.b[i]); }
  next++;
  EEPROM.write(EE_NEXT,(next & 0xFF));
  EEPROM.write(EE_NEXT+1,((next>>8) & 0xFF));

  return(0);  
}




// ok
// add a tag (tag_t * tag) 
// to eeprom access list
int add_nfctag(char *stag) {
  unsigned int i;
  unsigned int next;
  char *ptr;
  unsigned int addr;
  tag_t tag;

  if(encode_tag(stag,&tag)<0) { Serial.println(F("can not encode tag...")); return(-1); }
  add_tag(tag);
  return(0);  
}





// write the enable pw to eeprom
int write_enable_pw (char *pw) {
  int i=0;
  if (strlen(pw) > EE_ENABLE_MAX_LEN) { Serial.println(F("error: password too long!")); return(-1); }
  for (i=0;i<strlen(pw);i++) { EEPROM.write(EE_ENABLE_PW+i,pw[i]); }
  EEPROM.write(EE_ENABLE_PW+i+1,0); // trailing 0
  EEPROM.write(EE_ENABLE_LEN,strlen(pw));
  return(0);
}


// compare the enable pw with eeprom
int check_enable_pw (char *pw) {
  int i;
  int len=0;
  char x=0;
  
  len=EEPROM.read(EE_ENABLE_LEN);
  for (i=0;i<len;i++) {
    x=EEPROM.read(EE_ENABLE_PW+i);
    if (x!=pw[i]) { Serial.println(F("error: password does not match !")); return(-1); }
  }
  return(0);
}


// start enable mode
int start_enable (char *msg) {
  if(enable==1) { return(0); }
  if(check_enable_pw(&msg[7])==0) {
    enable=1;
    get_time(&enable_start);
    Serial.println(F("switching to (privileged) enable mode."));
    write_log("enable on");
    return(0);
  }
  else {
    Serial.println(F("wrong password."));
    write_log("enable failed : wrong pw");
    return(-1);
  }
}



// change enable password , msg="chpw oldpw newpw"
int change_enable_pw (char *msg) {
  char *ptr;
  //char pw[EE_ENABLE_MAX_LEN];
  write_log("password change attempted");

  // old pw
  if(check_enable_pw(&msg[5])!=0) {
    Serial.println(F("error: enable pw does not match !"));
    write_log("password change failed (no match)");
    return(-1);
  }
  // new pw
  // first remove CR
  ptr=strchr(&msg[5],'\r'); if (ptr!=NULL) { *ptr=0; }
  // search for second " "
  ptr=strchr(&msg[5],' ');
  if (ptr==NULL) { return(-1); } // wrong format
  ptr++;
  if(strlen(ptr)>EE_ENABLE_MAX_LEN-1) { Serial.println(F("error: enable pw too long !")); return(-1); } // too long
  write_enable_pw (ptr);
  write_log("password updated in eeprom");
  return(0);
}



// print current date/time form rtc
int print_date () {
  char buffer[64];
  time_t now;
  get_time(&now);
  sprintf (buffer,"current date: %04d/%02d/%02d %02d:%02d:%02d",now.year,now.month,now.day,now.hour,now.minute,now.second);
  Serial.println(buffer);  
  return(0);
}



// clear user (tag) db in internal eeprom (EE_USER...EE_MAX)
void clear_db(unsigned int startaddr) {
  unsigned int addr;

  startaddr=min(startaddr,EE_MAX);
  startaddr=max(startaddr,EE_USER);

  if(startaddr==EE_USER) { // clearing all
    Serial.print(F("clearing eeprom (user db) ..."));
    // set next to 0
    EEPROM.write(EE_NEXT,0);
    EEPROM.write(EE_NEXT+1,0);
  }
  for (addr=startaddr;addr<4096;addr+=1) {
    EEPROM.write(addr,0);
    if((startaddr==EE_USER)&&(addr%100==0)) {Serial.print(F(".")); }
  }
  if(startaddr==EE_USER) {Serial.println(F(" done.")); }
}





//
// LOGGING
//


// clear i2c eeprom log space
int clear_log() {
  //unsigned int i=0;
  unsigned long i=0;
  //char buffer[255];
  //time_t now;

  Serial.print(F("clearing all logs .."));

  // 16 byte chunks
  for (i=0;i<EE_LOG_MAX;i+=16) {
    if(i>=EE_LOG_MAX) { break; } // paranoia
    i2c_eeprom_write_null(I2C_EE_ADDR, i, 16);
    //if(i%512==0) { Serial.print("."); Serial.print(i,DEC); }
    if(i%512==0) { Serial.print(F(".")); }
  }
 
  // write new log start to eeprom:
  i2c_eeprom_write_byte(I2C_EE_ADDR, EE_LOG_START_ADDR, 0);
  i2c_eeprom_write_byte(I2C_EE_ADDR, EE_LOG_START_ADDR+1, 0);
  // write new log end to eeprom:
  i2c_eeprom_write_byte(I2C_EE_ADDR, EE_LOG_END_ADDR, 0);
  i2c_eeprom_write_byte(I2C_EE_ADDR, EE_LOG_END_ADDR+1, 0);

  Serial.println(F(" done."));
  write_log("cleared logs");
}






// TODO: read log flag==0: read ; flag==1; short ; flag==2: save to disk
//int read_log (unsigned int flag) {
// read log
int read_log () {
  //unsigned int i=0;
  unsigned long i=0;
  unsigned int bytesread=0;
  int len=0;
  //unsigned int logstart=0;
  //unsigned int logend=0;
  unsigned long logstart=0;
  unsigned long logend=0;
  byte buf[4];
  char buffer[255];
  time_t now;
  unsigned int chunk=0;
  unsigned int chunk_max_size=16;
  //char *ptr;

  get_time(&now);
  memset(buffer,0,sizeof(buffer));
  sprintf (&buffer[0],"log listing requested at %04d/%02d/%02d %02d:%02d:%02d",now.year,now.month,now.day,now.hour,now.minute,now.second);
  Serial.println(&buffer[0]);

  // read start of logs from eeprom
  buf[0] = i2c_eeprom_read_byte( I2C_EE_ADDR, EE_LOG_START_ADDR+1);
  buf[1] = i2c_eeprom_read_byte( I2C_EE_ADDR, EE_LOG_START_ADDR);
  logstart=((buf[0]<<0) & 0xFF) + ((buf[1]<<8) & 0xFFFF);
  // read end of logs from eeprom
  buf[0] = i2c_eeprom_read_byte( I2C_EE_ADDR, EE_LOG_END_ADDR+1);
  buf[1] = i2c_eeprom_read_byte( I2C_EE_ADDR, EE_LOG_END_ADDR);
  logend=((buf[0]<<0) & 0xFF) + ((buf[1]<<8) & 0xFFFF);

  // debug
  Serial.print(F("logstart: ")); Serial.print(logstart); Serial.print(F(" logend: ")); Serial.println(logend);

  // logstart>logend? ... so read from start to max first
  if(logstart > logend) {
    while (logstart<EE_LOG_MAX) {
      //chunk = min(30,(EE_LOG_MAX-logstart));
      chunk = min(chunk_max_size,(EE_LOG_MAX-logstart));
      memset(buffer,0,sizeof(buffer));
      i2c_eeprom_read(I2C_EE_ADDR, logstart, (byte *) &buffer[0], chunk);
      logstart+=chunk;
      Serial.print(buffer);
    }
    // set start to 0 ... to continue
    logstart=0;
  }
  //chunk=30;



  while (logstart<logend) {
    //chunk = min(30,(logend-logstart));
    chunk = min(chunk_max_size,(logend-logstart));
    memset(buffer,0,sizeof(buffer));
    i2c_eeprom_read(I2C_EE_ADDR, logstart, (byte *) &buffer[0], chunk);
    logstart+=chunk;
    Serial.print(buffer);
  }
  
  
  Serial.println();
  return(0);
}




// write (append) to log in external 32k i2c eeprom
int write_log(const char *msg) {
  unsigned long i = 0;
  int len = 0;
  unsigned long logstart = 0;
  unsigned long logend = 0;
  unsigned long logend_new = 0;
  byte buf[2];
  char buffer[255];
  char rbuffer[255];
  time_t now;
  char *ptr;
  long spaceleft = 0;
  int rollover = 0;

  // prepare log message with time stamp
  get_time(&now);
  memset(buffer,0,sizeof(buffer));
  snprintf (&buffer[0],254,"%04d/%02d/%02d %02d:%02d:%02d %s\r\n\0",now.year,now.month,now.day,now.hour,now.minute,now.second,msg);
  len=strlen(&buffer[0]);

  // read start of logs from eeprom
  buf[0] = i2c_eeprom_read_byte( I2C_EE_ADDR, EE_LOG_START_ADDR+1);
  buf[1] = i2c_eeprom_read_byte( I2C_EE_ADDR, EE_LOG_START_ADDR);
  logstart=((buf[0]<<0) & 0xFF) + ((buf[1]<<8) & 0xFFFF);
  // read end of logs from eeprom
  buf[0] = i2c_eeprom_read_byte( I2C_EE_ADDR, EE_LOG_END_ADDR+1);
  buf[1] = i2c_eeprom_read_byte( I2C_EE_ADDR, EE_LOG_END_ADDR);
  logend=((buf[0]<<0) & 0xFF) + ((buf[1]<<8) & 0xFFFF);

  // old logend: rollover before?
  if(logstart > logend) { rollover=1; }

  // check for space left till end
  spaceleft = EE_LOG_MAX - logend;
  // debug
  memset(rbuffer,0,sizeof(rbuffer));
  // will we roll over
  if(len > spaceleft) { // just rolling over ... split into two parts
    rollover=1;
    strcpy(rbuffer,&buffer[spaceleft]);
    memset(&buffer[spaceleft],0,sizeof(buffer)-spaceleft);
    // write 2nd part starting from 0 first
    i2c_eeprom_write(I2C_EE_ADDR, 0, &rbuffer[0]);
  }

  // write 1st part anyway
  i2c_eeprom_write(I2C_EE_ADDR, logend, &buffer[0]);

  // new logend (modulo log size)
  logend = (logend + len) % EE_LOG_MAX;


  if(rollover>0) {
    logstart = (logend + 1) % EE_LOG_MAX; // default
    // now search ... for new start addr at newline
    // read max 8*16 bytes
    for (i=0;i<8;i++) {
      memset(buffer,0,sizeof(buffer));
      i2c_eeprom_read(I2C_EE_ADDR, logend+(i*16), (byte *) &buffer,16);
      ptr=strchr(buffer,'\n');
      if(ptr!=NULL) { logstart = (logend + i*16 + (ptr - buffer) + 1 ) % EE_LOG_MAX; break; }
    }
  }
  else { } // never rolled over, logstart is at 0

  // write new log end to eeprom:
  buf[0] = (logend & 0xFF);
  buf[1] = ((logend >> 8) & 0xFF);
  i2c_eeprom_write_byte(I2C_EE_ADDR, EE_LOG_END_ADDR, buf[1]);
  i2c_eeprom_write_byte(I2C_EE_ADDR, EE_LOG_END_ADDR+1, buf[0]);

  // just in case, write new log start to eeprom:
  if(logstart >0) {
    buf[0] = (logstart & 0xFF);
    buf[1] = ((logstart >> 8) & 0xFF);
    i2c_eeprom_write_byte(I2C_EE_ADDR, EE_LOG_START_ADDR, buf[1]);
    i2c_eeprom_write_byte(I2C_EE_ADDR, EE_LOG_START_ADDR+1, buf[0]);
  }
}






//
void print_tft_power(int on) {
  char buffer[8];

  tft.setRotation(3);
  tft.setTextColor(ILI9341_WHITE);

  if(on==0) {
    tft.fillRect(240, 3, 78, 45, ILI9341_RED);
    sprintf(buffer,"%s","OFF");
    tft.drawString(buffer,255,15,4);
  }
  sprintf(buffer,"%s","ON");
  if(on==1) {
    tft.fillRect(240, 3, 78, 45, ILI9341_GREEN);
    tft.drawString(buffer,260,15,4);
  }
  if(on==2) {
    tft.setTextColor(ILI9341_ORANGE,ILI9341_GREEN);
    tft.drawString(buffer,260,15,4);
  }
}



//
// print time hh:mm to tft
//
void print_tft_time() {
  char buffer[8];
  char buf[8];
  int color;
  
  sprintf (buffer,"%02d:%02d",now.hour,now.minute);
  tft.setRotation(3);
  tft.setTextSize(1);
  color=tft.color565(0xE0,0xE0,0xE0);
  tft.setTextColor(color);
  sprintf (buf,"88:88");
  tft.drawString(buf,83,3,7); // Overwrite the text to clear it
  tft.setTextColor(ILI9341_NAVY);
  tft.drawString(buffer,83,3,7);
}





void print_tft_sense() {
  int s=-1;
  int bgcolor=0;
  char buffer[16];
  
  tft.setRotation(3);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);

  // 3 66 129 192 255
  // 26

  // sense#0 service hatches
  s = digitalRead(SENSE_0);
  if(s != sense[0]) {
    sense[0]=s;
    if(s>0) { bgcolor=ILI9341_RED; }
    else {  bgcolor=ILI9341_GREEN; }
    tft.fillRect(3, 212, 60, 26, bgcolor);
    sprintf (buffer,"Hatches");
    tft.drawString(buffer,3+8,218,2);
  }
  // sense#1 front hatch
  s = digitalRead(SENSE_1);
  if(s != sense[1]) {
    sense[1]=s;
    if(s>0) { bgcolor=ILI9341_RED; }
    else {  bgcolor=ILI9341_GREEN; }
    tft.fillRect(66, 212, 60, 26, bgcolor);
    sprintf (buffer,"Front");
    tft.drawString(buffer,66+16,218,2);
  }
  // sense#2 chiller
  s = digitalRead(SENSE_2);
  if(s != sense[2]) {
    sense[2]=s;
    if(s>0) { bgcolor=ILI9341_RED; }
    else {  bgcolor=ILI9341_GREEN; }
    tft.fillRect(129, 212, 60, 26, bgcolor);
    sprintf (buffer,"Chiller");
    tft.drawString(buffer,129+9,218,2);
  }

  // sense#3 compressed air
  s = digitalRead(SENSE_3);
  if(s != sense[3]) {
    sense[3]=s;
    if(s>0) { bgcolor=ILI9341_RED; }
    else {  bgcolor=ILI9341_GREEN; }
    tft.fillRect(192, 212, 60, 26, bgcolor);
    sprintf (buffer,"CompAir");
    tft.drawString(buffer,192+8,218,2);
  }

  // sense#4 reserved
  s = digitalRead(SENSE_4);
  if(s != sense[4]) {
    sense[4]=s;
    //if(s>0) { bgcolor=ILI9341_RED; }
    //else {  bgcolor=ILI9341_GREEN; }
    bgcolor=ILI9341_LIGHTGREY;
    tft.fillRect(255, 212, 60, 26, bgcolor);
    sprintf (buffer,"Reserve");
    tft.drawString(buffer,255+7,218,2);
  }
  
//  // sense#5 reserved
//  s = digitalRead(SENSE_5);
//  if(s != sense[5]) {
//    sense[5]=s;
//    //if(s>0) { bgcolor=ILI9341_RED; }
//    //else {  bgcolor=ILI9341_GREEN; }
//    bgcolor=ILI9341_LIGHTGREY;
//    tft.fillRect(255, 212, 28, 26, bgcolor);
//    sprintf (buffer,"Reserve");
//    tft.drawString(buffer,255+7,218,2);
//  }

}


void print_tft_laser(int on) {  
  int xoff=30;
  int yoff=3;
  int color;
  
  if(on==0) { 
    tft.fillRect(3, 3, 75, 45, ILI9341_LIGHTGREY); 
    color=ILI9341_DARKGREY;
  }
  else { 
    tft.fillRect(3, 3, 75, 45, ILI9341_YELLOW); 
    color=ILI9341_RED;
  }

  tft.drawLine(10, 22+yoff, 22+xoff, 22+yoff, color);  // -

  tft.drawLine(8+xoff, 22+yoff, 36+xoff, 22+yoff, color);  // -
  tft.drawLine(22+xoff, 8+yoff, 22+xoff, 36+yoff, color);   // |

  tft.drawLine(12+xoff, 12+yoff, 32+xoff, 32+yoff, color);  // 
  tft.drawLine(12+xoff, 32+yoff, 32+xoff, 12+yoff, color);  // /

  tft.drawLine(11+xoff, 27+yoff, 33+xoff, 17+yoff, color);
  tft.drawLine(17+xoff, 33+yoff, 27+xoff, 11+yoff, color);

  tft.drawLine(11+xoff, 17+yoff, 33+xoff, 27+yoff, color);
  tft.drawLine(17+xoff, 11+yoff, 27+xoff, 33+yoff, color);

}


void print_tft_test() {

  int i=0,j=0;
  int r=34;
  int h=78;
  int d=4;
  int z;
  int COLOR_KEY_FG=0xFFFF;
  int COLOR_KEY_BG=0x0000;
  char buffer[8];
  
  for (j=0;j<4;j++) {
    for (i=0;i<3;i++) {
       z=j*3+i+1;
       if(z==11) { z=0; }
       if(z!=10) {
         if(z==12) { 
           tft.setTextColor(ILI9341_WHITE);
           tft.setTextSize(1);
           sprintf (buffer,"Delete");
           tft.drawString(buffer,i*(h+d)+20,j*(2*r+d)+70,2);
         }
         else {
           tft.drawCircle(i*(h+d)+h/2, j*(2*r+d)+r+35, r, COLOR_KEY_FG);
           tft.fillCircle(i*(h+d)+h/2, j*(2*r+d)+r+35, r-1, COLOR_KEY_BG);
           tft.setTextColor(ILI9341_WHITE);
           sprintf(buffer,"%d",z);
           tft.drawString(buffer,i*(h+d)+32,j*(2*r+d)+20+40,4);
         }
       } //if
     } //for
   } //for
}



//
// set power (on | off )
//
void power_on(int on) {

  analogWrite(PIN_TFT,200);
  get_time(&dim_start);
  dim=1;
  char buffer[32];

  tft.setRotation(3);
  tft.fillRect(0, 55, 310, 144, ILI9341_WHITE);
  if(on==0) {
    mode=MODE_OFF;
    // power relais : LOW => ON , HIGH => OFF
    digitalWrite(PIN_POWER,HIGH);
    tft.setTextColor(ILI9341_BLACK);
    tft.setRotation(3);
    sprintf(buffer,"%s","Insert Card");
    tft.drawString(buffer,95,110,4);
  }
  else { 
    mode=MODE_ON;
    // power relais : LOW => ON , HIGH => OFF
    digitalWrite(PIN_POWER,LOW);
  }

  print_tft_time();
  print_tft_power(on);
  print_tft_laser(on);
  print_tft_sense();

}

