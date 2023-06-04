#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_NeoPixel.h>
#include <RTClib.h>
#include <Servo.h>

// Constants
const int DS18B20_PIN = 2;
const int TURBIDITY_PIN = A0;
const int RELAY_PUMP_PIN = 3;
const int RELAY_SOLENOID_PIN = 4;
const int RELAY_BUZZER_PIN = 5;
const int RELAY_WATER_LEVEL_PIN = 6;
const int BUZZER_PIN = 8;
const int WATER_LEVEL_PIN = A1;
const int LED_STRIP_PIN = 9;
const int FEEDER_SERVO_PIN = 10;
const int FEEDER_SERVO_TIMES = 10;


// LCD Display
const int LCD_ADDRESS = 0x27;
const int LCD_COLUMNS = 20;
const int LCD_ROWS = 4;
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

// Temperature Sensor
OneWire oneWire(DS18B20_PIN);
DallasTemperature temperatureSensor(&oneWire);
const int TEMP_THRESHOLD = 25;

// Turbidity Sensor
const int TURBIDITY_THRESHOLD_LOW = 500;     // Adjust threshold values as per your needs
const int TURBIDITY_THRESHOLD_MEDIUM = 1000; // Adjust threshold values as per your needs

// Water Level Sensor
const float WATER_LEVEL_THRESHOLD = 500;
const float WATER_LEVEL_BUFFER = 100;       // Adjust threshold values as per your needs
const float WATER_LEVEL_MAX = 700;
const float WATER_LEVEL_MIN = 100;

// LED Strip
const int LED_COUNT = 1;
Adafruit_NeoPixel ledStrip(LED_COUNT, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);

// RTC Module
RTC_DS3231 rtc;  // Use the RTC_DS3231 class from RTClib

// Servo for Fish Feeder
Servo feederServo;

// Variables
float WaterLevel = 0;
int FishFeederCount = 0;
bool m_p = false;
bool m_s = false;
bool rtc_status = true;

// Function prototypes
void printTemperature(float temperature);
void printTurbidityLevel(int turbidityLevel);
void controlPump(bool on);
void controlSolenoidValve(bool on);
void checkWaterLevel();
void checkFeedingTime();
void checkRTCStatus();
void maintainWaterLevel();
void ledstrip(int turbidityLevel);
void printwaterstatus(int turbidityLevel);

void setup() {
  lcd.begin(LCD_COLUMNS, LCD_ROWS);
  lcd.setBacklight(LOW);

  temperatureSensor.begin();
  temperatureSensor.setResolution(12);  // Adjust resolution as per your needs

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(WATER_LEVEL_PIN, INPUT);

  ledStrip.begin();
  ledStrip.show();  // Turn off LED strip

  feederServo.attach(FEEDER_SERVO_PIN);

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
}

void loop() {

  // Read and print turbidity level and Display Message if Turbidity is High
  int turbidityLevel = analogRead(TURBIDITY_PIN);
  printTurbidityLevel(turbidityLevel);

  //Display Status of the water
  printwaterstatus(turbidityLevel);

  //Contral LED Stripe
  ledstrip(turbidityLevel);

  // Read and print temperature
  temperatureSensor.requestTemperatures();
  float temperature = temperatureSensor.getTempCByIndex(0);
  printTemperature(temperature);

  // Control pump and solenoid valve based on temperature
  if (temperature > TEMP_THRESHOLD) {
    maintainWaterLevel();

  } else {
    // Check water level and control water level relay
    checkWaterLevel();
  }

  // Check feeding time
  checkFeedingTime();

  delay(1000);  // Delay for one second before repeating the loop
}

void printTemperature(float temperature) {
  lcd.setCursor(6, 1);
  lcd.print("             ");
  lcd.setCursor(6, 1);
  lcd.print(temperature);
  lcd.print("C");
}

void printTurbidityLevel(int turbidityLevel) {
  lcd.setCursor(11, 2);
  lcd.print("        ");
  lcd.setCursor(11, 2);
  lcd.print(turbidityLevel);
}

void printwaterstatus(int turbidityLevel){
  if(rtc_status == true){
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
  }else{

  }
  
}

void ledstrip(int turbidityLevel){
  if (turbidityLevel < TURBIDITY_THRESHOLD_LOW) {
    // Low turbidity level, turn LED strip green
    ledStrip.setPixelColor(0, 0, 255, 0);
  } else if (turbidityLevel < TURBIDITY_THRESHOLD_MEDIUM) {
    // Medium turbidity level, turn LED strip yellow
    ledStrip.setPixelColor(0, 255, 255, 0);
  } else {
    // High turbidity level, turn LED strip red
    ledStrip.setPixelColor(0, 255, 0, 0);
  }

  ledStrip.show();

}

void controlPump(bool on_p) {
  if(on_p){
    digitalWrite(RELAY_PUMP_PIN,HIGH);
  }else{
    digitalWrite(RELAY_PUMP_PIN,LOW);
  }
  
}

void controlSolenoidValve(bool on_v) {
  if(on_v){
    digitalWrite(RELAY_SOLENOID_PIN,HIGH);
  }else{
    digitalWrite(RELAY_SOLENOID_PIN,LOW);
  }
}

void maintainWaterLevel() {
  WaterLevel = analogRead(WATER_LEVEL_PIN);

  if (WaterLevel <= (WATER_LEVEL_MIN + WATER_LEVEL_BUFFER)){
    m_p = false;
    m_s = true;
    controlSolenoidValve(true);  // Turn on solenoid valve if water level is low
    controlPump(false);
  }else if(WaterLevel > (WATER_LEVEL_MAX - WATER_LEVEL_BUFFER)){
    m_p = true;
    m_s = false;
    controlSolenoidValve(false); // Turn off solenoid valve if water level is normal
    controlPump(true);
  }
   else {
     if(m_p == false && m_s == false){
      controlSolenoidValve(true);
      controlPump(true);
     }
  }
}

void checkWaterLevel() {
  WaterLevel = analogRead(WATER_LEVEL_PIN);

  if (WaterLevel < (WATER_LEVEL_THRESHOLD - WATER_LEVEL_BUFFER)) {
    controlSolenoidValve(true);  // Turn on solenoid valve if water level is low
  }else if(WaterLevel > (WATER_LEVEL_THRESHOLD + WATER_LEVEL_BUFFER)){
    controlSolenoidValve(false); // Turn off solenoid valve if water level is normal
    controlPump(true);
  }
   else {
    controlSolenoidValve(false); // Turn off solenoid valve if water level is normal
    controlPump(false);
  }
}

void checkFeedingTime() {
  DateTime now = rtc.now();  // Get current date and time
  int currentHour = now.hour();

  // Check if it's feeding time (twice a day)
  if (currentHour == 8 || currentHour == 16) {
    if(FishFeederCount <= FEEDER_SERVO_TIMES){
      feederServo.write(180); // Turn on fish feeder
      delay(2000);           // Wait for 2 seconds
      feederServo.write(0);   // Turn off fish feeder
      FishFeederCount = FishFeederCount + 1;
    }
  }else{
    FishFeederCount = 0;
  }
}

void checkRTCStatus() {
  lcd.setCursor(0, 3);

  if (!rtc.begin()) {  // Check if RTC module is connected
    lcd.setCursor(0, 3);
    lcd.print("                    ");
    lcd.setCursor(0, 3);
    lcd.print("RTC Not Found");
    rtc_status = false;
  } else {
    DateTime now = rtc.now();  // Get current date and time
    if (now.year() < 2020) {  // Check if RTC module is running by checking the year
      lcd.setCursor(0, 3);
      lcd.print("                    ");
      lcd.setCursor(0, 3);
      lcd.print("RTC Not Running");
      rtc_status = false;
    } else {
      rtc_status = true;
      
    }
  }
}
