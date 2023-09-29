#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <Servo.h>
#include <FastLED.h>

// Constants
const int DS18B20_PIN = 2;
const int TURBIDITY_PIN = A0;
const int RELAY_PUMP_PIN = 6;
const int RELAY_SOLENOID_PIN = 4;
const int RELAY_HEATER_PIN = 5;
const int WATER_LEVEL_PIN = A1;
const int LED_STRIP_PIN = 9;
const int FEEDER_SERVO_PIN = 10;
// const int NP_LIGHT_PIN = 8;


//Number of Times Feeder Running
const int FEEDER_SERVO_TIMES = 3;

//System(Internal) Temprerature Threshold
const int INT_TEMP_TRESHOLD = 45;

// LCD Display
const int LCD_ADDRESS = 0x3F;  //3F 5F 68 57
const int LCD_COLUMNS = 20;
const int LCD_ROWS = 4;
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

// Temperature Sensor
//Initializing the Sensor
OneWire oneWire(DS18B20_PIN);
DallasTemperature temperatureSensor(&oneWire);
// Setting Temperature Threshold and Buffer
const int TEMP_THRESHOLD = 30;
const int TEMP_BUFFER = 2;

// Turbidity Sensor
// Setting Turbidity Sensor Max reading and Min Reading
const int TURBIDITY_SENSOR_READING_MIN = 750;
const int TURBIDITY_SENSOR_READING_MAX = 0;
//Setting the LOW MEDIUM and HIGH Values for Turbidity in NTU
const int TURBIDITY_THRESHOLD_LOW = 10;     
const int TURBIDITY_THRESHOLD_MEDIUM = 50;  
const int TURBIDITY_THRESHOLD_BAD = 75;  

//Sensor Correction
const double SENSOR_CORRECTION = 0.6;

// //Setting the LOW MEDIUM and HIGH Values after mapping 0-100
// const int TURBIDITY_THRESHOLD_LOW = 10;     
// const int TURBIDITY_THRESHOLD_MEDIUM = 50;  
// const int TURBIDITY_THRESHOLD_BAD = 75;   

// Water Level Sensor
const int WATER_LEVEL_THRESHOLD = 350;
const int WATER_LEVEL_BUFFER = 100;
const int WATER_LEVEL_MAX = 500;
const int WATER_LEVEL_MIN = 50;

// LED Strip
const int LED_COUNT = 27;
//Setting LED Brighyness 0 - 255
const int BRIGHTNESS = 75;
//Initializing the Stripe
CRGB leds[LED_COUNT];

// RTC Module
RTC_DS3231 rtc;  // Use the RTC_DS3231 class from RTClib

// Servo for Fish Feeder
Servo feederServo;

// Variables
int WaterLevel = 0;
int FishFeederCount = 0;
bool m_p = false;
bool m_s = false;
bool rtc_status = true;
bool is_int_temp_high = false;
double volt;
double ntu;

// Function prototypes
void printTemperature(float temperature);
void printTurbidityLevel(int turbidityLevel);
void printTurbidityLevel_ntu(int turbidityLevel);
void controlPump(bool on_p);
void controlSolenoidValve(bool on_v);
void controlHeater(bool on_h);
void checkWaterLevel();
void checkFeedingTime();
void checkRTCStatus();
void maintainWaterLevel();
void ledstrip(int turbidityLevel);
void printwaterstatus(int turbidityLevel);
void setRTCDateTime(int year, int month, int day, int hour, int minute, int second);
void checkInternalTemp();
void debuggingPrint(int waterLevel, int turbidityLevel_row, int turbidityLevel);
float round_to_dp( float in_value, int decimal_place );
float readturbidity_ntu();
int readturbidity();
// void controlLight();

void setup() {
  //Debugging
  Serial.begin(9600);

  // Initialize the RTC module
  rtc.begin();
  // Uncomment the following line to set the date and time (year, month, day, hour, minute, second)
  // setRTCDateTime(2023, 6, 22, 12, 30, 0);

  //Stating the LCD Screen and Setting the Backlight
  lcd.init();
  lcd.backlight();

  //Starting the Temperature Sensor
  temperatureSensor.begin();
  temperatureSensor.setResolution(12);  // Adjust resolution as per your needs

  //Setting up LED Strip
  FastLED.addLeds<WS2812, LED_STRIP_PIN, GRB>(leds, LED_COUNT);  // Define LED strip type and pin
  FastLED.setBrightness(BRIGHTNESS);                             // Set initial brightness
  fill_solid(leds, LED_COUNT, CRGB::White);
  FastLED.show();

  //Setting up Servo Motor
  feederServo.attach(FEEDER_SERVO_PIN);

  //Initializing I2C Communication in Arduino
  Wire.begin();

  // Print initial status on LCD
  lcd.setCursor(0, 0);
  lcd.print("Fish Tank System");
  lcd.setCursor(0, 1);
  lcd.print("Temp:");
  lcd.setCursor(0, 2);
  lcd.print("Turbidity:");

  // Check and display RTC status
  checkRTCStatus();

  // Setup waterlevel and turdity pins as input pins
  pinMode(WATER_LEVEL_PIN, INPUT);
  pinMode(TURBIDITY_PIN, INPUT);

  //Setup Relay
  pinMode(RELAY_PUMP_PIN, OUTPUT);
  pinMode(RELAY_SOLENOID_PIN, OUTPUT);
  pinMode(RELAY_HEATER_PIN, OUTPUT);

  // //Setup Light
  // pinMode(NP_LIGHT_PIN, OUTPUT);

}

void loop() {

  // Read the analog value from the sensor
  int waterLevel = analogRead(WATER_LEVEL_PIN);

  // Read and print turbidity level and Display Message if Turbidity is High
   //Reading the direct sensor value
  int turbidityLevel_row = analogRead(TURBIDITY_PIN);
  // //Map Turbidity Values to 0 - 100
  // int turbidityLevel = readturbidity(); //Mapping turbidity to 0 - 100
  // printTurbidityLevel(turbidityLevel);

  //Get turbidiy and covert it to NTU Value
  float turbidityLevel = readturbidity_ntu();
  printTurbidityLevel_ntu(turbidityLevel);

  //Display Status of the water
  printwaterstatus(turbidityLevel);

  //Contral LED Stripe
  ledstrip(turbidityLevel);

  // Read and print temperature
  temperatureSensor.requestTemperatures();
  int temperature = temperatureSensor.getTempCByIndex(0);
  printTemperature(temperature);

  // Control pump and solenoid valve based on temperature
  if (temperature > TEMP_THRESHOLD + TEMP_BUFFER) {
    //If temperature is High remove water from tank and add new water while maintaining the water level
    maintainWaterLevel();
    controlHeater(false);  // Turn off heater if temperature is High

  } else if (temperature < TEMP_THRESHOLD - TEMP_BUFFER) {
    controlHeater(true);          // Turn off heater if temperature is Normal
    controlSolenoidValve(false);  // Turn off solenoid valve if water level is normal
    controlPump(false);           // Turn off pump if water level is normal

  } else {
    // if Temperature is Normal Check and maintain waterlevel
    checkWaterLevel();
    controlHeater(false);  // Turn off heater if temperature is Normal
  }


  // Check feeding time
  checkFeedingTime();

  // // Control Light
  // controlLight();

  //calibrating and debigging
  debuggingPrint(waterLevel, turbidityLevel_row, turbidityLevel);

  delay(3000);  // Delay for three second before repeating the loop
}

//Read Turbidty sensor input and convert to NTU value
float readturbidity_ntu(){
  volt = 0;
    //Getting 800 samples and Taking the medium
    for(int i=0; i<800; i++)
    {
        volt += ((double)analogRead(TURBIDITY_PIN)/1023)*5; 
    }

    volt = volt/800; 

    volt = volt + SENSOR_CORRECTION; //Sensor Correction

    if(volt < 2.0){
      ntu = 3000;

    }else{
      ntu = -1120.4*pow(volt,2)+5742.3*volt-4353.8; //Sesnsor Characteristic Equation

    }

    if(ntu < 0){
      ntu = 0;
    }
    
  return ntu;
}

//Read Turbidity sensor output and mapping the vaalues to 0 - 100
int readturbidity(){
  //Reading the direct sensor value
  int turbidityLevel_row = analogRead(TURBIDITY_PIN);
  int turbidityLevel = map(turbidityLevel_row, TURBIDITY_SENSOR_READING_MAX, TURBIDITY_SENSOR_READING_MIN, 100, 0);

  if(turbidityLevel <= 0){
    turbidityLevel = 0;
  }

  return turbidityLevel;
}

//Print Sensor Inputs to Serial Monitor for Calibrating and Debugging Purposes
void debuggingPrint(int waterLevel, int turbidityLevel_row, int turbidityLevel) {

  Serial.print("Water level: ");
  Serial.println(waterLevel);

  Serial.print("Turbidity level(Raw Data): ");
  Serial.println(turbidityLevel_row);

  Serial.print("Turbidity Volt(corrected) ");
  Serial.println(volt,6);

  Serial.print("Turbidity Volt ");
  Serial.println((volt - SENSOR_CORRECTION),6);

  // Print the date and time to the serial monitor
  DateTime now = rtc.now();  // Get current date and time
  Serial.print("Current Date: ");
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" | ");
  Serial.print("Current Time: ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();

  int internal_temp = rtc.getTemperature();
  Serial.print("System Temperature ");
  Serial.println(internal_temp);
}

void printTemperature(float temperature) {
  lcd.setCursor(6, 1);
  lcd.print("             ");
  lcd.setCursor(6, 1);
  lcd.print(temperature);
  lcd.print("C");
}

void printTurbidityLevel_ntu(int turbidityLevel) {
  lcd.setCursor(11, 2);
  lcd.print("        ");
  lcd.setCursor(11, 2);
  lcd.print(turbidityLevel);
  lcd.print(" NTU");
}

void printTurbidityLevel(int turbidityLevel) {
  lcd.setCursor(11, 2);
  lcd.print("        ");
  lcd.setCursor(11, 2);
  lcd.print(turbidityLevel);
  lcd.print("%");
}

void printwaterstatus(int turbidityLevel) {

  //If RTC is Working and System Temperature is okay then Print water Status
  if ((rtc_status == true) || (is_int_temp_high == false)) {
    lcd.setCursor(0, 3);
    lcd.print("                    ");

    if (turbidityLevel < TURBIDITY_THRESHOLD_LOW) {
      lcd.setCursor(0, 3);
      lcd.print("Water Status: Good");

    } else if (turbidityLevel < TURBIDITY_THRESHOLD_MEDIUM) {
      lcd.setCursor(0, 3);
      lcd.print("Water Status: Normal");

    } else {
      lcd.setCursor(0, 3);
      lcd.print("Change Water!!!");
    }
  } else {
  }
}

void ledstrip(int turbidityLevel_ntu) {

  if(turbidityLevel_ntu <= TURBIDITY_THRESHOLD_LOW){
    int bulbs_on = map(turbidityLevel_ntu, 0, TURBIDITY_THRESHOLD_LOW, 0, LED_COUNT/3);

    for (int i = 0; i < LED_COUNT; i++) {

      if (i <= bulbs_on) {
          leds[i] = CRGB(0, 255, 0); 

      } else {
        leds[i] = CRGB::Black;
      }
  }

  }else if(turbidityLevel_ntu <= TURBIDITY_THRESHOLD_MEDIUM){
    int bulbs_on = map(turbidityLevel_ntu, 0, TURBIDITY_THRESHOLD_MEDIUM, 0, 2*LED_COUNT/3);

    for (int i = 0; i < LED_COUNT; i++) {

      if (i <= bulbs_on) {

        if (i <= LED_COUNT / 3) {
          leds[i] = CRGB(0, 255, 0);

        } else {
          leds[i] = CRGB(255, 255, 0);
        }

      } else {
        leds[i] = CRGB::Black;
      }
    }

  }else{
    int bulbs_on = map(turbidityLevel_ntu, 0, TURBIDITY_THRESHOLD_BAD, 0, LED_COUNT);

    for (int i = 0; i < LED_COUNT; i++) {

      if (i <= bulbs_on) {

        if (i <= LED_COUNT / 3) {
          leds[i] = CRGB(0, 255, 0);

        } else if (i <= (2 * LED_COUNT) / 3) {
          leds[i] = CRGB(255, 255, 0);

        } else {
          leds[i] = CRGB(255, 0, 0);
        }

      } else {
        leds[i] = CRGB::Black;
      }
    }

  }

  FastLED.show();
}

//Controll Pump
void controlPump(bool on_p) {

  if (!on_p) {
    digitalWrite(RELAY_PUMP_PIN, HIGH);

  } else {
    digitalWrite(RELAY_PUMP_PIN, LOW);
  }
}

//Control Solonoid Valve
void controlSolenoidValve(bool on_v) {

  if (!on_v) {
    digitalWrite(RELAY_SOLENOID_PIN, HIGH);

  } else {
    digitalWrite(RELAY_SOLENOID_PIN, LOW);
  }
}

//Controll Heater
void controlHeater(bool on_h) {

  if (!on_h) {
    digitalWrite(RELAY_HEATER_PIN, HIGH);

  } else {
    digitalWrite(RELAY_HEATER_PIN, LOW);
  }
}


//Repeatly doing the water removing and water adding to the tank while maintaing the water level
//This funtion use to remove old water from the tank and add fress water while maintaing the water level
void maintainWaterLevel() {

  WaterLevel = analogRead(WATER_LEVEL_PIN);

  //If Water Level came to min level of the sensor turn on valve and off the pump
  if (WaterLevel <= (WATER_LEVEL_MIN + WATER_LEVEL_BUFFER)) {
    m_p = false;
    m_s = true;
    controlSolenoidValve(true);
    controlPump(false);

    //If Water Level came to max level of the sensor turn on pump and off the valve
  } else if (WaterLevel > (WATER_LEVEL_MAX - WATER_LEVEL_BUFFER)) {
    m_p = true;
    m_s = false;
    controlSolenoidValve(false);
    controlPump(true);

  } else {
    //If both pump and valve is currently off turn on both
    if (m_p == false && m_s == false) {
      controlSolenoidValve(true);
      controlPump(true);
    }
  }
}


//Maintain Regular Water Level
void checkWaterLevel() {
  WaterLevel = analogRead(WATER_LEVEL_PIN);

  // Turn on solenoid valve if water level is low
  if (WaterLevel < (WATER_LEVEL_THRESHOLD - WATER_LEVEL_BUFFER)) {
    controlSolenoidValve(true);
    controlPump(false);

  } else if (WaterLevel > (WATER_LEVEL_THRESHOLD + WATER_LEVEL_BUFFER)) {
    // Turn on pump if water level is High
    controlSolenoidValve(false);
    controlPump(true);

  } else {
    // Turn off pump and valve if water level is Normal
    controlSolenoidValve(false);
    controlPump(false);
  }
}

// //Contral the Tank Light
// void controlLight() {

//   DateTime now = rtc.now();
//   int currentHour = now.hour();

//   //Turn of Tank Light from 6pm to 6am
//   if ((currentHour >= 18 && currentHour <= 24) || (currentHour >= 0 && currentHour <= 6)) {
//     digitalWrite(NP_LIGHT_PIN, HIGH);

//   } else {
//     digitalWrite(NP_LIGHT_PIN, LOW);
//   }
// }

//Feeder Funtion, rotate feeder if time is correct
void checkFeedingTime() {
  DateTime now = rtc.now();  // Get current date and time
  int currentHour = now.hour();

  // Check if it's feeding time
  if (currentHour == 7 || currentHour == 12 || currentHour == 18) {

    if (FishFeederCount <= FEEDER_SERVO_TIMES) {
      feederServo.write(180);  // Turn on fish feeder
      delay(2000);             // Wait for 2 seconds
      feederServo.write(0);    // Turn off fish feeder
      FishFeederCount = FishFeederCount + 1;
    }

  } else {
    FishFeederCount = 0;
  }
}

//Funtion to Set and Initialize the Date and Time to RTC
void setRTCDateTime(int year, int month, int day, int hour, int minute, int second) {

  DateTime dt(year, month, day, hour, minute, second);
  rtc.adjust(dt);
  Serial.println("Date and time set successfully!");
}

//Check the system temperature by using the Temperature sensor if RTC and Print warning and Sound alarm if Temp is High
void checkInternalTemp() {

  int internal_temp = rtc.getTemperature();

  if (internal_temp > INT_TEMP_TRESHOLD) {
    is_int_temp_high = true;

    //Print Warning
    lcd.setCursor(0, 3);
    lcd.print("                    ");
    lcd.setCursor(0, 3);
    lcd.print("System Temp is High!!");
  }else{
    is_int_temp_high = false;
  }  
}

//Funtion to Check Wheter RTC is running or Not and Check Battery is dead or Not
void checkRTCStatus() {
  lcd.setCursor(0, 3);

  if (!rtc.begin()) {  // Check if RTC module is connected
    lcd.setCursor(0, 3);
    lcd.print("                    ");
    lcd.setCursor(0, 3);
    lcd.print("Feeder not working");
    rtc_status = false;

  } else {
    DateTime now = rtc.now();  // Get current date and time

    // Check if RTC module is running by checking the year (Checking the battery of RTC is working or Not)
    if (now.year() < 2020) {
      lcd.setCursor(0, 3);
      lcd.print("                    ");
      lcd.setCursor(0, 3);
      lcd.print("Feeder not working");

      rtc_status = false;

    } else {
      rtc_status = true;
    }
  }
}
