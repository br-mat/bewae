//#########################################################################################################################
//  br-mat (c) 2021
//#########################################################################################################################
//  Author:                             Matthias Braun
//  email:                        matthiasbraun@gmx.at
//#########################################################################################################################

//todo:
// - shift register ground switched away!
// - flow sensor not integrated yet
// - 74hc595 ground transistor remove completely or find solution (high side switch maybee)
//#########################################################################################################################
//  INCLUDE
//#########################################################################################################################
// arduino standard libs
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
// extern
#include "Seeed_BME280.h"
#include <LowPower.h>
#include <TimerOne.h>
//#########################################################################################################################

#define DEBUG

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//             additional information
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//YZ-401 durchflusssensor; link:
//https://www.robotics.org.za/YF-S401
//rtc module zs-042; link of the tutorial reused & define the adress of the module
//tutorial found: https://tronixstuff.com/2014/12/01/tutorial-using-ds1307-and-ds3231-real-time-clock-modules-with-arduino/

/*
//MUX channel setup (illustrate)
uint8_t mux_channel[16][4]={
  {0,0,0,0}, //channel 0
  {1,0,0,0}, //channel 1
  {0,1,0,0}, //channel 2
  {1,1,0,0}, //channel 3
  {0,0,1,0}, //channel 4
  {1,0,1,0}, //channel 5
  {0,1,1,0}, //channel 6
  {1,1,1,0}, //channel 7
  {0,0,0,1}, //channel 8
  {1,0,0,1}, //channel 9
  {0,1,0,1}, //channel 10
  {1,1,0,1}, //channel 11
  {0,0,1,1}, //channel 12
  {1,0,1,1}, //channel 13
  {0,1,1,1}, //channel 14
  {1,1,1,1}  //channel 15
};
*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//   definitions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//PINS: [RX,TX,2,3,4,5,6,7,8,9,10,11,12,13,A0,A1,A2,A3,A4,A5,A6,A7]
//      [ ?, ?,+,+,+,+,+,+,+,+, +, ?, ?, ?,  , +, +, +, +, +,  ,  ]
//used pins marekd with '+' other reserved are signed with '?'

#define nano_adr 0x07 //adress of arduino nano
#define DS3231_I2C_ADDRESS 0x68 //adress bme280 sensor
BME280 bme280;
#define slave_adr 0x08 //adress of slave esp for wifi
#define max_groups 6 //number of plant groups
#define measure_intervall 600000UL // value/1000 ==> seconds
#define twbr_global 230 //set TWBR value for i2c frequency

const uint8_t chip_select=10; //SPI cs pin

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//             ESP power down
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define esp_pdown 2
bool esp_busy = false;
unsigned long esp_time = 0;
#define esp_timeout 60000UL //timeout time of esp time in ms
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//             Transistor pins
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t q1_nmos = 4;      //Mosfet transistor for hungry 5v devices siwthing (gnd sided switch)
uint8_t q2_npn_sens = 3;  //Bipolar transistor switching of sensors (gnd npn)
uint8_t q4_npn_ensh = A2; //Bipolar transistor enable for 74hc595 shiftregister (gnd npn NEVER SWITCH OFF)

//Fast pwm Pin
uint8_t vent_pwm = 9;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//             MUX 74hc595 pins
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//MUX 1 definition
#define s0_mux_1 (uint8_t)5
#define s1_mux_1 (uint8_t)6
#define s2_mux_1 (uint8_t)7
#define s3_mux_1 (uint8_t)8
#define sig_mux_1 A1
#define en_mux_1 A3
//uint8_t s0_mux_1 = 5;
//uint8_t s1_mux_1 = 6;
//uint8_t s2_mux_1 = 7;
//uint8_t s3_mux_1 = 8;
//uint8_t en_mux_1 = A3; //enable mux 1
//uint8_t sig_mux_1= A1; //signal pin of mux 1

uint8_t mux[4] = {s0_mux_1,s1_mux_1,s2_mux_1,s3_mux_1};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//             default static variable definitions & virtual pins
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//String dummy[100]; //approx data measurment string
// virtual pins (shift register)
#define pump1 (uint8_t)7
#define pump2 (uint8_t)6
uint8_t groups[6] = {0,1,2,3,4,5};
//uint8_t pump1 = 7; //shift pin 7
//uint8_t pump2 = 6; //shift pin 0

//total vent&sensor count
uint8_t sens_count=12;
uint8_t vent_count=6;

//limitations & watering & other
//group limits and watering (some hardcoded stuff)                          
//          type                  amount of plant/brushes    estimated water amount in liter per day (average hot +30*C)
//group 1 - große tom             5 brushes                  ~8l
//group 2 - Chilli, Paprika       5 brushes                  ~1.5l
//group 3 - Kräuter (trocken)     4 brushes                  ~0.2l
//group 4 - Hochbeet2             10 brushes                 ~4l
//group 5 - kleine tom            4 brushes                  ~3.5l
//group 6 - leer 4 brushes +3 small?        ~0.5l

int group_st_time[6]={120,10,5,30,60,0};//{180, 60, 5, 0, 120, 0} depending on assigned plants to the valve-pipe system
                                        //standard watering time per day and valve (about 0.7-1l/min)
int watering_base[6]={0}; //placeholder array to handle watering time over more than one loop period
                          //watering should pause when exceeding the measurement period

//moisture sensors
//int low_lim = 300;  //lower limitations, values lower are not realistic
//int high_lim = 600; //high limitation, values passed that threshold are not realistic

//important global variables
byte sec_; byte min_; byte hour_; byte day_w_; byte day_m_; byte mon_; byte year_;
unsigned long up_time = 0;    //general estimated uptime
unsigned long last_activation = 0; //timemark variable
bool thirsty = false; //marks if a watering cycle is finished
bool config = false;  //handle wireless configuration
bool config_bewae_sw = true; //switch watering on off
bool config_watering_sw = true; //switch default and custom values, default in group_st_time and custom sent via mqtt

//wireless config array to switch on/off functions
// watering time of specific group; binary values;
const int raspi_config_size = max_groups+2; //6 groups + 2 binary
int raspi_config[raspi_config_size]={0};
byte esp_status = 0; //indicating data recieved from esp01, set true in request event

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//             functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// seting shiftregister to defined value (8bit)
void shiftvalue8b(uint8_t val){
  //Function description: shiftout 8 bit value, MSBFIRST
  //FUNCTION PARAMETER:
  //val         -- 8bit value writte out to shift register                             uint8_t
  //------------------------------------------------------------------------------------------------
  shiftOut(s0_mux_1, s2_mux_1, MSBFIRST, val); //take byte type as value
  digitalWrite(s1_mux_1, HIGH); //update output of the register
  delay(1);
  digitalWrite(s1_mux_1, LOW);
}


void system_sleep(){
  //Function: deactivate the modules, prepare for sleep & setting mux to "lowpower standby" mode:
  digitalWrite(vent_pwm, LOW);     //pulls vent pwm pin low
  digitalWrite(q2_npn_sens, LOW);  //deactivates sensors
  digitalWrite(q1_nmos, LOW);      //deactivates energy hungry devices
  //digitalWrite(q4_npn_ensh, LOW);  //deactivates shiftregister to disable pump in case of a problem from setupsequence
  //delay(1);
  digitalWrite(q4_npn_ensh, HIGH);  //activates shiftregister and set all pins 0
  shiftvalue8b(0);
  digitalWrite(en_mux_1, HIGH);    //deactivates mux 1
  digitalWrite(s0_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
  digitalWrite(s1_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
  digitalWrite(s2_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
  digitalWrite(s3_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
}


// Function to copy 'len' elements from 'src' to 'dst'
// code: https://forum.arduino.cc/index.php?topic=274173.0
void copy(int* src, int* dst, int len) {
  //Function description: copy strings
  //FUNCTION PARAMETER:
  //src         -- source array                                                       int
  //dst         -- destiny array                                                      int
  //len         -- length of array                                                    int
  //------------------------------------------------------------------------------------------------
  memcpy(dst, src, sizeof(src[0])*len);
}


void watering(uint8_t datapin, uint8_t clock, uint8_t latch, uint8_t _time, uint8_t vent_pin, uint8_t pump_pin, uint8_t en, uint8_t pwm){
  //Function description: Controlls the watering procedure on valves and pump
  // Careful with interupts! To avoid unwanted flooding.
  //FUNCTION PARAMETER:
  //datapin     -- pins to set the mux binaries [4 pins]; mux as example;             uint8_t
  //clock       -- selected channel; 0-15 as example;                                 uint8_t
  //latch       -- signal input signal output; free pin on arduino;                   uint8_t
  //time        -- time in seconds, max 60 seconds                                    uint8_t
  //vent_pin    -- virtual pin number with shift register                             uint8_t
  //pump_pin    -- virtual pin number with shift register                             uint8_t 
  //pwm         -- pwm pin for fast pwm                                               uint8_t
  //------------------------------------------------------------------------------------------------
  //initialize state
  digitalWrite(en, LOW);
  digitalWrite(clock, LOW); //make sure clock is low so rising-edge triggers
  digitalWrite(latch, LOW);
  digitalWrite(datapin, LOW);
  #ifdef DEBUG
  Serial.print(F("uint time:")); Serial.println(_time);
  #endif
  digitalWrite(en, HIGH);
  unsigned long time_s = (unsigned long)_time * 1000UL;
  if (time_s > 60000UL){
    time_s = 60000UL;
    #ifdef DEBUG
    Serial.println(F("time warning: time exceeds 60 sec"));
    #endif
  }

  // seting shiftregister to 0
  shiftOut(datapin, clock, MSBFIRST, 0); //take byte type as value
  digitalWrite(latch, HIGH); //enables the output of the register
  delay(1);
  digitalWrite(latch, LOW);
  #ifdef DEBUG
  Serial.print(F("Watering group: "));
  Serial.println(vent_pin); Serial.print(F("time: ")); Serial.println(time_s);
  #endif
  // perform actual function
  byte value = (1 << vent_pin) + (1 << pump_pin);
  #ifdef DEBUG
  Serial.print(F("shiftout value: ")); Serial.println(value);
  #endif
  shiftOut(datapin, clock, MSBFIRST, value);
  digitalWrite(latch, HIGH); //enables the output of the register
  delay(1);
  digitalWrite(latch, LOW);
  delay(200); //wait balance load after pump switches on
  Timer1.pwm(pwm, 950); //fastpwm to avoid anoying noise of solenoids
  delay(1);
  Timer1.pwm(pwm, 930); //fastpwm to avoid anoying noise of solenoids
  delay(1);
  Timer1.pwm(pwm, 900); //fastpwm to avoid anoying noise of solenoids
  delay(1);
  Timer1.pwm(pwm, 880); //fastpwm to avoid anoying noise of solenoids
  delay(1);
  Timer1.pwm(pwm, 850); //fastpwm to avoid anoying noise of solenoids
  delay(1);
  Timer1.pwm(pwm, 820); //fastpwm to avoid anoying noise of solenoids
  delay(1);
  Timer1.pwm(pwm, 800); //fastpwm to avoid anoying noise of solenoids
  delay(time_s);

  // reset
  // seting shiftregister to 0
  shiftOut(datapin, clock, MSBFIRST, 0); //take byte type as value
  digitalWrite(latch, HIGH); //enables the output of the register
  delay(1);
  digitalWrite(latch, LOW);

  digitalWrite(clock, LOW); //make sure clock is low so rising-edge triggers
  digitalWrite(latch, LOW);
  digitalWrite(datapin, LOW);
  digitalWrite(en, LOW);
}


//controll mux function
void controll_mux(uint8_t control_pins[], uint8_t channel, uint8_t sipsop, uint8_t enable, String mode, int *val){
  //Function description: Controlls the mux, only switches for a short period of time for reading and sending short pulses
  //FUNCTION PARAMETER:
  //control_pins  -- pins to set the mux binaries [4 pins]; mux as example;            uint8_t array [4]
  //NOT IN USE channel_setup -- array to define the 16 different channels; mux_channel as example; uint8_t array [16][4]
  //channel       -- selected channel; 0-15 as example;                                 uint8_t
  //sipsop        -- signal input signal output; free pin on arduino;                   uint8_t
  //enable        -- enable a selected mux; free pin on arduino;                        uint8_t
  //mode          -- mode wanted to use; set_low, set_high, read;                       String
  //val           -- pointer to reading value; &value in function call;                 int (&pointer)   
  //------------------------------------------------------------------------------------------------
  
  uint8_t channel_setup[16][4]={
    {0,0,0,0}, //channel 0
    {1,0,0,0}, //channel 1
    {0,1,0,0}, //channel 2
    {1,1,0,0}, //channel 3
    {0,0,1,0}, //channel 4
    {1,0,1,0}, //channel 5
    {0,1,1,0}, //channel 6
    {1,1,1,0}, //channel 7
    {0,0,0,1}, //channel 8
    {1,0,0,1}, //channel 9
    {0,1,0,1}, //channel 10
    {1,1,0,1}, //channel 11
    {0,0,1,1}, //channel 12
    {1,0,1,1}, //channel 13
    {0,1,1,1}, //channel 14
    {1,1,1,1}  //channel 15
  };
  
  //make sure sig in/out of the mux is disabled
  digitalWrite(enable, HIGH);
  
  //selecting channel
  for(int i=0; i<4; i++){
    digitalWrite(control_pins[i], channel_setup[channel][i]);
  }
  //modes
  //"set_low" mode
  if(mode == String("set_low")){
    pinMode(sipsop, OUTPUT); //turning signal to output
    delay(3);
    digitalWrite(sipsop, LOW);
    digitalWrite(enable, LOW);
    delay(2);
    digitalWrite(enable, HIGH);
    pinMode(sipsop, INPUT); //seting back on input to not accidentally short the circuit somewhere
  }
  //"set_high" mode
  if(mode == String("set_high")){
    pinMode(sipsop, OUTPUT); //turning signal to output
    delay(3);
    digitalWrite(sipsop, HIGH);
    digitalWrite(enable, LOW);
    delay(2);
    digitalWrite(enable, HIGH);
    pinMode(sipsop, INPUT); //seting back to input to not accidentally short the circuit somewhere
  }
  //"read" mode
  if(mode == String("read")){
    double valsum=0;
    pinMode(sipsop, INPUT); //make sure its on input
    digitalWrite(enable, LOW);
    delay(3);
    *val=analogRead(sipsop); //throw away 
    *val=analogRead(sipsop); //throw away
    *val=analogRead(sipsop); //throw away
    *val=analogRead(sipsop); //throw away
    *val=analogRead(sipsop); //throw away
    *val=analogRead(sipsop); //throw away
    for(int i=0; i<5; i++){
      valsum += analogRead(sipsop);
    }
    *val=(int)(valsum/5.0+0.5); //take measurement (mean value)
    delay(1);
    digitalWrite(enable, HIGH);
  }
}


void save_datalog(String data, uint8_t cs, String file){
  //Function: saves data given as string to a sd card via spi
  //FUNCTION PARAMETER
  //data       --      a string to save on SD card;    String
  //cs         --      Chip Select of SPI;             int
  //file       --      filename as string;             String
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  Serial.begin(9600);
  pinMode(cs, OUTPUT);
  // see if the card is present and can be initialized: NEEDED ALTOUGH NOTHING IS DONE!!! DONT DELETE
  bool sd_check;
  byte iteration =0;
  if (!SD.begin(cs)) {
    // don't do anything more: 
    //return;
    #ifdef DEBUG
    Serial.println(F("some problem occured: SD card not there"));
    #endif
    sd_check=false;
  }
  else{
    sd_check=true;
  }
  while(!sd_check){
    delay(5000);
    if(!SD.begin(cs)){
      //do again nothing
      #ifdef DEBUG
      Serial.print(F(" - again not found"));
      #endif
    }
    else{
      sd_check=true;
    }
    iteration = iteration + 1;
    if(iteration > 5){
      #ifdef DEBUG
      Serial.println(F(" - not trying again"));
      #endif
      sd_check=false;
    }
  }
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open(file, FILE_WRITE);
  // if the file is available, write to it:
  if (dataFile){
    dataFile.println(data);
    dataFile.close();
    // print to the serial port too:
    Serial.println(data);
  }   //ln 
  delay(1000);  //need time to save for some reason to work without mistakes
}
// Convert normal decimal numbers to binary coded decimal
byte dec_bcd(byte val)
{
  return( (val/10*16) + (val%10) );
}
// Convert binary coded decimal to normal decimal numbers
byte bcd_dec(byte val)
{
  return( (val/16*10) + (val%16) );
}


//took basically out of the librarys example as the rest of the time functions, slightly modded
void set_time(byte second, byte minute, byte hour, byte dayOfWeek, byte
//Function: sets the time on the rtc module (iic)
//FUNCTION PARAMETERS:
//second     --                   seconds -- byte
//minute     --                   minutes -- byte
//hour       --                   hours   -- byte
//dayofweek  --         weekday as number -- byte
//dayofmonrh --    day of month as number -- byte
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
dayOfMonth, byte month, byte year)
{
  // sets time and date data to DS3231
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set next input to start at the seconds register
  Wire.write(dec_bcd(second)); // set seconds
  Wire.write(dec_bcd(minute)); // set minutes
  Wire.write(dec_bcd(hour)); // set hours
  Wire.write(dec_bcd(dayOfWeek)); // set day of week (1=Sunday, 7=Saturday)
  Wire.write(dec_bcd(dayOfMonth)); // set date (1 to 31)
  Wire.write(dec_bcd(month)); // set month
  Wire.write(dec_bcd(year)); // set year (0 to 99)
  Wire.endTransmission();
}


void read_time(byte *second,byte *minute,byte *hour,byte *dayOfWeek,byte *dayOfMonth,byte *month,byte *year)
{
//Function: read the time on the rtc module (iic)
//FUNCTION PARAMETERS:
//second     --                   seconds -- byte
//minute     --                   minutes -- byte
//hour       --                   hours   -- byte
//dayofweek  --         weekday as number -- byte
//dayofmonrh --    day of month as number -- byte
//month      --                     month -- byte
//year       --   year as number 2 digits -- byte
//comment: took basically out of the librarys example as the rest of the time functions, slightly modded
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
  // request seven bytes of data from DS3231 starting from register 00h
  *second = bcd_dec(Wire.read() & 0x7f);
  *minute = bcd_dec(Wire.read());
  *hour = bcd_dec(Wire.read() & 0x3f);
  *dayOfWeek = bcd_dec(Wire.read());
  *dayOfMonth = bcd_dec(Wire.read());
  *month = bcd_dec(Wire.read());
  *year = bcd_dec(Wire.read());
}


String timestamp(){
//Function: give back a timestamp as string (iic)
//FUNCTION PARAMETERS:
// NONE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  String time_data="";
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  // retrieve data from DS3231
  read_time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month,
  &year);
  time_data += String(hour, DEC);
  time_data += String(F(","));
  time_data += String(minute, DEC);
  time_data += String(F(","));
  time_data += String(second, DEC);
  time_data += String(F(","));
  time_data += String(dayOfMonth, DEC);
  time_data += String(F(","));
  time_data += String(month, DEC);
  return time_data;
}


//sendbyte function
//payload codes to trigger specific actions on ESP
byte signalesp01(byte payload){
  Wire.beginTransmission(0x08);
  Wire.write(payload);
  return Wire.endTransmission();
}

//NEW transmitt
//Testing
byte transmitt(int data[], int len){
  delay(3000);
  // data - FIXED SIZE 28: int array should be packed in max [28]; less values will send zeros from esp01 to influxdb
  byte rstatus = 0;
  if(len/2 < (int)(28 * sizeof(int) +1)){ //changed
    //int tsize = sizeof data;
    //Serial.println(tsize);
    #ifdef DEBUG
    Serial.println(F("Debugstatement inside if positive"));
    #endif
    int tsize = 14 * sizeof (int)+1;
    byte bdata[tsize]={0}; //+1 because 1 transmission byte; last byte cannot be read correctly sometimes (WHY?!)
    memcpy(bdata, data, tsize);
    bdata[tsize-1] = 0; //transmission part value
    for(unsigned int i=0; i<sizeof bdata-1; i=i+2){
      #ifdef DEBUG
      Serial.print(F("value: "));
      Serial.println(bdata[i] | bdata [i+1] << 8);
      #endif
    }
    #ifdef DEBUG
    Serial.print(F("transmissionbyte"));
    Serial.println((byte)bdata[tsize]);
    #endif
    Wire.beginTransmission(0x08);
    Wire.write(bdata, tsize);
    //Wire.write(0);
    //Wire.endTransmission();
    rstatus += Wire.endTransmission();
    delay(3000); //was 2000
    memcpy(bdata, data +14, tsize);
    bdata[tsize-1] = 1; //transmission part value
    #ifdef DEBUG
    for(unsigned int i=0; i<sizeof bdata-1; i=i+2){
      Serial.print(F("value: "));
      Serial.println(bdata[i] | bdata [i+1] << 8);
    }
    Serial.print(F("last byte: "));
    Serial.println((byte)bdata[tsize]);
    #endif
    Wire.beginTransmission(0x08);
    Wire.write(bdata, tsize);
    //Wire.write(1);
    //Wire.endTransmission();
    rstatus += Wire.endTransmission();
  }
  return rstatus;
}


unsigned int readVcc(void) {
  /*
   * readVcc - reads actual VCC on arduino using 1.1v reference
   */
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  ADMUX = (1<<REFS0) | (1<<MUX3) | (1<<MUX2) | (1<<MUX1); //sets admux B01001110
  //where MUX3...0 -- (last 4 bits) B....1110 activates 1.1v ref (atmega328) on single ended input
  delay(3); // Wait for Vref to settle
  ADCSRA |= (1<<ADSC); // Start conversion
  while(bit_is_set(ADCSRA,ADSC)); // measuring
  unsigned int result = ADC;
  //custom scale factor, processor specific NEED TO BE MEASURED on Aref pin!!
  //analogReference(INTERNAL);  //perform one analog read to dummy variable to make sure to activate the internal 1.1v
  result = 1131473UL / (unsigned long)result; // Calculate Vcc (in mV); 1101824 = 1.XXXXX*1024*1000  -- (INTERNAL => 1.078V in my case -- 1101824)
  return result; // Vcc in millivolts
}

//FIND OUT WHILE 1 or 0 < blabla ??
//interrupt funciton for receiving data on iic bus, keep in mind that it can get stuck from time to time
//esp as 2nd master is allowed to send only at specific times!
void receiveEvent(int bytes){
  //collect transmitted bytes
  Serial.print("bytes recieve: ");Serial.println(bytes);
  byte barr[bytes]={0};
  int i = 0;
  barr[i]=Wire.read(); //thorw away first byte
  while(1<Wire.available()){
    barr[i]=Wire.read();
    i++;
  }
  byte status_sign = Wire.read();
  Serial.print("Status byte: "); Serial.println(status_sign);
  int size=(float)bytes/2.0+0.5;
  int data[size] = {0};
  i = 0;
  //transform byte array to int data array
  for(int j=0; j<bytes; j=j+2){
    data[i]=barr[j] | barr[j+1] << 8;
    #ifdef DEBUG
    Serial.print("ESP sent: ");
    Serial.println(barr[i]);
    #endif
    i++;
  }
  switch((int)status_sign) {
    case 111: //111 all good reporting process finished
      //check controll byte indicating a correct transmission & passing to predefined static array
      for(unsigned int i=0;i<raspi_config_size;i++){
        if(i<sizeof(data)/2){
          raspi_config[i]=data[i];
        }
        else{
          raspi_config[i]=0;
        }
      }
      Serial.println("Data received, transmission correct");
      esp_status=(byte)1;
      break;

    case 112: //112 all good but process could not be finished;
      esp_status=(byte)2; // NOT IMPLEMENTED YET this should allow signaling Nano to not shutdown ESP because it needs extra time
      break;

    default :
      esp_status=(byte)3;
      #ifdef DEBUG
      Serial.print(F("Error: data transmission went wrong signing byte incorrect"));
      #endif
   }
}


void setup() {
  // SETUP AND INITALIZE
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // setup serial com
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  Serial.begin(9600);
  #ifdef DEBUG
  Serial.print(F("Hello, World!"));
  #endif
  Wire.begin(nano_adr); //join iic, to avoid problems with serial comms we run the bus with multiple masters (2)
                        //ESP01 seems to have trouble on working as a slave device
  //TWBR = 78;   //78 --> 25 kHz ; 158 --> 12,5 kHz
  TWBR=twbr_global; //lower clock speed of SCL so esp01 can read the message it is neccessary to set controller speed at 160Hz
  //TWBR = 250;    //formula: freq = clock / (16 + (2 * TWBR * prescaler))
  //TWSR |= bit (TWPS0);  // change prescaler did not work for me, default 1
  //prescaler description
  //https://www.arnabkumardas.com/arduino-tutorial/i2c-register-description/
  Wire.onReceive(receiveEvent);
  #ifdef DEBUG
  Serial.println(F("Wiresetup debug 0"));
  #endif
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // setup pin mode
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  pinMode(esp_pdown, OUTPUT);     //2
  pinMode(q2_npn_sens, OUTPUT);   //3
  pinMode(q1_nmos, OUTPUT);       //4
  pinMode(s0_mux_1, OUTPUT);      //5  (mux controll pin & 74hc595 data pin)
  pinMode(s1_mux_1, OUTPUT);      //6  (mux controll pin & 74hc595 latch pin)
  pinMode(s2_mux_1, OUTPUT);      //7  (mux controll pin & 74hc595 clk pin)
  pinMode(s3_mux_1, OUTPUT);      //8
  pinMode(vent_pwm, OUTPUT);      //9
  pinMode(chip_select, OUTPUT);   //10
  //hardware defined SPI pin        11
  //hardware defined SPI pin        12
  //hardware defined SPI pin        13
  pinMode(A0, INPUT);             //A0
  pinMode(sig_mux_1, INPUT);      //A1
  pinMode(en_mux_1, OUTPUT);      //A2
  pinMode(q4_npn_ensh, OUTPUT);   //A3
  //hardware defined IIC pin        A4
  //hardware defined IIC pin        A5
  pinMode(A6, INPUT);             //A6
  pinMode(A7, INPUT);             //A7
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //initialize pins
  //  (seting mux pins HIGH so it not wastes energy leaking down when LOW)
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  delay(500);
  digitalWrite(esp_pdown, LOW);    //puts esp01 into sleep
  digitalWrite(vent_pwm, LOW);     //pulls vent pwm pin low
  digitalWrite(q2_npn_sens, LOW);  //deactivates sensors
  digitalWrite(q1_nmos, LOW);      //deactivates energy hungry devices
  digitalWrite(q4_npn_ensh, HIGH);  //deactivates shiftregister (SET ALL PINS HIGH on output because ground switched away)
  digitalWrite(en_mux_1, HIGH);    //deactivates mux 1
  digitalWrite(s0_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
  digitalWrite(s1_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
  digitalWrite(s2_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
  digitalWrite(s3_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
  //controll_mux(mux, mux_channel, reset_ff_vent, sig_mux_1, en_mux_1, String("set_low"), &value);
  //controll_mux(mux, mux_channel, reset_ff_pump, sig_mux_1, en_mux_1, String("set_low"), &value);
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //init timer1 lib -- solenoids produce anoying beep sound wenn pulsed
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //  --> bring them to a higher frequency so the sound is less anoying (cant be heard above 20kHz)
  //EXAMPLE: (100) is 100 micro seconds = 10KHz
  Timer1.initialize(40);
  //for activate the PWM
  //  //Timer1.pwm(9, 512) gives 50% on pin D9
  //  Timer1.pwm(9, 830);
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //initialize bme280
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  if(!bme280.init()){
    #ifdef DEBUG
    Serial.println(F("bme device error!"));
    #endif
  }


  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //init time and date
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //uncomment if want to set the time
  //set_time(01,42,17,02,30,07+2,20);

  //seting time (second,minute,hour,weekday,date_day,date_month,year)
  //set_time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)
  //  sec,min,h,day_w,day_m,month,ear
  #ifdef DEBUG
  Serial.println(F("debug statement"));
  #endif
  //initialize global time
  read_time(&sec_, &min_, &hour_, &day_w_, &day_m_, &mon_, &year_);
  #ifdef DEBUG
  Serial.println(F("debug 0"));
  #endif

  delay(500);  //make TX pin Blink 2 times, visualize end of setup
  Serial.print(measure_intervall);
  delay(500);
  Serial.println(measure_intervall);
  
  delay(500);
  shiftvalue8b(0); //reset shiftregister to fixed state (all 0)
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//             MAIN LOOP
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NO INTERUPTS! while watering they can cause massive problems!
// solution would be to turn them off - for testing reasons i keep the step by step way

void loop() {
  // --- main loop ---
  // put your main code here, to run repeatedly:
  digitalWrite(q4_npn_ensh, HIGH);  //activate shift register and set it all to zero
  shiftvalue8b(0);
  #ifdef DEBUG
  Serial.println(F("Start loop debug 1"));
  #endif
  //delay(500);

  // --- init phase ---
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //check if RTC answers on IIC bus
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  byte rtc_status = Wire.endTransmission();
  if(rtc_status != 0){
    #ifdef DEBUG
    Serial.println(F("Error: rtc device not available!"));
    #endif
    while(rtc_status != 0){
      //loop as long as the rtc module is unavailable!
      #ifdef DEBUG
      Serial.print(F(" . "));
      #endif
      Wire.beginTransmission(DS3231_I2C_ADDRESS);
      rtc_status = Wire.endTransmission();
      system_sleep();
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
    }
  }
  #ifdef DEBUG
  Serial.println(F("enter init & check phase"));
  #endif
  //delay(200);
  byte sec1, min1, hour1, day_w1, day_m1, mon1 , y1;
  read_time(&sec1, &min1, &hour1, &day_w1, &day_m1, &mon1, &y1);
  if(hour_ != hour1){
  //if(true){
    up_time = up_time + (unsigned long)(60UL* 60UL * 1000UL); //refresh the up_time every hour, no need for extra function or lib to calculate the up time
    //if(1){
    if((hour1 == (byte)11) | (hour1 == (byte)19)){
      config = true; //only activate once per cycle
      thirsty = true;
      copy(group_st_time, watering_base, 6); //fill watering placeholder array
    }
  }
  unsigned long actual_time = (unsigned long)up_time+(unsigned long)sec1*1000+(unsigned long)min1*60000UL; //time in ms, comming from rtc module
                                                                                                 //give acurate values despide temperature changes

  if((esp_busy) & (esp_time+esp_timeout < actual_time)){
    esp_busy = false;
    digitalWrite(esp_pdown, LOW); //turn down esp
    #ifdef DEBUG
    Serial.println(F("shutdown ESP-01"));
    #endif
  }

  // --- datalog phase ---
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //saving data to SD, shape:
  //timestamp,pressure,temp,hum,analogdata(10 sensors+)
  // INFO:
  //Attention! RAM might get critical here. Backup string to the SD card and the Data int array itself cost a lot of memory!
  //           The SD lib need additional space to work.
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  #ifdef DEBUG
  Serial.print(F("Datalogphase: ")); Serial.println(actual_time-last_activation > measure_intervall);
  Serial.println(actual_time); Serial.println(last_activation); Serial.println(measure_intervall-500000UL);
  Serial.println((float)((float)actual_time-(float)last_activation)); Serial.println(up_time);
  #endif
  //if((unsigned long)(actual_time-last_activation) > (unsigned long)(measure_intervall-500000UL)) //replace for debuging
  if((unsigned long)(actual_time-last_activation) > (unsigned long)(measure_intervall))
  //if(true)
  //if(false)
  {
    last_activation = actual_time; //first action refresh the time
    TWBR=twbr_global; //make sure that IIC clock speed is low enough for esp01
    
    esp_busy = true;
    esp_time = actual_time;
    esp_status=(byte)0; //
    digitalWrite(esp_pdown,HIGH); //turn up esp
    delay(500);
    signalesp01((byte)2); //esp should update config (mqtt --> raspberrypi)
    #ifdef DEBUG
    Serial.print(F("Signaled esp to update data"));
    #endif
    //delay(10000); //give esp enought time for the request NEEDED?!????????????????????????????????
    
    delay(100);
    #ifdef DEBUG
    Serial.println(F("enter datalog phase"));
    #endif
    int value=0; //mux reading value
    int data2[28] = {0}; //all sensors via mux plus bme readings and additional space
    controll_mux(mux, 11, sig_mux_1, en_mux_1, "read", &value); //take battery measurement bevore devices are switched on
    data2[5] = readVcc();                                       //  (Supply voltage is not stressed)
    data2[4] = value;
    delay(1);
    digitalWrite(q2_npn_sens, HIGH); //activate sensors and give them some time to reduce noisy measurements
    digitalWrite(q4_npn_ensh, HIGH);  //activate shift register and set it all to zero
    shiftvalue8b(0);

    // --- prepare for datalog ---
    //digitalWrite(q4_npn_ensh, LOW);  //deactivates shiftregister, so actuall measurement can happen (troubles with values otherwise)
    delay(100);

    // --- collect data ---
    //delay(5000); //give some time to esp01 on iic bus (remove after testing)
    data2[0] = (float)bme280.getPressure() / (float)10+0;
    data2[1] = (float)bme280.getHumidity() * (float)100+0;
    data2[2] = (float)bme280.getTemperature() * (float)100+0;
    data2[3] = readVcc();
    //data2[4] = battery_indicator; //measurement is done early without stressed supply voltage
    //data2[5] = 0;                 //vcc
    //data2[22]=0;
    //data2[23]=0;
    //data2[24]=0;
    //data2[25]=0;
    //data2[26]=0;
    //data2[27]=0;
    for(int i=0; i<sens_count; i++){
      controll_mux(mux, i, sig_mux_1, en_mux_1, "read", &value);
      delay(1); //was 3
      data2[6+i] = value;
    }
    //delay(500);
    digitalWrite(q2_npn_sens, LOW);   //deactivate sensors
    digitalWrite(q4_npn_ensh, HIGH);  //reactivate shift register and set it all to zero
    shiftvalue8b(0);

    // --- log data to SD card (backup) ---
    String data="";
    data += timestamp();
    data += ",";
    for(uint8_t i=0; i<28; i++){
      data += data2[i];
      data += ",";
    }
    delay(500); //give some time
    save_datalog(data,chip_select, "datalog2.txt");
    #ifdef DEBUG
    Serial.print(F("data:"));
    Serial.println(data);
    #endif
    data=""; //clear string
    // --- sending data to esp01 & log to INFLUXDB ---
    delay(5000); //was 5000
    TWBR=twbr_global; //make sure that IIC clock speed is low enough for esp01
    delay(500); //give some time to esp01 on iic bus
    //int tsize = sizeof data2;
    byte stat = transmitt(data2, sizeof(data2));
    if(stat != 0x00)
    {
      #ifdef DEBUG
      Serial.print(F("Transmission Error! Value: "));//transmissiion error
      Serial.println(stat);
      #endif
    }
    else
    {
      #ifdef DEBUG
      Serial.println(F("Transmission OK!"));
      #endif
    }
    delay(10);
  }
  
  // --- data recieving phase ---
  //IMPORTANT! this should only be used once in the begin off every watering circle. To avoid pumping out too much water.
  //
  //expecting i2c to send all values as int (2 bytes) with max 32 bytes due to i2c limitation
  //the bool values can be optimized if needed because 2 bytes for true or false is a big waste
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  if(config){
    if(!esp_busy){
          esp_busy = true;
          esp_time = actual_time;
          esp_status=(byte)0; //
          digitalWrite(esp_pdown,HIGH); //turn up esp
          delay(500);
          signalesp01((byte)2);
          delay(8000); //wait for esp to get the data
    }
    signalesp01((byte)3); //permit esp01 to takeover iic bus as master
    byte count = 0;
    while(esp_status == 0){
      //wait for esp to finish processing, it is important to wait for esp to finish iic messaging
      //  recieve event is triggered by interupt and can randomly halt and so freeze arduino from time to time
      //  this MUST NOT happen during the watering procedure
      
      //FINISH HANDLING OF ERROR CODES LATER
      //4 should probably shut down esp or set thirsty false to avoid conflict
      //probably skipp thisty for a while then?
      if(count>(byte)120){
        esp_status = (byte)4; //4 indicating an error, no answer from esp after about 1 min
      }
      count++;
      delay(1000);
    }
    if((esp_status == (byte)1) & (config)){
      config = false;
      config_bewae_sw=(bool)raspi_config[max_groups];
      config_watering_sw=(bool)raspi_config[max_groups+1];
      #ifdef DEBUG
      Serial.print(F("config triggered, bool values: ")); Serial.print(config_bewae_sw);
      Serial.print(F(" ")); Serial.println(config_watering_sw);
      #endif
      if(config_watering_sw){
        copy(raspi_config, watering_base, max_groups);
        for(int i=0; i<max_groups; i++){
          watering_base[i]=raspi_config[i];
        }
      }
      if(!config_bewae_sw){
        thirsty = false;
        int zeros[max_groups]={0};
        copy(raspi_config, zeros, max_groups);
      }
      //delay(1000);
    }
    if(esp_status == (byte)1){
      esp_busy = false;
      digitalWrite(esp_pdown, LOW); //turn down esp
      #ifdef DEBUG
      Serial.println(F("shutdown ESP-01"));
      #endif
    }
  }
  //test   test   test   test   test   test   test   test   test   test   test   test   test   test   test   test   
  Serial.println("test values raspi");
  for(int i=0; i < 8; i++){
    Serial.println(raspi_config[i]);
  }
  
  // --- watering phase ---
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //thirsty = false;  //uncomment if not wanted
  if(thirsty){
    #ifdef DEBUG
    Serial.println(F("start watering phase"));
    #endif
    // --- Watering ---
    //trigger at specific time
    //alternate the solenoids to avoid heat damage, let cooldown time if only one remains
    //Hints: it can happen that solenoids work without delay for 60+60 sec should be no problem when it happens once
    //       main mosfet probably get warm (check that!)
    //       overall time should not go over 40 min while watering
    //       NEVER INTERUPT WHILE WATERING!!!!!!!!!!!!!!!!!!
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    uint8_t watering_done[6]= {1,1,1,1,1,1};
    #ifdef DEBUG
    Serial.println(F("watering while"));
    #endif
    unsigned long start_t = millis();
    //bool finished_watering = false; //using thirsty instead
    while((start_t+measure_intervall > millis()) & (thirsty)){
      //solenoids need time to cool down after 60 sek on time
      //controlling for loop to alternate the groups and work on the "todo watering"
      #ifdef DEBUG
      Serial.println(F("watering while inside"));
      #endif
      for(uint8_t i=0; i<6; i++){
        int water_timer=0;
        uint8_t group = groups[i];
        #ifdef DEBUG
        Serial.println(group);
        Serial.print(F("watering base: ")); Serial.println(watering_base[i]);
        #endif
        delay(500);
        if(watering_base[i]<1){
          water_timer=0;
          watering_done[i]=0;
          #ifdef DEBUG
          Serial.println(F("watering group finished"));
          #endif
        }
        else if(watering_base[i]<60){
          water_timer=watering_base[i];
          watering_base[i]=watering_base[i]-water_timer;
        }
        else{
          water_timer=60;
          watering_base[i]=watering_base[i]-60;
        }
        
        //watering(uint8_t datapin, uint8_t clock, uint8_t latch, uint8_t time, uint8_t vent_pin, uint8_t pump_pin, uint8_t en, uint8_t pwm)
        if(water_timer > 60){
          //sanity check (should not be possible)
          water_timer = 0;
          #ifdef DEBUG
          Serial.println(F("Error: watering over 60 sec"));
          #endif
        }
        //water_timer = 0;
        if(water_timer != 0){
          #ifdef DEBUG
          Serial.println(F("start watering"));
          #endif
          watering(s0_mux_1, s2_mux_1, s1_mux_1, water_timer, group, pump1, q4_npn_ensh, vent_pwm);
          //Serial.print(F("cooldown time: ")); Serial.println(1000UL * water_timer / 2+10000);
          //delay(1000UL * water_timer / 2+10000); //cooldown pump (OLD)
          
          //While doing nothing put arduino to sleep when it waits to cooldown
          //sleep keep brown out detection (BOD) on in case of problems, so the system can reset
          //and end possibly harming situations
          unsigned long sleeptime = 1000UL * water_timer / 2+8000; //let it rest for half of active time + 8sec
          unsigned int sleep8 = sleeptime/8000;
          for(unsigned int i = 0; i<sleep8; i++){
            LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_ON); 
          }
          #ifdef DEBUG
          Serial.print(F("sleeptime%8: ")); Serial.println(sleeptime%8);
          #endif
          delay(sleeptime%8); //delay rest time not devidable by 8000ms
        }
      }
      // check how many groups still need watering
      byte sum = 0;
      for(int i=0; i<6; i++){
        //sum it up, could be done in the other for loop too
        sum += watering_done[i];
      }
      if(sum == (byte)1){
        //trigger if only one group remains in the list
        //delay for 30 sec for cooldown on solenoid
        delay(30000UL);
      }
      if(sum == (byte)0){
        //break the loop
        thirsty=false;
      }
      #ifdef DEBUG
      Serial.print(F("Groups left: ")); Serial.println(sum); Serial.println(thirsty);
      #endif
    }
  }

  delay(500);
  // --- end back to sleep ---
  ///////////////////////////////////////////////////////////////////////////
  //update global time variable
  read_time(&sec_, &min_, &hour_, &day_w_, &day_m_, &mon_, &year_);
  #ifdef DEBUG
  Serial.print(F("Hour:")); Serial.println(hour_);
  #endif
  system_sleep();

  //if not busy with watering sleep for about 2 min (15 iterations)
  if(!thirsty){
    #ifdef DEBUG
    Serial.println(F("enter sleep phase"));
    delay(100); //remove when not debuging
    #endif
    for(int i = 0; i<15; i++){
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF); 
    }
  }
}