#ifndef CONFIG_H
#define CONFIG_H

#define DS3231_I2C_ADDRESS 0x68 //adress bme280 sensor
#define MAX_MSG_LEN 128

#define sw_sens 4 //switch mux, SD etc.
#define en_mux_1 5 //mux enable
#define chip_select 15 //SPI cs pin
#define s3_mux_1 16 //mux controll pin & s3_mux_1
#define s2_mux_1 17 //mux controll pin & s2_mux_1
#define s1_mux_1 18 //mux controll pin & s1_mux_1
#define s0_mux_1 19 //mux controll pin & s0_mux_1
//hardware defined IIC pin     SDA    GPIO21()
//hardware defined IIC pin     SCL    GPIO22()
#define sw_3_3v 23 //switch 3.3v output (with shift bme, rtc)
#define sw_sens2 25 //switch sensor rail IMPORTANT: PIN moved to 25 layout board 1 v3 !!!!!
#define st_cp_shft 26 //74hc595 ST_CP LATCH pin
#define sh_cp_shft 27 //74hc595 SH_CP CLK pin
#define vent_pwm 32 //vent pwm output
#define data_shft 33 //74hc595 Data
#define sig_mux_1 39 //mux sig in

#define max_groups 6 //number of plant groups
#define measure_intervall 600000UL // value/1000 ==> seconds
#define pwm_ch0 0 //pwm channel
#define pwm_ch0_res 8 //pwm resolution in bit (1-15)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//             default static variable definitions & virtual pins (shift register)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//String dummy[100]; //approx data measurment string
// virtual pins (shift register)
#define pump1 (uint8_t)7
#define pump2 (uint8_t)6
uint8_t groups[6] = {0,1,2,3,4,5};
uint8_t mux[4] = {s0_mux_1,s1_mux_1,s2_mux_1,s3_mux_1};
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

#endif