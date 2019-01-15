///////////////////////////////////////////////////////////////////////////////////////
/*Terms of use
///////////////////////////////////////////////////////////////////////////////////////
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//THE SOFTWARE.


///////////////////////////////////////////////////////////////////////////////////////
//Support
///////////////////////////////////////////////////////////////////////////////////////
Website: http://www.brokking.net/imu.html
Youtube: https://youtu.be/4BoIE8YQwM8
Version: 1.0 (May 5, 2016)

///////////////////////////////////////////////////////////////////////////////////////
//Connections
///////////////////////////////////////////////////////////////////////////////////////
Power (5V) is provided to the Arduino pro mini by the FTDI programmer

Gyro - Arduino pro mini
VCC  -  5V
GND  -  GND
SDA  -  A4
SCL  -  A5

display.  - Arduino pro mini
VCC  -  5V
GND  -  GND
SDA  -  A4
SCL  -  A5
*//////////////////////////////////////////////////////////////////////////////////////

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <Adafruit_SSD1306.h>

//// Software SPI (slower updates, more flexible pin options):
//// pin 13 - Serial clock out (SCLK)
//// pin 11 - Serial data out (DIN)
//// pin 5 - Data/Command select (D/C)
//// pin 7 - display chip select (CS)
//// pin 6 - display reset (RST)

Adafruit_PCD8544 display = Adafruit_PCD8544(13, 11, 5, 7, 6);

//Declaring some global variables
int gyro_x, gyro_y, gyro_z;
long acc_x, acc_y, acc_z, acc_total_vector;
int temperature;
long gyro_x_cal, gyro_y_cal, gyro_z_cal;
long loop_timer, display_timer;
int lcd_loop_counter;
float angle_pitch, angle_roll;
int angle_pitch_buffer, angle_roll_buffer;
boolean set_gyro_angles;
float angle_roll_acc, angle_pitch_acc;
float angle_pitch_output, angle_roll_output;
int fontSizeHeight = 16;
int fontSizeWidth = 10;
int dataOffset = 8;
byte shadowPitchValues[4] = {0};
byte shadowRollValues[4] = {0};
char shadowPitchSign = '+';
char shadowRollSign = '+';

void setup_mpu_6050_registers(){
  //Activate the MPU-6050
  Wire.beginTransmission(0x68);                                        //Start communicating with the MPU-6050
  Wire.write(0x6B);                                                    //Send the requested starting register
  Wire.write(0x00);                                                    //Set the requested starting register
  Wire.endTransmission();                                              //End the transmission
  //Configure the accelerometer (+/-8g)
  Wire.beginTransmission(0x68);                                        //Start communicating with the MPU-6050
  Wire.write(0x1C);                                                    //Send the requested starting register
  Wire.write(0x10);                                                    //Set the requested starting register
  Wire.endTransmission();                                              //End the transmission
  //Configure the gyro (500dps full scale)
  Wire.beginTransmission(0x68);                                        //Start communicating with the MPU-6050
  Wire.write(0x1B);                                                    //Send the requested starting register
  Wire.write(0x08);                                                    //Set the requested starting register
  Wire.endTransmission();                                              //End the transmission
}

void setup_nokia_5110_lcd()
{
  pinMode(9, OUTPUT);                                                  // Turn on Nokia5110 LCD backlight
  digitalWrite(9, LOW);
  
  display.begin();
  display.setContrast(50);
  display.display();                                                   // Show splashscreen
  delay(3000);
  display.clearDisplay();                                              // Clear the screen and buffer

  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  display.setCursor(0,0);                                              //Set the display. cursor to position to position 0,0
  display.print("MPU-6050 IMU");                                       //Print text to screen
  display.setCursor(0,fontSizeHeight);                                 //Set the display. cursor to position to position 0,1
  display.print("V1.0");                                               //Print text to screen
  display.display();
  delay(3000);                                                         //Delay 1.5 second to display the text
  display.clearDisplay();                                              //Clear the display.
}

void calibrate_imu()
{
  display.setCursor(0,0);                                              //Set the display. cursor to position to position 0,0
  display.print("Calibrating: ");                                      //Print text to screen
  int count = 8;                    
  for (int cal_int = 0; cal_int < 2000 ; cal_int ++){                  //Run this code 2000 times
    if(cal_int % 250 == 0)
    { 
      display.setCursor(0,fontSizeHeight); 
      count--;                                                         //Set count on the display. Every 250 readings
      display.print(count);
      display.display();
    }
    read_mpu_6050_data();                                              //Read the raw acc and gyro data from the MPU-6050
    gyro_x_cal += gyro_x;                                              //Add the gyro x-axis offset to the gyro_x_cal variable
    gyro_y_cal += gyro_y;                                              //Add the gyro y-axis offset to the gyro_y_cal variable
    gyro_z_cal += gyro_z;                                              //Add the gyro z-axis offset to the gyro_z_cal variable
    delay(3);                                                          //Delay 3us to simulate the 250Hz program loop
  }
  gyro_x_cal /= 2000;                                                  //Divide the gyro_x_cal variable by 2000 to get the avarage offset
  gyro_y_cal /= 2000;                                                  //Divide the gyro_y_cal variable by 2000 to get the avarage offset
  gyro_z_cal /= 2000;                                                  //Divide the gyro_z_cal variable by 2000 to get the avarage offset
}

void prepare_lcd_data()
{
  display.clearDisplay();                                              //Clear the display.
  display.setCursor(0,0);                                              //Set the display. cursor to position to position 0,0
  display.print("Pitch:");                                             //Print Pitch text to screen
  display.setCursor(0,fontSizeHeight);                                 //Set the display. cursor to position to position 0,1
  display.print("Roll:");                                              //Print Roll text to screen
  display.display();
  loop_timer = micros();                                               //Reset the loop timer
}

void read_mpu_6050_data(){                                             //Subroutine for reading the raw gyro and accelerometer data
  Wire.beginTransmission(0x68);                                        //Start communicating with the MPU-6050
  Wire.write(0x3B);                                                    //Send the requested starting register
  Wire.endTransmission();                                              //End the transmission
  Wire.requestFrom(0x68,14);                                           //Request 14 bytes from the MPU-6050
  while(Wire.available() < 14);                                        //Wait until all the bytes are received
  acc_x = Wire.read()<<8|Wire.read();                                  //Add the low and high byte to the acc_x variable
  acc_y = Wire.read()<<8|Wire.read();                                  //Add the low and high byte to the acc_y variable
  acc_z = Wire.read()<<8|Wire.read();                                  //Add the low and high byte to the acc_z variable
  temperature = Wire.read()<<8|Wire.read();                            //Add the low and high byte to the temperature variable
  gyro_x = Wire.read()<<8|Wire.read();                                 //Add the low and high byte to the gyro_x variable
  gyro_y = Wire.read()<<8|Wire.read();                                 //Add the low and high byte to the gyro_y variable
  gyro_z = Wire.read()<<8|Wire.read();                                 //Add the low and high byte to the gyro_z variable
}

void setup() {
  Wire.begin();                                                        //Start I2C as master
  setup_mpu_6050_registers();                                          //Setup the registers of the MPU-6050 (500dfs / +/-8g) and start the gyro
  setup_nokia_5110_lcd();
  calibrate_imu();
  prepare_lcd_data();
}
void write_display(){                                                  //Subroutine for writing the display.
  
  //To get a 250Hz program loop (4us) it's only possible to write one character per loop
  //Writing multiple characters is taking to much time
  if(lcd_loop_counter == 14)
  {
    display.display();
    lcd_loop_counter = 0;                                             //Reset the counter after 14 characters
  }
  
  lcd_loop_counter++;                                                 //Increase the counter

  if(lcd_loop_counter == 1){
    angle_pitch_buffer = angle_pitch_output * 10;                     //Buffer the pitch angle because it will change
    display.setCursor(fontSizeWidth*dataOffset,0);                    //Set the LCD cursor to position of the data
  }
  
  if(lcd_loop_counter == 2){
      if(angle_pitch_buffer < 0)
      {
        shadowPitchSign = '-';                                        //Print - if value is negative
      }
      else
      {
        shadowPitchSign = '+';                                        //Print + if value is negative
      }
  
      display.print(shadowPitchSign);
  }

  if(lcd_loop_counter == 3)
  {
    shadowPitchValues[0] = abs(angle_pitch_buffer)/1000;
    display.print(shadowPitchValues[0]);                             //Print first number
  }
  if(lcd_loop_counter == 4)
  {
    shadowPitchValues[1] = (abs(angle_pitch_buffer)/100)%10;
    display.print(shadowPitchValues[1]);                            //Print second number
  }
  if(lcd_loop_counter == 5)
  {
    shadowPitchValues[2] = (abs(angle_pitch_buffer)/10)%10;
    display.print(shadowPitchValues[2]);                            //Print third number
  }
  if(lcd_loop_counter == 6)
  {
    display.print(".");                                             //Print decimal point
  }
  if(lcd_loop_counter == 7)
  {
    shadowPitchValues[3] = (abs(angle_pitch_buffer))%10;
    display.print(shadowPitchValues[3]);                            //Print fourth number
  }

  if(lcd_loop_counter == 8){
    angle_roll_buffer = angle_roll_output * 10;
    display.setCursor(fontSizeWidth*dataOffset,fontSizeHeight);     // Set the position of the cursor to the data position
  }

  if(lcd_loop_counter == 9){
    if(angle_roll_buffer < 0)
    {
      shadowRollSign = '-';                                         //Print - if value is negative
    }
    else
    {
      shadowRollSign = '+';                                         //Print + if value is negative
    }
    display.print(shadowRollSign);     
  }

  if(lcd_loop_counter == 10)
  {
    shadowRollValues[0] = abs(angle_roll_buffer)/1000; 
    display.print(shadowRollValues[0]);                             //Print first roll number
  }

  if(lcd_loop_counter == 11)
  {
    shadowRollValues[1] = (abs(angle_roll_buffer)/100)%10; 
    display.print(shadowRollValues[1]);                             //Print second number
  }
 
  if(lcd_loop_counter == 12)
  {
    shadowRollValues[2] = (abs(angle_roll_buffer)/10)%10; 
    display.print(shadowRollValues[2]);                             //Print third number
  }

  if(lcd_loop_counter == 13)
  {
    display.print(".");                                             //Print decimal point
  }
  
  if(lcd_loop_counter == 14)
  {
    shadowRollValues[3] = abs(angle_roll_buffer)%10; 
    display.print(shadowRollValues[3]);                             //Print fourth number
  }
}

void loop(){

  read_mpu_6050_data();                                                //Read the raw acc and gyro data from the MPU-6050

  gyro_x -= gyro_x_cal;                                                //Subtract the offset calibration value from the raw gyro_x value
  gyro_y -= gyro_y_cal;                                                //Subtract the offset calibration value from the raw gyro_y value
  gyro_z -= gyro_z_cal;                                                //Subtract the offset calibration value from the raw gyro_z value
  
  //Gyro angle calculations
  //0.0000611 = 1 / (250Hz / 65.5)
  angle_pitch += gyro_x * 0.0000611;                                   //Calculate the traveled pitch angle and add this to the angle_pitch variable
  angle_roll += gyro_y * 0.0000611;                                    //Calculate the traveled roll angle and add this to the angle_roll variable
  
  //0.000001066 = 0.0000611 * (3.142(PI) / 180degr) The Arduino sin function is in radians
  angle_pitch += angle_roll * sin(gyro_z * 0.000001066);               //If the IMU has yawed transfer the roll angle to the pitch angel
  angle_roll -= angle_pitch * sin(gyro_z * 0.000001066);               //If the IMU has yawed transfer the pitch angle to the roll angel
  
  //Accelerometer angle calculations
  acc_total_vector = sqrt((acc_x*acc_x)+(acc_y*acc_y)+(acc_z*acc_z));  //Calculate the total accelerometer vector
  //57.296 = 1 / (3.142 / 180) The Arduino asin function is in radians
  angle_pitch_acc = asin((float)acc_y/acc_total_vector)* 57.296;       //Calculate the pitch angle
  angle_roll_acc = asin((float)acc_x/acc_total_vector)* -57.296;       //Calculate the roll angle
  
  //Place the MPU-6050 spirit level and note the values in the following two lines for calibration
  angle_pitch_acc -= 0.0;                                              //Accelerometer calibration value for pitch
  angle_roll_acc -= 0.0;                                               //Accelerometer calibration value for roll

  if(set_gyro_angles){                                                 //If the IMU is already started
    angle_pitch = angle_pitch * 0.9996 + angle_pitch_acc * 0.0004;     //Correct the drift of the gyro pitch angle with the accelerometer pitch angle
    angle_roll = angle_roll * 0.9996 + angle_roll_acc * 0.0004;        //Correct the drift of the gyro roll angle with the accelerometer roll angle
  }
  else{                                                                //At first start
    angle_pitch = angle_pitch_acc;                                     //Set the gyro pitch angle equal to the accelerometer pitch angle 
    angle_roll = angle_roll_acc;                                       //Set the gyro roll angle equal to the accelerometer roll angle 
    set_gyro_angles = true;                                            //Set the IMU started flag
  }
  
  //To dampen the pitch and roll angles a complementary filter is used
  angle_pitch_output = angle_pitch_output * 0.9 + angle_pitch * 0.1;   //Take 90% of the output pitch value and add 10% of the raw pitch value
  angle_roll_output = angle_roll_output * 0.9 + angle_roll * 0.1;      //Take 90% of the output roll value and add 10% of the raw roll value
  
  write_display();                                                   //Write the roll and pitch values to the display. display

  while(micros() - loop_timer < 4000);                                 //Wait until the loop_timer reaches 4000us (250Hz) before starting the next loop
  loop_timer = micros();                                               //Reset the loop timer
}














