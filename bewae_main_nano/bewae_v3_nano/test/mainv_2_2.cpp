//todo:
// - shift register ground switched away!
// - flow sensor not used
// - make sure shiftvalue8b function works and performes action even when shift reg is switched off

//#########################################################################################################################
//  INCLUDE
// auslagern in header.h ???
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
//#include <SoftwareSerial.h>
//#########################################################################################################################


//SoftwareSerial BT(10,9); // RX, TX 
//hm-11 module tx/rx communication
//#define HM11_PIN_TX PTE22 //FRDM-KL25Z UART2 
//#define HM11_PIN_RX PTE23
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

#define DS3231_I2C_ADDRESS 0x68 //adress bme280 sensor
BME280 bme280;
#define slave_adr 0x08 //adress of slave esp for wifi

const uint8_t chip_select=10; //SPI cs pin
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//   variables & pins
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//PINS: [RX,TX,2,3,4,5,6,7,8,9,10,11,12,13,A0,A1,A2,A3,A4,A5,A6,A7]
//      [ ?, ?, ,+,+,+,+,+,+,+, +, +, +, +, +, +, +, +, +, +,  ,  ]
//used pins marekd with '+' and reserved are signed with '?'

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//             Transistor pins
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t q1_nmos = 4;      //Mosfet transistor for hungry devices siwthing (gnd)
uint8_t q2_npn_sens = 3;  //Bipolar transistor switching of sensors (gnd)
uint8_t q4_npn_ensh = A3; //Bipolar transistor enable for 74hc595 shiftregister (gnd)

//Fast pwm Pin
uint8_t vent_pwm = 9;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//             MUX 74hc595 pins
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//MUX 1 definition
uint8_t s0_mux_1 = 5;
uint8_t s1_mux_1 = 6;
uint8_t s2_mux_1 = 7;
uint8_t s3_mux_1 = 8;

uint8_t mux[4] = {s0_mux_1,s1_mux_1,s2_mux_1,s3_mux_1};
uint8_t en_mux_1 = A2; //enable mux 1
uint8_t sig_mux_1= A1; //signal pin of mux 1

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//             default variable definitions & virtual pins
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//String dummy[100]; //to approx data measurment string

// virtual pins (shift register)
uint8_t groups[6] = {0,1,2,3,4,5};
uint8_t pump1 = 7; //shift pin 7
uint8_t pump2 = 6; //shift pin 0

//total vent&sensor count
uint8_t sens_count=12;
uint8_t vent_count=6;

//limitations & watering & other (move to loop later maybee)
//group limits and watering (some hardcoded stuff)                          
//          type                  amount of plant/brushes    water amount in liter for a day (30*)
//group 1 - große tom             7 brushes                  ~7.2l
//group 2 - Chilli, Paprika       5 brushes                  ~2l
//group 3 - Hochbeet1 (erdbeeren) 5 brushes                  ~2l
//group 4 - Hochbeet2             6 brushes                  ~2.5l
//group 5 - Kräuter (trocken)     3 brushes +1 small?        ~0.2l
//group 6 - Kräuter (mittel-nass) 4 brushes +3 small?        ~0.6l
int group_limlow[6]={400,410,425,420,445,405};
int group_limhigh[6]={440,450,465,450,490,450};
uint8_t group_reduce_low[6]={3,4,5,4,3,3}; //factor to devide = value/factor
uint8_t group_enhance_high[6]={3,4,2,4,3,3};  //factor to enhance = value+value/factor
int group_st_time[6]={120,30,4,100,70,0};//{180, 60, 5, 0, 120, 0}; //{630/3, 200/2, 18/3, 400/2, 320/2, 0 };//{20,20,20,20,20,20}; //standard watering time per day (about 0.7l/min)
//OLD! standart watering time per day (1.5 l per min - shared too all brushes!! {330, 80, 80, 120, 8, 24}) (new PUMP 2-3l per min;

int low_lim = 300;  //lower limitations, values lower are not realistic
int high_lim = 600; //dry 650 - high limitation, values passed that threshold are not realistic

//global time variables
byte sec_; byte min_; byte hour_; byte day_w_; byte day_m_; byte mon_; byte year_;
unsigned long up_time = 0;    //general estimated uptime
unsigned long last_activation = 0; //timemark variable
#define measure_intervall 600000UL // value/1000 ==> seconds

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//             functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// seting shiftregister to defined value (8bit)
void shiftvalue8b(uint8_t val){
  shiftOut(s0_mux_1, s2_mux_1, MSBFIRST, val); //take byte type as value
  digitalWrite(s1_mux_1, HIGH); //enables the output of the register
  delay(1);
  digitalWrite(s1_mux_1, LOW);
}

void system_sleep(){
  //Function: deactivate the modules, prepare for sleep & setting mux to "lowpower standby" mode:
  digitalWrite(vent_pwm, LOW);     //pulls vent pwm pin low
  digitalWrite(q2_npn_sens, LOW);  //deactivates sensors
  digitalWrite(q1_nmos, LOW);      //deactivates energy hungry devices
  digitalWrite(q4_npn_ensh, LOW);  //deactivates shiftregister to disable pump in case of a problem from setupsequence
  delay(1);
  digitalWrite(q4_npn_ensh, HIGH);  //activates shiftregister and set all pins 0
  shiftvalue8b(0);
  digitalWrite(en_mux_1, HIGH);    //deactivates mux 1
  digitalWrite(s0_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
  digitalWrite(s1_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
  digitalWrite(s2_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
  digitalWrite(s3_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
}

// c++ cant assign arrays to one another so i use a copy function
// Function to copy 'len' elements from 'src' to 'dst'
// code: https://forum.arduino.cc/index.php?topic=274173.0
void copy(int* src, int* dst, int len) {
  //Function: copy strings
  memcpy(dst, src, sizeof(src[0])*len);
}


void watering(uint8_t datapin, uint8_t clock, uint8_t latch, uint8_t _time, uint8_t vent_pin, uint8_t pump_pin, uint8_t en, uint8_t pwm){
  //Function description: Controlls the watering procedure on valves and pump
  // NEVER INTERUPT HERE!!!
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
  Serial.print(F("uint time:")); Serial.println(_time);
  digitalWrite(en, HIGH);
  unsigned long time_s = (unsigned long)_time * 1000UL;
  if (time_s > 60000UL){
    time_s = 60000UL;
    Serial.println(F("time warning: time exceeds 60 sec"));
  }

  // seting shiftregister to 0
  shiftOut(datapin, clock, MSBFIRST, 0); //take byte type as value
  digitalWrite(latch, HIGH); //enables the output of the register
  delay(1);
  digitalWrite(latch, LOW);
  Serial.print(F("Watering group: "));
  Serial.println(vent_pin); Serial.print(F("time: ")); Serial.println(time_s);
  // perform actual function
  byte value = (1 << vent_pin) + (1 << pump_pin);
  Serial.print(F("shiftout value: ")); Serial.println(value);
  shiftOut(datapin, clock, MSBFIRST, value);
  digitalWrite(latch, HIGH); //enables the output of the register
  delay(1);
  digitalWrite(latch, LOW);
  delay(350); //wait balance load after pump switches on
  Timer1.pwm(pwm, 900); //fastpwm to avoid anoying noise of solenoids
  delay(time_s); //NO INTERUPT HERE!!! Watering procedure

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


// int* control_pins, ... as pointer? or int control_pins[]
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
    //Serial.println("Card failed, or not present");
    // don't do anything more: 
    //return;
    Serial.println(F("some problem occured: SD card not there"));
    sd_check=false;
  }
  else{
    sd_check=true;
  }
  while(!sd_check){
    delay(5000);
    if(!SD.begin(cs)){
      //do again nothing
      Serial.print(F(" - again not found"));
    }
    else{
      sd_check=true;
    }
    iteration = iteration + 1;
    if(iteration > 5){
      Serial.println(F(" - not trying again"));
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


//NEW transmitt
//Testing
byte transmitt(int data[]){
  delay(3000);
  // data - FIXED SIZE 28: int array should be packed in max [28]; less values will send zeros from esp01 to influxdb
  byte rstatus = 0;
  if(sizeof data < 28 * sizeof (int) +1){
    //int tsize = sizeof data;
    //Serial.println(tsize);
    Serial.println(F("Debugstatement inside if positive"));
    
    int tsize = 14 * sizeof (int);
    byte bdata[tsize+2]={0}; //+2 because 1 transmission byte and the last byte cannot be read correctly sometimes (WHY?!)
    memcpy(bdata, data, tsize);
    bdata[tsize] = 0;
    for(unsigned int i=0; i<sizeof bdata-1; i=i+2){
      Serial.print(F("value: "));
      Serial.println(bdata[i] | bdata [i+1] << 8);
    }
    Serial.print(F("transmissionbyte"));
    Serial.println((byte)bdata[tsize]);
    Wire.beginTransmission(0x08);
    Wire.write(bdata, tsize+2);
    //Wire.write(0);
    //Wire.endTransmission();
    rstatus += Wire.endTransmission();
    delay(3000); //was 2000
    memcpy(bdata, data +14, tsize);
    bdata[tsize] = 1;
    for(unsigned int i=0; i<sizeof bdata-1; i=i+2){
      Serial.print(F("value: "));
      Serial.println(bdata[i] | bdata [i+1] << 8);
    }
    Serial.print(F("last byte: "));
    Serial.println((byte)bdata[tsize]);
    Wire.beginTransmission(0x08);
    Wire.write(bdata, tsize+2);
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

void setup() {
  // SETUP AND INITALIZE
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // setup serial com
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  Serial.begin(9600);
  Serial.print(F("Hello, World!"));

  Wire.begin(); //join iic as Master
  //TWBR = 78;   //78 --> 25 kHz
  TWBR = 230;  //158 --> 12,5 kHz
  //TWBR = 250;    //formula: freq = clock / (16 + (2 * TWBR * prescaler))
  //TWSR |= bit (TWPS0);  // change prescaler did not work for me, default 1
  //prescaler description
  //https://www.arnabkumardas.com/arduino-tutorial/i2c-register-description/
  Serial.println(F("Wiresetup debug 0"));
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // setup pin mode
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  pinMode(2, OUTPUT);             //2
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
  pinMode(A0, INPUT);      //A0
  pinMode(sig_mux_1, INPUT);             //A1
  pinMode(en_mux_1, OUTPUT);      //A2
  pinMode(q4_npn_ensh, OUTPUT);   //A3
  //hardware defined IIC pin      //A4
  //hardware defined IIC pin      //A5
  pinMode(A6, INPUT);             //A6
  pinMode(A7, INPUT);             //A7
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //initialize pins
  //  (seting mux pins HIGH so it not wastes energy leaking down when LOW)
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
  Serial.println(F("debug statement"));
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //init timer1 lib -- solenoids produce anoying beep sound wenn pulsed
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //  --> bring them to a higher frequency so the sound is less anoying (cant be heard above 20kHz)
  //EXAMPLE: (100) is 100 micro seconds = 10KHz
  Timer1.initialize(40);
  //for activate the PWM
  //  //Timer1.pwm(9, 512) gives 50% on pin D9
  //  Timer1.pwm(9, 830);
  Serial.println(F("debug statement"));
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //initialize bme280
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  if(!bme280.init()){
    Serial.println(F("bme device error!"));
  }

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //init time and date
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //uncomment if want to set the time
  //set_time(01,42,17,02,30,07+2,20);

  //seting time (second,minute,hour,weekday,date_day,date_month,year)
  //set_time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)
  //  sec,min,h,day_w,day_m,month,ear
  Serial.println(F("debug statement"));

  //initialize global time
  read_time(&sec_, &min_, &hour_, &day_w_, &day_m_, &mon_, &year_);
  Serial.println(F("debug 0"));
  delay(500);

  shiftvalue8b(0); //reset shiftregister to fixed state (all 0)
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//             MAIN LOOP
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NO INTERUPTS! while watering they can cause massive problems!
// solution would be to turn them off - for testing reasons i keep the step by step way



/*
unsigned long uptime(unsigned long uptime){
  byte sec, min, hour, day_w, day_m, mon , y;
  read_time(&sec, &min, &hour, &day_w, &day_m, &mon, &y);
  return 0L;
}
*/

void loop() {
  /*
  if(millis()< 20000UL){
    //delay(20000);
    read_time(&sec_, &min_, &hour_, &day_w_, &day_m_, &mon_, &year_);
  }
  */
  // --- main loop ---
  // put your main code here, to run repeatedly:
  digitalWrite(q4_npn_ensh, HIGH);  //activate shift register and set it all to zero
  shiftvalue8b(0);
  Serial.println(F("Start loop debug 1"));
  //delay(500);
  // --- init phase ---
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //check if RTC answers on IIC bus
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  byte rtc_status = Wire.endTransmission();
  if(rtc_status != 0){
    Serial.println(F("Error: rtc device not available!"));
  }

  Serial.println(F("enter init & check phase"));
  bool thirsty=false;
  //delay(200);
  byte sec1, min1, hour1, day_w1, day_m1, mon1 , y1;
  read_time(&sec1, &min1, &hour1, &day_w1, &day_m1, &mon1, &y1);
  if(hour_ != hour1){
  //if(true){
    up_time = up_time + (unsigned long)(60UL* 60UL * 1000UL); //refresh the up_time every hour, no need for extra function or lib to calculate the up time
    if((hour1 == 11) | (hour1 == 18)){
      thirsty = true;
    }
  }
  unsigned long actual_time = (unsigned long)up_time+(unsigned long)sec1*1000+(unsigned long)min1*60000UL;


  // --- datalog phase ---
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //saving data to SD, shape:
  //timestamp,pressure,temp,hum,analogdata(10 sensors+)
  //string cost ton of memory space!
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  Serial.print(F("Datalogphase: ")); Serial.println(actual_time-last_activation > measure_intervall);
  Serial.println(actual_time); Serial.println(last_activation); Serial.println(measure_intervall-500000UL);
  Serial.println((float)((float)actual_time-(float)last_activation)); Serial.println(up_time);
  //if((unsigned long)(actual_time-last_activation) > (unsigned long)(measure_intervall-500000UL)) //replace for debuging
  if((unsigned long)(actual_time-last_activation) > (unsigned long)(measure_intervall))
  //if(true)
  {
    last_activation = actual_time;
    Serial.println(F("enter datalog phase"));
    int value=0; //mux reading value
    // --- Datalog SD---
    digitalWrite(q4_npn_ensh, HIGH);  //activate shift register and set it all to zero
    shiftvalue8b(0);
    digitalWrite(q4_npn_ensh, LOW);  //deactivates shiftregister, so actuall measurement can happen

    digitalWrite(q2_npn_sens, HIGH); //activate sensors and give them some time
    delay(250);
    String data="";
    data += timestamp();
    data += ",";
    data += String(bme280.getPressure());
    data += ",";
    data += String(bme280.getTemperature());
    data += ",";
    data += String(bme280.getHumidity());
    data += ",";
    for(uint8_t i=0; i<sens_count; i++){
      int reading=0;
      controll_mux(mux, i, sig_mux_1, en_mux_1, "read", &value);
      delay(1); //was 3
      reading = value;
      data += reading;
      data += ",";
    }
    save_datalog(data,chip_select, "datalog2.txt");
    Serial.print(F("data:"));
    Serial.println(data);
    data=""; //clear string


    // --- Datalog INFLUXDB ---
    
    //delay(5000); //give some time to esp01 on iic bus (remove after testing)
    int data2[28] = {0}; //all sensors via mux plus bme readings and additional space
    data2[0] = (float)bme280.getPressure() / (float)10+0;
    data2[1] = (float)bme280.getHumidity() * (float)100+0;
    data2[2] = (float)bme280.getTemperature() * (float)100+0;
    data2[3] = readVcc();
    data2[4] = 0;
    data2[5] = 0;
    for(int i=0; i<sens_count; i++){
      controll_mux(mux, i, sig_mux_1, en_mux_1, "read", &value);
      delay(1); //was 3
      data2[6+i] = value;
    }
    delay(1000);
    digitalWrite(q2_npn_sens, LOW);

    digitalWrite(q4_npn_ensh, HIGH);  //activate shift register and set it all to zero
    shiftvalue8b(0);
    delay(5000); //was 5000
    TWBR=230; //make sure that IIC clock speed is low enough for esp01
    delay(500); //give some time to esp01 on iic bus
    //int tsize = sizeof data2;
    byte stat = transmitt(data2);
    if(stat != 0x00)
    {
      Serial.print(F("Transmission Error! Value: "));//transmissiion error
      Serial.println(stat);
    }
    else
    {
      Serial.println(F("Transmission OK!"));
    }
    delay(10);
  }
  
  // --- watering phase ---
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  
  //thirsty = true;  //uncomment if not wanted
  if(thirsty){
  Serial.println(F("enter watering phase"));
    // --- Watering ---
    //trigger at specific time (7am+) once per day
    //alternate the solenoids to avoid heat damage, let cooldown time if only one remains (cooldown = 90 sec!)
    //Hints: it can happen that solenoids work without delay for 60+60 sec should be no problem when it happens once
    //       main mosfet probably get warm (check that!)
    //       overall time should not go over 40 min while watering
    //       NEVER INTERUPT WHILE WATERING!!!!!!!!!!!!!!!!!!
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    int watering_base[6]={0};
    copy(group_st_time, watering_base, 6);
    uint8_t watering_done[6]= {1,1,1,1,1,1};
    Serial.println(F("watering while"));
    while(thirsty == true){
      //solenoids need time to cool down after 60 sek on time
      //controlling for loop to alternate the groups and work on the "todo watering"
      Serial.println(F("watering while inside"));
      for(uint8_t i=0; i<6; i++){
        int water_timer=0;
        uint8_t group = groups[i];
        Serial.println(group);
        Serial.print(F("watering base:")); Serial.println(watering_base[i]);
        delay(1000);
        if(watering_base[i]<1){
          water_timer=0;
          watering_done[i]=0;
          Serial.println(F("watering group finished"));
        }
        else if((watering_base[i]<60) & (watering_base[i]>0)){
          water_timer=watering_base[i];
          watering_base[i]=watering_base[i]-water_timer;
        }
        else{
          water_timer=60;
          watering_base[i]=watering_base[i]-60;
        }
        Serial.println(F("start watering"));
        //watering(uint8_t datapin, uint8_t clock, uint8_t latch, uint8_t time, uint8_t vent_pin, uint8_t pump_pin, uint8_t en, uint8_t pwm)
        if(water_timer > 60){
          //sanity check (should not be possible)
          water_timer = 0;
          Serial.println(F("Error: watering over 60 sec (should not happen)"));
        }
        //water_timer = 0;
        if(water_timer != 0){
          watering(s0_mux_1, s2_mux_1, s1_mux_1, water_timer, group, pump1, q4_npn_ensh, vent_pwm);
          Serial.print(F("cooldown time: ")); Serial.println(1000UL * water_timer / 2) + 5000;
          delay(1000UL * water_timer / 2+5000); //cooldown pump
        }
      }
      // check how many groups still need watering
      uint8_t sum = 0;
      for(int i=0; i<6; i++){
        //sum it up, could be done in the other for loop too
        sum += watering_done[i];
      }
      if(sum == 1){
        //trigger if only one group remains in the list
        //delay for 90 sec for cooldown on solenoids
        delay(90000UL);
      }
      if(sum < 1){
        //break the loop
        thirsty=false;
      }
      delay(60000UL); //cooldown
    }
  }

  delay(500);
  // --- end back to sleep ---
  ///////////////////////////////////////////////////////////////////////////
  //update global time variable
  read_time(&sec_, &min_, &hour_, &day_w_, &day_m_, &mon_, &year_);
  Serial.println(hour_);
  system_sleep();
  //sleep for about 10 min (80 iterations)
  Serial.println(F("enter sleep phase"));
  delay(100); //remove when not debuging
  for(int i = 0; i<40; i++){
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF); 
  }
}