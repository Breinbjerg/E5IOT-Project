/*
 * Project Trackster
 * Description: Code for IoT device "Trackster"
 * Author: Thomas Serup 
 */
#include <Adafruit_GPS.h>
#define GPSSerial Serial1
#define BUTTON D2
#define boardLED D7

// Avoid starting up wifi and connection wtih cloud
SYSTEM_MODE(SEMI_AUTOMATIC);

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

// TCP Client
void SendMail(String);

// Blink function when button is pressed
void blink3(void);

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
SystemSleepConfiguration config1, config2;

// Implementing tcp class
TCPClient client;

void setup()
{ 
  //***************** Pin setup for LED and button ******************
  // Setup pinMode for button 
  pinMode(BUTTON, INPUT_PULLUP);
  // Setup pinMode for board LED 
  pinMode(boardLED, OUTPUT);
  // Setup interrupt for pin D2
  attachInterrupt(BUTTON, handler, FALLING);
  // Button not pressed
  BUTTON_PRESSED = false;

 //************************ Sleep modes ******************************
  // Setup sleep mode. Wakes up by button press or time  
  config1.mode(SystemSleepMode::ULTRA_LOW_POWER)
      .duration(9500ms)
      .gpio(BUTTON, FALLING);
  // Setup new sleepmode, woken up by button press 
  config2.mode(SystemSleepMode::ULTRA_LOW_POWER)
      .gpio(BUTTON, FALLING);
  
  //************************** Debugging *****************************
  // connect at 115200 so we can read the GPS fast enough and echo without dropping chars
  Serial.begin(115200);

  //************************* GPS setup ******************************
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
  
  //*************************** LED setup ****************************
  // take control of the RGB-LED
  RGB.control(true);
  // Set board LED high to show system is on
  digitalWrite(boardLED, HIGH);
  delay(1000);
}

void loop()
{
  init();
  tracking();
  send();
}

void handler()
{
  BUTTON_PRESSED = true; 
}

void init()
{
  // Set LED to yellow
  RGB.color(255, 170, 0);

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
          // Turn off LED
          RGB.color(0, 0, 0);
          
          // Put device to sleep until button is pressed
          System.sleep(config2);

          // Avoid debouncing
          blink3();
          BUTTON_PRESSED = false;
          return;  
        }
      }
    }
  }  
}

void tracking(void)
{
  RGB.color(0, 255, 0);
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
          coords[count] = "|" + String(conv_coords(GPS.latitude), 4) + "," + String(conv_coords(GPS.longitude), 4);
          count++;
          
          // Put device to sleep for 9.5 sec.
          System.sleep(config1); 
        }
      } 
    }  
    // Enters if button has been pressed
    if(BUTTON_PRESSED)
    { 
      // Blink LED
      blink3();
      BUTTON_PRESSED = false;
      return;
    }
  }
}

void send()
{
  // Set LED to red
  RGB.color(255, 0, 0);
  WiFi.on();
  //Connect to WiFI 
  WiFi.connect();
  // Wait for WiFi to be ready 
  while(!WiFi.ready());

  // Create string with all logged coordinates
  String path = "";
  for(int n = 0; n < count; n++)
    path += coords[n]; 

  // Sending to mailGun using SMTP. 
  SendMail(path);
  
  // Set LED to off
  RGB.color(0, 0, 0);
  // Turn off wifi again
  WiFi.off();
}

float conv_coords(float f)
{
  // Get the first two digits by turning f into an integer, then doing an integer divide by 100;
  int firsttwodigits = ((int)f)/100;
  float nexttwodigits = f - (float)(firsttwodigits*100);
  float theFinalAnswer = (float)(firsttwodigits + nexttwodigits/60.0);
  return theFinalAnswer;
}

void SendMail(String path)
{     
  Serial.println("connecting... ");
    
  if (client.connect("smtp.mailgun.org", 587)) 
  { 
    Serial.println("connected"); 
    client.println("EHLO 185.182.204.219");
    client.println("AUTH PLAIN");
    client.println("**************************************");
    client.println("MAIL FROM:<particle@sandbox74891f94361d48bf8fa4f94ebb06d09e.mailgun.org>");
    client.println("RCPT TO:<thomas.serup80@gmail.com>");
    client.println("DATA");  
    client.println("From: Trackster <particle@sandbox74891f94361d48bf8fa4f94ebb06d09e.mailgun.org>");
    client.println("Subject: Your route");
    client.println("To: Thomas <thomas.serup80@gmail.com>");
    client.println();
    client.println("To see your route, open link below:");
    client.println("https://maps.googleapis.com/maps/api/staticmap?size=600x400&path=color:0xff0000ff|weight:5" +path+ "&key=AIzaSyC3WoXkULYQTKwO3bRNlboEQAVWnP80aRM");
    client.println(".");
    client.println("QUIT"); 
  }
  else
  {
    Serial.println("Failed to connect");
  }
  while(true)
  {
    if (client.available())
    {
      char c = client.read();
      Serial.print(c);
    }    
    if (!client.connected())
    {
      Serial.println();
      Serial.println("disconnecting.");
      client.stop();
      return;
    }   
  }
}

void blink3(void)
{
  RGB.color(0, 255, 0);
  delay(200);
  RGB.color(0, 0, 0);
  delay(200);
  RGB.color(0, 255, 0);
  delay(200);
  RGB.color(0, 0, 0);
  delay(200);
  RGB.color(0, 255, 0);
  delay(200);
  RGB.color(0, 0, 0);
}