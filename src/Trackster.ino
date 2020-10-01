/*
 * Project Trackster
 * Description: Code for IoT device "Trackster"
 * Author: Thomas Serup 
 */
#include <Adafruit_GPS.h>
#define GPSSerial Serial1
#define BUTTON D2
#define boardLED D7

// Interrupt handler
void handler(void);
// Initial function for getting fix signal and wait for start signal (button press)
void init(void); 
// Function for tracking the route
void tracking(void);
// Function for sending the data to the cloud
void send(void);
// Function for converting the coordinates to DD instead of DMM
float conv_coords(float f);
// Check if button has been pressed 
boolean BUTTON_PRESSED;
// Connect to the GPS on the hardware port
Adafruit_GPS GPS(&GPSSerial);
// Milliseconds since device was started
uint32_t timer = millis();
// Counting up the data array
int count;
// Data array
String coords[1000];
// Create config variables for different sleepmodes
SystemSleepConfiguration config1;
SystemSleepConfiguration config2;

void setup()
{ 
  // Setup pinMode for button 
  pinMode(BUTTON, INPUT_PULLUP);
  // Setup pinMode for D7 onboard LED
  pinMode(boardLED, OUTPUT);
  // Setup interrupt for pin D2
  attachInterrupt(BUTTON, handler, FALLING);
  // Button not pressed
  BUTTON_PRESSED = false;
  // Setup sleep mode. Wakes up by button press or time  
  config1.mode(SystemSleepMode::ULTRA_LOW_POWER)
      .duration(9500ms)
      .gpio(BUTTON, FALLING);
  // Setup new sleepmode, woken up by button press 
  config2.mode(SystemSleepMode::ULTRA_LOW_POWER)
      .gpio(BUTTON, FALLING);
  // connect at 115200 so we can read the GPS fast enough and echo without dropping chars
  Serial.begin(115200);
  // Set serial port baud rate to 9600 
  GPSSerial.begin(9600);
  // 9600 NMEA is the default baud rate for Adafruit MTK GPS's
  GPS.begin(9600);
  // Turn on only the "minimum recommended" data
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  // Set the update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ); // Once every sec. 
  // Set position FIX update rate
  GPS.sendCommand(PMTK_API_SET_FIX_CTL_1HZ); // Once every sec.
 
  delay(1000);
}

void loop()
{
  init();
  tracking();
  send();
}

void tracking(void)
{
  digitalWrite(boardLED, LOW);
   // Counter for data array
  count = 0;

  while(1)
  {
    // read data from the GPS
    GPS.read();
    // if a sentence is received, we can check the checksum, parse it...
    if (GPS.newNMEAreceived()) 
    {
      // this also sets the newNMEAreceived() flag to false.
      // We can fail to parse a sentence in which case we should just wait for another
      if (GPS.parse(GPS.lastNMEA()))
      {
        // approximately every 10 seconds or so, save the coordinates
        if (millis() - timer > 10000) 
        {
          timer = millis(); // reset the timer 
          // Save latest data in array
          coords[count] = String(conv_coords(GPS.latitude), 4) + "," + String(conv_coords(GPS.longitude), 4);
          count++;
          // Put device to sleep for 9.5 sec.
          System.sleep(config1); 
        }
      } 
    }  
    // Enters if button has been pressed and print out to serial
    if(BUTTON_PRESSED)
    { 
      delay(500);
      BUTTON_PRESSED = false;
      return;
    }
  }
}

void handler()
{
  BUTTON_PRESSED = true; 
}

void init()
{
  WiFi.off();
  digitalWrite(boardLED, LOW);

  while(1)
  {
    // read data from the GPS
    GPS.read();
    // if a sentence is received, we can check the checksum, parse it...
    if(GPS.newNMEAreceived()) 
    {
      // We can fail to parse a sentence in which case we should just wait for another
      if(GPS.parse(GPS.lastNMEA()))
      { 
        // When GPS is connected to satellites  
        if(GPS.fix)
        {
          digitalWrite(boardLED, HIGH);
          // Put device to sleep until button is pressed
          System.sleep(config2);
          // Avoid debouncing
          delay(500);
          BUTTON_PRESSED = false;
          return;  
        }
      }
    }
  }  
}

void send()
{
  WiFi.on();
  // Wait for WiFI connection 
  WiFi.connect();
  delay(5000);
  // Print out all logged coordinates
  for(int n = 0; n < count; n++)
    Serial.println(coords[n]);
}

float conv_coords(float f)
{
  // Get the first two digits by turning f into an integer, then doing an integer divide by 100;
  int firsttwodigits = ((int)f)/100;
  float nexttwodigits = f - (float)(firsttwodigits*100);
  float theFinalAnswer = (float)(firsttwodigits + nexttwodigits/60.0);
  return theFinalAnswer;
}