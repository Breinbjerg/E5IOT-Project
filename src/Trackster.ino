/*
 * Project Trackster
 * Description: Code for IoT device "Trackster"
 * Author: Thomas Serup 
 */
#include <Adafruit_GPS.h>
#include <secret.h>                             // Info not going to github
#define GPSSerial Serial1
#define BUTTON D2
#define BATT D3
#define boardLED D7
#define BATTERY A1

SYSTEM_MODE(SEMI_AUTOMATIC);                    // Avoid starting up wifi and connection with cloud

void handler(void);                             // Interrupt handler
void batt_handler(void);                        // Interrupt handler
void init(void);                                // Initial function getting fix signal and wait for start signal (button press)
void tracking(void);                            // Function for tracking the route
void send(void);                                // Create string from coordinates to send to mail
float conv_coords(float);                       // Converting the coordinates to DD instead of DMM
void SendMail(String);                          // TCP Client to send SMTP mail
void blink_green(void);                         // RGB-LED Blink function when button is pressed
void read_battery(void);                        // Read analog pin to get battery level

boolean BUTTON_PRESSED;                         // Check if button has been pressed
boolean BATT_PRESSED;                           // Check if battery button has been pressed 
Adafruit_GPS GPS(&GPSSerial);                   // Connect to the GPS on the hardware port
uint32_t timer = millis();                      // Milliseconds since device was started
int count;                                      // Counting up the data array
String coords[1000];                            // Data array holding all coordinates
SystemSleepConfiguration config1,               // Create config variables for different sleepmodes
                         config2, 
                         config3;               
TCPClient client;                               // Implementing tcp class

void setup()
{ 
  //************************** PIN SETUP ****************************************
  pinMode(BUTTON, INPUT_PULLUP);                // Setup pinMode for button
  pinMode(BATT, INPUT_PULLUP);                  // Setup pinMode for battery button 
  pinMode(boardLED, OUTPUT);                    // Setup pinMode for board LED
  attachInterrupt(BUTTON, handler, FALLING);    // Setup interrupt for pin D2
  attachInterrupt(BATT, batt_handler, FALLING); // Setup interrupt for pin D2
  BUTTON_PRESSED = false;                       // Button not pressed
  BATT_PRESSED = false;                         // Battery button not pressed

 //************************** SLEEP-MODES ***************************************  
  config1.mode(SystemSleepMode::ULTRA_LOW_POWER)// Wakes up by button press or time
      .duration(9500ms)
      .gpio(BUTTON, FALLING)
      .gpio(BATT, FALLING);                     // Get battery status

  config2.mode(SystemSleepMode::ULTRA_LOW_POWER)// Wakes up by button press 
      .gpio(BUTTON, FALLING)
      .gpio(BATT, FALLING);                     // Get battery status
  
  config3.mode(SystemSleepMode::ULTRA_LOW_POWER)// Wakes up by time 
      .duration(2000ms);

  //************************** DEBUGGING ****************************************
  Serial.begin(115200);                         // connect at baud rate 115200 

  //************************** GPS SETUP **************************************** 
  GPSSerial.begin(9600);                        // Set serial port baud rate to 9600
  GPS.begin(9600);                              // 9600 NMEA is the default baud rate for Adafruit MTK GPS's 
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);// Turn on only the "minimum recommended" data
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);    // Set the update rate to once every sec. 
  GPS.sendCommand(PMTK_API_SET_FIX_CTL_1HZ);    // Set position FIX update rate to once every sec.
  
  //*************************** LED SETUP ***************************************
  RGB.control(true);                            // take control of the RGB-LED
  digitalWrite(boardLED, HIGH);                 // Set board LED high to show system is on
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

void batt_handler()
{
  BATT_PRESSED = true; 
}

void init()
{
  // Set LED to yellow
  RGB.color(255, 170, 0);

  while(1)
  {
    // Once every 3rd. second it goes to sleep 
    if (millis() - timer > 3000) 
        {
          // reset the timer
          timer = millis();
          // Put device to sleep for 2 seconds
          System.sleep(config3); 
        }
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
          // If button has been pressed while waiting for satellittes
          BUTTON_PRESSED = false;
          
          // Turn off LED
          RGB.color(0, 0, 0);
          
          // Put device to sleep until button is pressed
          System.sleep(config2);

          // Goes in here if button D2 has been pressed
          if(BUTTON_PRESSED)
          {
            // Avoid debouncing
            blink_green();
            BUTTON_PRESSED = false;
            return;
          }
          // Else it must have been the other button
          read_battery();
          BATT_PRESSED = false;      
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
          // Turn back on green if read_battery has been called
          RGB.color(0, 255, 0); 
          // reset the timer
          timer = millis();  
          // Save latest data in array
          coords[count] = "|" + String(conv_coords(GPS.latitude), 4) + "," + String(conv_coords(GPS.longitude), 4);
          count++;
          
          // Put device to sleep for 9.5 sec.
          System.sleep(config1); 
        }
      } 
    }
    // Enter if battery button has been pressed
    if(BATT_PRESSED)
    {
      read_battery();
      BATT_PRESSED = false; 
    } 

    // Enters if button has been pressed
    if(BUTTON_PRESSED)
    { 
      // Blink LED
      blink_green();
      BUTTON_PRESSED = false;
      return;
    }
  }
}

void send()
{
  // Set LED to red
  RGB.color(255, 0, 0);
  // Turn on wi-fi module
  WiFi.on();
  //Connect to WiFI 
  WiFi.connect();
  // Wait for WiFi to be ready 
  while(!WiFi.ready()){
    RGB.color(255, 0, 0);
    delay(200);
    RGB.color(0, 0, 0);
    delay(200);
  };
  
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
    client.println(HELLO);
    client.println("AUTH PLAIN");
    client.println(PASSWORD);
    client.println(MAIL_FROM);
    client.println(MAIL_TO);
    client.println("DATA");  
    client.println(FROM);
    client.println("Subject: Your route");
    client.println(TO);
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

void blink_green(void)
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

void read_battery()
{
  // Equal size resistors in voltage devider gives half the voltage 
  // (Half the voltage)/(resolution) 
  // eg. 4V: (4/2)/(3.3/4096) = 2500 
  #define LOWEST 2100                         // ADC Value where light suppose to be red
  #define HIGEST 2500                         // ADC Value where light suppose to be green

  int battery = analogRead(BATTERY);          // Read analog value between 0 and 4095. 12-bit resolution
  int factor = (255*10)/(HIGEST-LOWEST);      // Multiply by 10 to avoid floats 
  int val = ((battery - LOWEST)*(factor))/10; // Devide by 10 to return to original scale
  
  if(val < 0)                                 // Keep value within 0-255
    val = 0;
  if(val > 255)
    val = 255;
    
  int green = val;                            // High value, high green
  int red = 255-val;                          // High value, low red  
      
  RGB.color(red, green, 0);
  
  delay(1500);                                // Show battery status for 1.5 sec.
  RGB.color(0, 0, 0);
}