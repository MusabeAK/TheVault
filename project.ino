#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include <Servo.h>
#include <dht.h> // Include library
#include <String.h>

#include <SoftwareSerial.h>
SoftwareSerial gprsSerial(A13,A12);

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define OLED_RESET -1         // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C   ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define outPin 11             // Defines pin number to which the sensor is connected
#define BUZZER_PIN 12         // Define buzzer pin
#define LED_PIN 14            // Define red LED pin
#define GREEN_LED_PIN 13      // Define green LED pin

/* variables to be sent to 
 * Things Speak 
 */
float t ;        // temperature
float h ;        // humidity
unsigned int peakToPeak = 0;   // peak-to-peak level (sound level)

const byte ROWS = 4;  // four rows
const byte COLS = 4;  // four columns
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {2, 3, 4, 5}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {6, 7, 8, 9}; //connect to the column pinouts of the keypad

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
Servo servo;
dht DHT; // Creates a DHT object

int servoPin = 10;
int angle = 0;  // servo position in degrees

String inputCode = "";
const String correctCode = "1234";
int successfulEntries = 0; // Counter for successful code entries

const int sampleWindow = 50;  // Sample window width in mS (50 mS = 20Hz)
int const AMP_PIN = A1;       // Preamp output pin connected to A0
unsigned int sample;

unsigned long previousMillisTemp = 0;
unsigned long previousMillisSound = 0;
unsigned long intervalTemp = 5000;  // 5 seconds in milliseconds
unsigned long intervalSound = 10000;  // 10 seconds in milliseconds
unsigned long minutes = 0;
unsigned long currentTimeTemp = 0;

void setup() {
  servo.attach(servoPin);
  Serial.begin(9600);
  pinMode(BUZZER_PIN, OUTPUT); // Initialize buzzer pin as output
  pinMode(LED_PIN, OUTPUT);    // Initialize red LED pin as output
  pinMode(GREEN_LED_PIN, OUTPUT); // Initialize green LED pin as output

  // Set up the OLED display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("VAULT");
  display.display();
  delay(2000);
  display.clearDisplay();


  // Things Speak set up
  gprsSerial.begin(9600);               // the GPRS baud rate     // the GPRS baud rate 
  delay(1000);
}

void loop() {
  unsigned long currentMillisTemp = millis();
  unsigned long currentMillisSound = millis();

  // Temperature and humidity sensing
  if (currentMillisTemp - previousMillisTemp >= intervalTemp) {
    previousMillisTemp = currentMillisTemp;

    int readData = DHT.read11(outPin);

    t = DHT.temperature;        // Read temperature
    h = DHT.humidity;           // Read humidity

    // Get the current time for recording
    currentTimeTemp = millis();

    Serial.print("Time: ");
    printTime(currentTimeTemp);
    Serial.print("Temperature = ");
    Serial.print(t);
    Serial.print("°C | ");
    Serial.print((t * 9.0) / 5.0 + 32.0); // Convert celsius to fahrenheit
    Serial.println("°F ");
    Serial.print("Humidity = ");
    Serial.print(h);
    Serial.println("% ");
    Serial.println("");

    if (t > 35.0) {
      Serial.println("Alert: Temperature exceeded 35°C!");
      // Blink the red LED three times
      for (int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(500);
        digitalWrite(LED_PIN, LOW);
        delay(500);
      }
    }
  }

  // Sound sensing
  if (currentMillisSound - previousMillisSound >= intervalSound) {
    previousMillisSound = currentMillisSound;

    unsigned int signalMax = 0;
    unsigned int signalMin = 1024;

    // collect data for 50 mS and then plot data
    unsigned long startMillis = millis(); // Start of sample window
    while (millis() - startMillis < sampleWindow) {
      sample = analogRead(AMP_PIN);
      if (sample < 1024) {  // toss out spurious readings
        if (sample > signalMax) {
          signalMax = sample;  // save just the max levels
        } else if (sample < signalMin) {
          signalMin = sample;  // save just the min levels
        }
      }
    }
    peakToPeak = signalMax - signalMin;  // max - min = peak-peak amplitude

    // Get the current time for recording
    unsigned long currentTimeSound = millis();

    // Print the peak-to-peak amplitude along with the current time
    Serial.print("Peak-to-Peak Amplitude: ");
    Serial.print(peakToPeak);
    Serial.print(" at ");
    printTime(currentTimeSound);

    // Check if peak-to-peak amplitude exceeds the threshold (e.g., 1000)
  if (peakToPeak > 1000) {
    // Blink the red LED three times
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(500);
      digitalWrite(LED_PIN, LOW);
      delay(500);
    }

    // Sound the buzzer
    digitalWrite(BUZZER_PIN, HIGH); // Activate the buzzer
    delay(1000); // Sound the buzzer for 1 second
    digitalWrite(BUZZER_PIN, LOW);  // Turn off the buzzer
  }
  }

  char key = keypad.getKey();

  if (key) {
    inputCode += key;
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Entered Key: ");
    display.print(inputCode);
    display.display();
  }

  if (inputCode == correctCode) {
    successfulEntries++;
    display.clearDisplay();

    if (successfulEntries % 2 != 0) {
      display.setCursor(0, 0);
      display.print("Access Granted");
      // Blink the green LED three times
      for (int i = 0; i < 3; i++) {
        digitalWrite(GREEN_LED_PIN, HIGH);
        delay(500);
        digitalWrite(GREEN_LED_PIN, LOW);
        delay(500);
      }
      // Odd number of entries, rotate forward
      for (angle = 60; angle < 180; angle++) {
        servo.write(angle);
        delay(15);
      }
    } else {
      display.setCursor(0, 0);
      display.print("Access Revoked.");
      for (int i = 0; i < 3; i++) {
        digitalWrite(GREEN_LED_PIN, HIGH);
        delay(500);
        digitalWrite(GREEN_LED_PIN, LOW);
        delay(500);
      }
      // Even number of entries, rotate backward
      for (angle = 180; angle > 60; angle--) {
        servo.write(angle);
        delay(15);
      }
    }
    display.display();
    delay(2000); // Display success message for 2 seconds
    inputCode = "";  // Reset input code for next attempt
  } else if (inputCode.length() == correctCode.length()) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Wrong Key!");
    display.display();
    // Blink the red LED three times
    digitalWrite(BUZZER_PIN, HIGH); // Activate the buzzer
    delay(1000); // Sound the buzzer for 1 second
    digitalWrite(BUZZER_PIN, LOW);  // Turn off the buzzer
    delay(1000); // Display error message for 1 second
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(500);
      digitalWrite(LED_PIN, LOW);
      delay(500);
    }
    delay(1000); // Display error message for 1 second
    inputCode = "";  // Reset input code for next attempt
  }
  
  // printTime(currentTimeTemp);
  if( minutes % 1 == 0){
    thingsSpeakConn();
  }
}

void printTime(unsigned long millisec) {
  // Convert milliseconds to seconds
  unsigned long seconds = millisec / 1000;

  // Calculate hours, minutes, and seconds
  unsigned long hours = seconds / 3600;
  seconds = seconds % 3600;
  minutes = seconds / 60;
  seconds = seconds % 60;

  // Print the formatted time
  if (hours < 10) {
    Serial.print("0");
  }
  Serial.print(hours);
  Serial.print(":");

  if (minutes < 10) {
    Serial.print("0");
  }
  Serial.print(minutes);
  Serial.print(":");

  if (seconds < 10) {
    Serial.print("0");
  }
  Serial.println(seconds);
}


/* Connection to :
 * SIM800L module, and
 * ThingsSpeak
 */


void thingsSpeakConn(){
  delay(100);   
         
  if (gprsSerial.available())
    Serial.write(gprsSerial.read());
 
  gprsSerial.println("AT");
  delay(1000);
 
  gprsSerial.println("AT+CPIN?");
  delay(1000);
 
  gprsSerial.println("AT+CREG?");
  delay(1000);
 
  gprsSerial.println("AT+CGATT?");
  delay(1000);
 
  gprsSerial.println("AT+CIPSHUT");
  delay(1000);
 
  gprsSerial.println("AT+CIPSTATUS");
  delay(2000);
 
  gprsSerial.println("AT+CIPMUX=0");
  delay(2000);
 
  ShowSerialData();
 
  gprsSerial.println("AT+CST T=\"internet.ng.airtel.com\"");//start task and setting the APN,
  delay(1000);
 
  ShowSerialData();
 
  gprsSerial.println("AT+CIICR");//bring up wireless connection
  delay(3000);
 
  ShowSerialData();
 
  gprsSerial.println("AT+CIFSR");//get local IP adress
  delay(2000);
 
  ShowSerialData();
 
  gprsSerial.println("AT+CIPSPRT=0");
  delay(3000);
 
  ShowSerialData();
  
  gprsSerial.println("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",\"80\"");//start up the connection
  delay(6000);
 
  ShowSerialData();
 

  gprsSerial.println("AT+CIPSEND");//begin send data to remote server
  delay(4000);
  ShowSerialData();
 
  String str="GET https://api.thingspeak.com/update?api_key=S5CHM8IZMB20XCOR&field1=" + String(t) + "&field2=" + String(h) + "&field3=" + String(peakToPeak);
  Serial.println(str);
  gprsSerial.println(str);//begin send data to remote server
  
  delay(4000);
  ShowSerialData();
 
  gprsSerial.println((char)26);//sending
  delay(5000);//waitting for reply, important! the time is base on the condition of internet 
  gprsSerial.println();
 
  ShowSerialData();
 
  gprsSerial.println("AT+CIPSHUT");//close the connection
  delay(100);
  ShowSerialData();
}

void ShowSerialData()
{
  while(gprsSerial.available()!=0)
  Serial.write(gprsSerial.read());
  delay(5000); 
  
}

