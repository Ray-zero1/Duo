#include <Wire.h>
#include <SD.h>
#include "TinyGPS++.h"

TinyGPSPlus gps;
//TinyGPSCustom magneticVariation(gps, "GPRMC", 10);

unsigned long timer;
//アドレス指定
#define BME280_ADDR 0x76
#define CONFIG 0xF5
#define CTRL_MEAS 0xF4
#define CTRL_HUM 0xF2
#define RX_pin 16
#define TX_pin 17
#define MPU6050_ACCEL_XOUT_H 0x3B  // R  
#define MPU6050_WHO_AM_I     0x75  // R
#define MPU6050_PWR_MGMT_1   0x6B  // R/W
#define MPU6050_I2C_ADDRESS  0x68
#define led_pin 13
#define input_pin 4

double SeaLevelPressure = 1016.8; //アメダスでその日の海面気圧を調べてください(hPa)
double altitude; 
//気温補正データ
uint16_t dig_T1;
int16_t  dig_T2;
int16_t  dig_T3;
 
//湿度補正データ
uint8_t  dig_H1;
int16_t  dig_H2;
uint8_t  dig_H3;
int16_t  dig_H4;
int16_t  dig_H5;
int8_t   dig_H6;
 
//気圧補正データ
uint16_t dig_P1;
int16_t  dig_P2;
int16_t  dig_P3;
int16_t  dig_P4;
int16_t  dig_P5;
int16_t  dig_P6;
int16_t  dig_P7;
int16_t  dig_P8;
int16_t  dig_P9;

unsigned char dac[26];
unsigned int i;

int32_t t_fine;
int32_t adc_P, adc_T, adc_H;
uint8_t buf[]={0xA5,0x5A,0x80,0x08,0x00,0xA0,0x13,0x01,0xFF,0x12,0x34,0x56,0x3D};

typedef union accel_t_gyro_union {
  struct {
    uint8_t x_accel_h;
    uint8_t x_accel_l;
    uint8_t y_accel_h;
    uint8_t y_accel_l;
    uint8_t z_accel_h;
    uint8_t z_accel_l;
    uint8_t t_h;
    uint8_t t_l;
    uint8_t x_gyro_h;
    uint8_t x_gyro_l;
    uint8_t y_gyro_h;
    uint8_t y_gyro_l;
    uint8_t z_gyro_h;
    uint8_t z_gyro_l;
  }
  reg;
  struct {
    int16_t x_accel;
    int16_t y_accel;
    int16_t z_accel;
    int16_t temperature;
    int16_t x_gyro;
    int16_t y_gyro;
    int16_t z_gyro;
  }
  value;
};

const char* f_name = "/test.txt";
File myFile;

int sum = 0;
int s = 0;
volatile int timeCounter1;
volatile int timeCounter2;
hw_timer_t *timer1 = NULL;    // For measurement
hw_timer_t *timer2 = NULL;    // For time adjustment
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
bool Flight = Serial.read();

void IRAM_ATTR onTimer1(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  timeCounter1++;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void IRAM_ATTR onTimer2(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  timeCounter2++;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void setup() {
   int error;
   uint8_t c;

  //シリアル通信初期化
  Serial.begin(115200);
  Serial2.begin(9600,SERIAL_8N1,RX_pin,TX_pin);
 
  //I2C初期化
  Wire.begin(21,22);//I2Cを初期化

  //BME280動作設定
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(CONFIG);//動作設定
  Wire.write(0x00);//「単発測定」、「フィルタなし」、「SPI 4線式
  Wire.endTransmission();
  //BME280測定条件設定
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(CTRL_MEAS);//測定条件設定
  Wire.write(0x24);//「温度・気圧オーバーサンプリングx1」、「スリープモード」
  Wire.endTransmission();

  //BME280温度測定条件設定
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(CTRL_HUM);//湿度測定条件設定
  Wire.write(0x01);//「湿度オーバーサンプリングx1」
  Wire.endTransmission();

  //BME280補正データ取得
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(0x88);//出力データバイトを「補正データ」のアドレスに指定
  Wire.endTransmission();
  
  Wire.requestFrom(BME280_ADDR, 26);//I2Cデバイス「BME280」に26Byteのデータ要求
  for (i=0; i<26; i++){
    while (Wire.available() == 0 ){}
    dac[i] = Wire.read();//dacにI2Cデバイス「BME280」のデータ読み込み
  }
  
  dig_T1 = ((uint16_t)((dac[1] << 8) | dac[0]));
  dig_T2 = ((int16_t)((dac[3] << 8) | dac[2]));
  dig_T3 = ((int16_t)((dac[5] << 8) | dac[4]));

  dig_P1 = ((uint16_t)((dac[7] << 8) | dac[6]));
  dig_P2 = ((int16_t)((dac[9] << 8) | dac[8]));
  dig_P3 = ((int16_t)((dac[11] << 8) | dac[10]));
  dig_P4 = ((int16_t)((dac[13] << 8) | dac[12]));
  dig_P5 = ((int16_t)((dac[15] << 8) | dac[14]));
  dig_P6 = ((int16_t)((dac[17] << 8) | dac[16]));
  dig_P7 = ((int16_t)((dac[19] << 8) | dac[18]));
  dig_P8 = ((int16_t)((dac[21] << 8) | dac[20]));
  dig_P9 = ((int16_t)((dac[23] << 8) | dac[22]));

  dig_H1 = ((uint8_t)(dac[25]));

  Wire.beginTransmission(BME280_ADDR);
  Wire.write(0xE1);//出力データバイトを「補正データ」のアドレスに指定
  Wire.endTransmission();
  
  Wire.requestFrom(BME280_ADDR, 7);//I2Cデバイス「BME280」に7Byteのデータ要求
  for (i=0; i<7; i++){
    while (Wire.available() == 0 ){}
    dac[i] = Wire.read();//dacにI2Cデバイス「BME280」のデータ読み込み
  }
  
  dig_H2 = ((int16_t)((dac[1] << 8) | dac[0]));
  dig_H3 = ((uint8_t)(dac[2]));
  dig_H4 = ((int16_t)((dac[3] << 4) + (dac[4] & 0x0F)));
  dig_H5 = ((int16_t)((dac[5] << 4) + ((dac[4] >> 4) & 0x0F)));
  dig_H6 = ((int8_t)dac[6]);

  error = MPU6050_read(MPU6050_WHO_AM_I, &c, 1);
  Serial.print("WHO_AM_I : ");
  Serial.print(c, HEX);
  Serial.print(", error = ");
  Serial.println(error, DEC);

  // 動作モードの読み出し
  error = MPU6050_read(MPU6050_PWR_MGMT_1, &c, 1);
  Serial.print("PWR_MGMT_1 : ");
  Serial.print(c, HEX);
  Serial.print(", error = ");
  Serial.println(error, DEC);
  
  // MPU6050動作開始
  MPU6050_write_reg(MPU6050_PWR_MGMT_1, 0); 
  
  delay(1000);//1000msec待機(1秒待機)
  Serial.println("Time\tPres\tTemp\tHum\tAlt\tA_x\tA_y\tA_z\tAan_x\tAan_y\tAan_z\tg_x\tg_y\tg_z\tLatitude\tLongitude\tAlt");
  SD.begin(5);
  myFile = SD.open(f_name,FILE_APPEND);
  myFile.println("Time\tPres\tTemp\tHum\tAlt\tA_x\tA_y\tA_z\tAan_x\tAan_y\tAan_z\tg_x\tg_y\tg_z\tLatitude\tLongitude\tAlt");
  myFile.close();

  timer1 = timerBegin(0, 80, true);
  timer2 = timerBegin(1, 80, true);
  timerAttachInterrupt(timer1, &onTimer1, true);  // Attach onTimer function.
  timerAttachInterrupt(timer2, &onTimer2, true);  // Attach onTimer function.
  timerAlarmWrite(timer1, 10000, true);  // Set alarm to call onTimer function every second (value in microseconds).
  timerAlarmWrite(timer2, 1000000, true);  // Set alarm to call onTimer function every second (value in microseconds).
  timerAlarmEnable(timer1);  // Start an alarm
  timerAlarmEnable(timer2);  // Start an alarm

}
 
void loop() {
   if (timeCounter1>0){ 
   portENTER_CRITICAL(&timerMux);
   timeCounter1--;
   portEXIT_CRITICAL(&timerMux);
   sum += 1;
   repeatCase();
   if(timeCounter2>0){
   portENTER_CRITICAL(&timerMux);
   timeCounter2--;
   portEXIT_CRITICAL(&timerMux);
   sum += 0.001;
   repeatCase();
  }


   }
}

void repeatCase(){
  int error;
  float dT;
  accel_t_gyro_union accel_t_gyro;
  int32_t  temp_cal;
  uint32_t humi_cal, pres_cal;
  float temp, humi, pres, altitude;
  SD.begin(5);
  myFile = SD.open(f_name,FILE_APPEND);
  //BME280測定条件設定(1回測定後、スリープモード)
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(CTRL_MEAS);//測定条件設定
  Wire.write(0x25);//「温度・気圧オーバーサンプリングx1」、「1回測定後、スリープモード」
  Wire.endTransmission();
  delay(10);//10msec待機

  //測定データ取得
  Wire.beginTransmission(BME280_ADDR);
  Wire.write(0xF7);//出力データバイトを「気圧データ」のアドレスに指定
  Wire.endTransmission();
  
  Wire.requestFrom(BME280_ADDR, 8);//I2Cデバイス「BME280」に8Byteのデータ要求
  for (i=0; i<8; i++){
    while (Wire.available() == 0 ){}
    dac[i] = Wire.read();//dacにI2Cデバイス「BME280」のデータ読み込み
  }
  
  
  adc_P = ((uint32_t)dac[0] << 12) | ((uint32_t)dac[1] << 4) | ((dac[2] >> 4) & 0x0F);
  adc_T = ((uint32_t)dac[3] << 12) | ((uint32_t)dac[4] << 4) | ((dac[5] >> 4) & 0x0F);
  adc_H = ((uint32_t)dac[6] << 8) | ((uint32_t)dac[7]);
  
  pres_cal = BME280_compensate_P_int32(adc_P);//気圧データ補正計算
  temp_cal = BME280_compensate_T_int32(adc_T);//温度データ補正計算
  humi_cal = bme280_compensate_H_int32(adc_H);//湿度データ補正計算

  pres = (float)pres_cal / 100.0;//気圧データを実際の値に計算
  temp = (float)temp_cal / 100.0;//温度データを実際の値に計算
  humi = (float)humi_cal / 1024.0;//湿度データを実際の値に計算
  altitude = 44330.0 * (1.0 - pow(pres / SeaLevelPressure, (1.0/5.255))); 
   // 加速度、角速度の読み出し
  // accel_t_gyroは読み出した値を保存する構造体、その後ろの引数は取り出すバイト数
  error = MPU6050_read(MPU6050_ACCEL_XOUT_H, (uint8_t *)&accel_t_gyro, sizeof(accel_t_gyro));

  // 取得できるデータはビッグエンディアンなので上位バイトと下位バイトの入れ替え（AVRはリトルエンディアン）
  uint8_t swap;
 #define SWAP(x,y) swap = x; x = y; y = swap
  SWAP (accel_t_gyro.reg.x_accel_h, accel_t_gyro.reg.x_accel_l);
  SWAP (accel_t_gyro.reg.y_accel_h, accel_t_gyro.reg.y_accel_l);
  SWAP (accel_t_gyro.reg.z_accel_h, accel_t_gyro.reg.z_accel_l);
  SWAP (accel_t_gyro.reg.t_h, accel_t_gyro.reg.t_l);
  SWAP (accel_t_gyro.reg.x_gyro_h, accel_t_gyro.reg.x_gyro_l);
  SWAP (accel_t_gyro.reg.y_gyro_h, accel_t_gyro.reg.y_gyro_l);
  SWAP (accel_t_gyro.reg.z_gyro_h, accel_t_gyro.reg.z_gyro_l);

  // 取得した加速度値を分解能で割って加速度(G)に変換する
  float acc_x = accel_t_gyro.value.x_accel / 16384.0; //FS_SEL_0 16,384 LSB / g
  float acc_y = accel_t_gyro.value.y_accel / 16384.0;
  float acc_z = accel_t_gyro.value.z_accel / 16384.0;

  // 加速度からセンサ対地角を求める
  float acc_angle_x = atan2(acc_x, acc_z) * 360 / 2.0 / PI;
  float acc_angle_y = atan2(acc_y, acc_z) * 360 / 2.0 / PI;
  float acc_angle_z = atan2(acc_x, acc_y) * 360 / 2.0 / PI;

  // 取得した角速度値を分解能で割って角速度(degrees per sec)に変換する
  float gyro_x = accel_t_gyro.value.x_gyro / 131.0;//FS_SEL_0 131 LSB / (°/s)
  float gyro_y = accel_t_gyro.value.y_gyro / 131.0;
  float gyro_z = accel_t_gyro.value.z_gyro / 131.0;
  
  myFile.print(sum);
  myFile.print("\t");
  myFile.print(pres);//「pres」をシリアルモニタに送信
  myFile.print("\t");
  myFile.print(temp);//「temp」をシリアルモニタに送信
  myFile.print("\t");//文字列「°C 」をシリアルモニタに送信
  myFile.print(humi);//「humi」をシリアルモニタに送信
  myFile.print("\t");//文字列「%」をシリアルモニタに送信、改行
  myFile.print(altitude);
  myFile.print("\t");
   
  myFile.print(acc_x, 2);
  myFile.print("\t");
  myFile.print(acc_y, 2);
  myFile.print("\t");
  myFile.print(acc_z, 2);
  myFile.print("\t");
  
  myFile.print(acc_angle_x, 2);
  myFile.print("\t");
  myFile.print(acc_angle_y, 2);
  myFile.print("\t");
  myFile.print(acc_angle_z, 2);
  myFile.print("\t");
  
  myFile.print(gyro_x, 2);
  myFile.print("\t");
  myFile.print(gyro_y, 2);
  myFile.print("\t");
  myFile.print(gyro_z, 2);
  myFile.print("\t");

  
  //シリアルモニタ送信
  Serial.print(sum);
  Serial.print("\t");
  Serial.print(pres);//「pres」をシリアルモニタに送信
  Serial.print("\t");
  Serial.print(temp);//「temp」をシリアルモニタに送信
  Serial.print("\t");//文字列「°C 」をシリアルモニタに送信
  Serial.print(humi);//「humi」をシリアルモニタに送信
  Serial.print("\t");//文字列「%」をシリアルモニタに送信、改行
  Serial.print(altitude);
  Serial.print("\t");
  
  Serial.print(acc_x, 2);
  Serial.print("\t");
  Serial.print(acc_y, 2);
  Serial.print("\t");
  Serial.print(acc_z, 2);
  Serial.print("\t");
  
  Serial.print(acc_angle_x, 2);
  Serial.print("\t");
  Serial.print(acc_angle_y, 2);
  Serial.print("\t");
  Serial.print(acc_angle_z, 2);
  Serial.print("\t");
  
  Serial.print(gyro_x, 2);
  Serial.print("\t");
  Serial.print(gyro_y, 2);
  Serial.print("\t");
  Serial.print(gyro_z, 2);
  Serial.print("\t");
  //Serial.print(c);
  while (Serial2.available() > 0){
  char c = Serial2.read();
  gps.encode(c);
  if (gps.location.isUpdated()){ 
    myFile.print(gps.location.lat(), 6);
    myFile.print("\t"); 
    myFile.print(gps.location.lng(), 6);
    myFile.print("\t"); 
    myFile.print(gps.altitude.meters());
    Serial.print(gps.location.lat(), 6);
    Serial.print("\t"); 
    Serial.print(gps.location.lng(), 6);
    Serial.print("\t"); 
    Serial.print(gps.altitude.meters());
  }
 }
 myFile.println("");
 myFile.close();
 Serial.println("");
}
//温度補正 関数
int32_t BME280_compensate_T_int32(int32_t adc_T)
{
  int32_t var1, var2, T;
  var1  = ((((adc_T>>3) - ((int32_t)dig_T1<<1))) * ((int32_t)dig_T2)) >> 11;
  var2  = (((((adc_T>>4) - ((int32_t)dig_T1)) * ((adc_T>>4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
  t_fine = var1 + var2;
  T  = (t_fine * 5 + 128) >> 8;
  return T;
}

//湿度補正 関数
uint32_t bme280_compensate_H_int32(int32_t adc_H)
{
  int32_t v_x1_u32r;

  v_x1_u32r = (t_fine - ((int32_t)76800)); 
  v_x1_u32r = (((((adc_H << 14) - (((int32_t)dig_H4) << 20) - (((int32_t)dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)dig_H6)) >> 10) * (((v_x1_u32r * ((int32_t)dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * 
((int32_t)dig_H2) + 8192) >> 14));
  v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)dig_H1)) >> 4));
  v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
  v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
  return (uint32_t)(v_x1_u32r>>12);
}

//気圧補正 関数
uint32_t BME280_compensate_P_int32(int32_t adc_P)
{
  int32_t var1, var2;
  uint32_t p;
  var1 = (((int32_t)t_fine)>>1) - (int32_t)64000;
  var2 = (((var1>>2) * (var1>>2)) >> 11 ) * ((int32_t)dig_P6);
  var2 = var2 + ((var1*((int32_t)dig_P5))<<1);
  var2 = (var2>>2)+(((int32_t)dig_P4)<<16);
  var1 = (((dig_P3 * (((var1>>2) * (var1>>2)) >> 13 )) >> 3) + ((((int32_t)dig_P2) * var1)>>1))>>18;
  var1 =((((32768+var1))*((int32_t)dig_P1))>>15);
  if (var1 == 0)
  {
    return 0; // avoid exception caused by division by zero
  }
  p = (((uint32_t)(((int32_t)1048576)-adc_P)-(var2>>12)))*3125;
  if (p < 0x80000000)
  {
    p = (p << 1) / ((uint32_t)var1);
  }
  else
  {
    p = (p / (uint32_t)var1) * 2;
  }
  var1 = (((int32_t)dig_P9) * ((int32_t)(((p>>3) * (p>>3))>>13)))>>12;
  var2 = (((int32_t)(p>>2)) * ((int32_t)dig_P8))>>13;
  p = (uint32_t)((int32_t)p + ((var1 + var2 + dig_P7) >> 4));
  return p;
            for(uint8_t i = 0; i < sizeof(buf); i++ ){
                Serial.write(buf[i]); //バイナリで送信するときはwrite
            }
            Serial.println(); //改行を入れたい

    

    while(Serial.available()){ //TWELITEからの受信待ち
        Serial.write(Serial.read());
    }
 
}

// MPU6050_read
int MPU6050_read(int start, uint8_t *buffer, int size) {
  int i, n, error;
  Wire.beginTransmission(MPU6050_I2C_ADDRESS);
  n = Wire.write(start);
  if (n != 1) {
    return (-10);
  }
  n = Wire.endTransmission(false);// hold the I2C-bus
  if (n != 0) {
    return (n);
  }
  // Third parameter is true: relase I2C-bus after data is read.
  Wire.requestFrom(MPU6050_I2C_ADDRESS, size, true);
  i = 0;
  while (Wire.available() && i < size) {
    buffer[i++] = Wire.read();
  }
  if ( i != size) {
    return (-11);
  }
  return (0); // return : no error
}

// MPU6050_write
int MPU6050_write(int start, const uint8_t *pData, int size) {
  int n, error;
  Wire.beginTransmission(MPU6050_I2C_ADDRESS);
  n = Wire.write(start);// write the start address
  if (n != 1) {
    return (-20);
  }
  n = Wire.write(pData, size);// write data bytes
  if (n != size) {
    return (-21);
  }
  error = Wire.endTransmission(true); // release the I2C-bus
  if (error != 0) {
    return (error);
  }

  return (0);// return : no error
}

// MPU6050_write_reg
int MPU6050_write_reg(int reg, uint8_t data) {
  int error;
  error = MPU6050_write(reg, &data, 1);
  Serial.print("error = ");
  Serial.println(error);
  return (error);
};
