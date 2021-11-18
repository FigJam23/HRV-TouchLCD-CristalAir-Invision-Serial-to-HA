
// Reads HRV TTL serial data and sends to an MQTT broker. Assumes you have HASSIO with MQTT running and configured.
//
// Requires:
//
// - ESP8266-01 (other 3.3V versions should work) see: https://github.com/esp8266/esp8266-wiki/wiki/Hardware_versions
// - TTL serial logic level converter (5V -> 3.3V) see: https://www.sparkfun.com/products/12009
// - AMS1117 3.3V voltage regulator (or similar, between HRV VCC and ESP VCC)
// - 1N5817 schottky diode (to stop HRV control panel resetting on ESP power on)
// - 10K ohm resistor (pullup resistor between 3.3 Voltage Regulator and ESP CH_PD)
// - 10V 100uF capacitor (or similar, to smooth out inbound power from HRV)
// - 10V 10uF capacitor (or similar, to smooth out outbound power to ESP)
// - Optional push button to reset the ESP

//
// Author:  Mike Davis
// Date:    15 Nov 2021
// Version: 1

//
// LEDs to indicate operation (for newer ESP's with LED onboard)
// LED on solid = no serial data detected
// LED turned off = no WIFI connection detected
// LED blinking fast 10 times = no MQTT broker connection
// LED blinking every 1-2 seconds = normal operation, sending MQTT data
//
// Credit to: http://www.hexperiments.com/?page_id=47 for data structure
// Credit to: https://github.com/benrugg/Arduino-Hex-Decimal-Conversion for Dec/Hex conversion
//

#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include "basicOTA.h"

// HRV constants
#define MSGSTARTSTOP 0x7E
#define HRVROOF 0x30
#define HRVHOUSE 0x31

// MQTT subs
#define HASSIOHRVSTATUS "hassio/hrv/status"
#define HASSIOHRVSUBHOUSE "hassio/hrv/housetemp"
#define HASSIOHRVSUBROOF "hassio/hrv/rooftemp"
#define HASSIOHRVSUBCONTROL "hassio/hrv/controltemp"
#define HASSIOHRVSUBFANSPEED "hassio/hrv/fanspeedInfo"

// MQTT pubs
#define HASSIOTESTINGPOWERCONTROL "hassio/hrv/powercontrol"
#define HASSIOTESTINGBURNTTOASTMODE "hassio/hrv/burnttoast"
#define HASSIOTESTINGVENTERLATIONLEVEL "hassio/hrv/venterlationlevel"
#define HASSIOTESTINGTEMPCONTROL "hassio/hrv/tempcontol"

// Wifi
const char* ssid     = "SSID";
const char* password = "Password";

// MQTT Broker
IPAddress MQTT_SERVER(192, 168, 1, 0);

// TTL hrvSerial on GPIO ports (HRV does not like RX/TX ports!)
SoftwareSerial hrvSerial(D1, D2);  // RX, TX

// The MAC address of the Arduino
byte mac[] = {  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// Define message buffer and publish string
char message_buff[16];
String pubString;
int iTotalDelay;

// TTL hrvSerial data indicators
bool bStarted = false;
bool bEnded = false;

// Temperature from Roof or House?
char eTempLoc;

// TTL hrvSerial data array, bIndex, bIndex of checksum and temperature
int iLoop;
byte inData[10];
byte bIndex;
byte bChecksum;

// Temperature for sending to MQTT
float fHRVTemp;
int iHRVControlTemp;
int iHRVFanSpeed;

// Maintain last temperature data, don't send MQTT unless changed
float fHRVLastRoof;
float fHRVLastHouse;
int iHRVLastControl;
int iHRVLastFanSpeed;

// Wifi Client
WiFiClient wifiClient;
IPAddress ipadd;

// Callback to Arduino from MQTT (inbound message arrives for a subscription)
// Potential future use...
/*void callback(char* topic, byte* payload, unsigned int length) {
  // Messaging inbound from MQTT broker
  int iChar = 0;
  for(iChar=0; iChar<length; iChar++) {
    message_buff[iChar] = payload[iChar];
  }
  message_buff[iChar] = '\0';
  Serial.println("MQTT callback");
  // This could be used in a future revision to control the HRV control panel remotely?
}*/

// Define Publish / Subscribe client (must be defined after callback function above if in use)
PubSubClient mqttClient(MQTT_SERVER, 1883, wifiClient);


//
// Setup the ESP for operation
//
void setup()
{

  // Setup Firmware update over the air (OTA)
  setup_OTA();
  
  // Set builtin LED as connection indicator
  pinMode(LED_BUILTIN, OUTPUT);

  // LED ON to show powering up but no connection
  digitalWrite(LED_BUILTIN, LOW); 

  // Start HRV software serial (HRV sends at 1200 baud)
  hrvSerial.begin(1200);

  // Debug USB Serial
  Serial.begin(115200);
  //Serial.setDebugOutput(true);
  myDelay(10);

  // Initialize defaults
  bIndex = 0;
  bChecksum = 0;
  iTotalDelay=0;
   
}


void loop()
{
  // Check for OTA updates
  ArduinoOTA.handle();


  // If not MQTT connected, try connecting
  if (!mqttClient.connected())  
  {

    // Check its not cause WiFi dropped
    startWIFI();
    
    // If not MQTT connected, blink LED 5 times in succession
    Serial.println("Connecting to MQTT Broker...");

    int iRetries;

    // If we lost MQTT connection, set last variables to arbitary figure to force a resend on next connection
    iHRVLastFanSpeed = 255;
    iHRVLastControl = 255;
    fHRVLastRoof = 255;
    fHRVLastHouse = 255;

    // WiFi must be connected, try connecting to MQTT 
    while (mqttClient.connect("hrvwireless", HASSIOHRVSTATUS, 0, 0, "0") != 1)
    {

      Serial.println("Error connecting to MQTT (State:" + String(mqttClient.state()) + ")");
      for (int iPos=1; iPos <= 10; iPos++)
      {
        // 10 fast blinks for no MQTT connection, also waits between attempts
        digitalWrite(LED_BUILTIN, LOW);
        myDelay(75);
        digitalWrite(LED_BUILTIN, HIGH);
        myDelay(75);
        Serial.print("."); 
      }
      
      // Double check WiFi state or # retries while in this loop just incase we timed it badly!
      if (WiFi.status() != WL_CONNECTED || iRetries > 5) 
      { 
        Serial.println("No MQTT connection...");
        break; 
      }

      // Make sure we're not stuck here forever, loop and reconnect WiFi if needed
      iRetries++;
    }
  
    // Delay a couple seconds to settle before proceeding
    myDelay(2000);
  
  }

  // LED back on solid if no serial data
  if (hrvSerial.available() == 0)
  {
    Serial.println("No serial data detected!");
    digitalWrite(LED_BUILTIN, LOW);
  }
  else
  {
    // Data available, LED off
    digitalWrite(LED_BUILTIN, HIGH);
  }

  // Only if we're plugged in...
  while (hrvSerial.available() > 0)
  {
    // Read hrvSerial data
    int inChar = hrvSerial.read();

    // Start or stop marker or data too long - which is it? Wait til next loop
    if (inChar == MSGSTARTSTOP || bIndex > 8)
    {
       // Start if first time we've got the message
       if (bIndex == 0)
       {
           bStarted = true; 
       }
       else
       {
           bChecksum = bIndex-1;
           bEnded = true;
           break;
       }
    }  

    // Grab data
    if (bStarted == true)
    {
            
      // Double check we actually got something
      if (sizeof(inChar) > 0)
      {
        //Serial.print(inChar, HEX);
        //Serial.print(",");
        inData[bIndex] = inChar;
        bIndex++;
      }
    }

    // Time for WDT
    myDelay(1);
    
  }

  // Validate data, or if not enough data will fail
  if (bStarted && bEnded && bChecksum > 0)
  {
      int iChar;
      int iLess;

      // Checks
      byte bCalc;
      String sCalc;
      byte bCheck;
      String sCheck;

      // Subtract from zero
      iChar = 0;
      
      // Subtract each byte in ID and data
      for (int iPos=1; iPos < bChecksum; iPos++)
      {
         iLess = inData[iPos];
         iChar = iChar - iLess;
      }
 
      // Convert calculations
      bCalc = (byte) (iChar % 0x100);
      sCalc = decToHex(bCalc, 2);
      bCheck = (byte) inData[bChecksum];
      sCheck = decToHex(bCheck, 2);
      
      // Mod result by 256 and compare to checksum, or not enough data
      if (sCalc != sCheck || bIndex < 6) 
      {
          // Checksum failed, reset
          bStarted = false;
          bEnded = false;
          bIndex = 0;
          
          // Need to flush, maybe getting the end marker first 
          hrvSerial.flush();
      }
      
      // Reset checksum
      bChecksum = 0;
         
  }

  // We got both start and end messages, process the data
  if (bStarted && bEnded)
  {

    // Only process if we got enough data, minimum 6 characters
    if (bIndex > 5)
    {   
        
        String sHexPartOne;
        String sHexPartTwo;
        int iPos;
         
        // Pull data out of the array, position 0 is 0x7E (start and end of message)
        for (int iPos=1; iPos <= bIndex; iPos++)
        {
          
          // Position 1 defines house or roof temperature
          if (iPos==1) { eTempLoc = (char) inData[iPos]; }

          // Position 2 and 3 are actual temperature, convert to hex
          if (iPos == 2) { sHexPartOne = decToHex(inData[iPos], 2); }
          if (iPos == 3) { sHexPartTwo = decToHex(inData[iPos], 2); }

          // Fan speed
          if (eTempLoc == HRVHOUSE && iPos == 4) { iHRVLastFanSpeed = inData[iPos]; }
          
          // If temperature is from control panel
          if (eTempLoc == HRVHOUSE && iPos == 5) { iHRVControlTemp = inData[iPos]; }

        }
        
        // Concatenate first and second hex, convert back to decimal, 1/16th of dec + rounding
        // Note: rounding is weird - it varies between roof and house, MQTT sub rounds to nearest 0.5
        fHRVTemp = hexToDec(sHexPartOne + sHexPartTwo);
        fHRVTemp = (fHRVTemp * 0.0625);

        // Send data to MQTT broker
        SendMQTTMessage();
    
        // Reset defaults for processing
        bStarted = false;
        bEnded = false;
        bIndex = 0;
        
    }
    
  }
  else
  {
    // Wait for hrvSerial to come alive
    myDelay(2000);
  }

  // Send I'm alive message every 30 seconds
  if (WiFi.status() == WL_CONNECTED && mqttClient.connected() && iTotalDelay >= 30000)
  {
    Serial.println("Telling HASSIO we're still alive");
    mqttClient.publish(HASSIOHRVSTATUS, "1");
    iTotalDelay=0;
  }

  mqttClient.loop();
  
}


//
// Convert from decimal to hex
//
String decToHex(byte decValue, byte desiredStringLength) 
{
  
  String hexString = String(decValue, HEX);
  while (hexString.length() < desiredStringLength) hexString = "0" + hexString;
  
  return hexString;
}


//
// Convert from hex to decimal
//
unsigned int hexToDec(String hexString) 
{
  
  unsigned int decValue = 0;
  int nextInt;
  
  for (int i = 0; i < hexString.length(); i++) {
    
    nextInt = int(hexString.charAt(i));
    if (nextInt >= 48 && nextInt <= 57) nextInt = map(nextInt, 48, 57, 0, 9);
    if (nextInt >= 65 && nextInt <= 70) nextInt = map(nextInt, 65, 70, 10, 15);
    if (nextInt >= 97 && nextInt <= 102) nextInt = map(nextInt, 97, 102, 10, 15);
    nextInt = constrain(nextInt, 0, 15);
    
    decValue = (decValue * 16) + nextInt;
  }
  
  return decValue;
}


//
// Send data to MQTT broker
//
void SendMQTTMessage()
{
  
  // Get an exception when MQTT publishing without wifi connection, so quick check first
  if (WiFi.status() == WL_CONNECTED && mqttClient.connected()) 
  {
    // Convert data to nearest 0.5 of a degree then convert to char array
    int iHRVTemp;
    iHRVTemp = (int) ((fHRVTemp * 2) + 0.5);
    fHRVTemp = (float) iHRVTemp / 2;
    pubString = String(fHRVTemp);
    pubString.toCharArray(message_buff, pubString.length()+1);

    // What data are we sending?
    if (eTempLoc == HRVHOUSE)
    {
        Serial.print("House ");
        Serial.println(message_buff);

        // Only send if changed
        if (fHRVTemp != fHRVLastHouse)
        {
          Serial.println("(sending)");
          fHRVLastHouse = fHRVTemp;
          mqttClient.publish(HASSIOHRVSUBHOUSE, message_buff); 
        }
    
    }
    else if (eTempLoc == HRVROOF)
    {
        Serial.print("Roof ");
        Serial.println(message_buff);
        
        // Only send if changed by +/- 0.5 degrees
        if (fHRVTemp != fHRVLastRoof)
        {
          Serial.println("(sending)");
          fHRVLastRoof = fHRVTemp;
          mqttClient.publish(HASSIOHRVSUBROOF, message_buff);
        }
    }

    // Control Panel Set temperature in whole degrees, send if changed
    if (iHRVControlTemp != iHRVLastControl || iHRVLastFanSpeed == 255)
    {
      pubString = String(iHRVControlTemp);
      pubString.toCharArray(message_buff, pubString.length()+1);
      iHRVLastControl = iHRVControlTemp;

      Serial.print("Control Panel: ");
      Serial.println(message_buff);
      
      mqttClient.publish(HASSIOHRVSUBCONTROL, message_buff); 
    }

    // If anything has changed
    if (iHRVLastFanSpeed != iHRVLastFanSpeed || iHRVControlTemp != iHRVLastControl || (eTempLoc == HRVROOF && fHRVTemp != fHRVLastRoof) || (eTempLoc == HRVHOUSE && fHRVTemp != fHRVLastHouse))
    {

      iHRVLastFanSpeed = iHRVLastFanSpeed;

      if (iHRVLastFanSpeed == 0)
      {
        pubString = "Off";
      }
      else if (iHRVLastFanSpeed == 5)
      {
        pubString = "Idle";
      }
      else if (iHRVLastFanSpeed == 100)
      {
        pubString = "Full";
        
        // Heating or Cooling?
        if (iHRVLastControl >= fHRVLastRoof && fHRVLastRoof > fHRVLastHouse)
        {
          pubString = pubString + " (heating)";
        }
        else if (fHRVLastRoof <= iHRVLastControl && fHRVLastRoof < fHRVLastHouse)
        {
          pubString = pubString + " (cooling)";
        }      
      }
      else
      {
        pubString = String(iHRVLastFanSpeed) + "%";
      }

      pubString.toCharArray(message_buff, pubString.length()+1);
      Serial.print("Fan speed: ");
      Serial.println(message_buff);
      
      mqttClient.publish(HASSIOHRVSUBFANSPEED, message_buff); 
    }
  
    // Flash LED real quick to indicate we're still working, then chill a bit
    digitalWrite(LED_BUILTIN, LOW);
    myDelay(50);                    
    digitalWrite(LED_BUILTIN, HIGH);  
    myDelay(1000);
  }
 
}

//
// This function yields back to the watchdog to avoid random ESP8266 resets
//
void myDelay(int ms)  
{

  int i;
  for(i=1; i!=ms; i++) 
  {
    delay(1);
    if(i%100 == 0) 
   {
      ESP.wdtFeed(); 
      yield();
    }
  }

  iTotalDelay+=ms;
  
}


//
// Starts WIFI connection
//
void startWIFI() 
{

    // If we are not connected
    if (WiFi.status() != WL_CONNECTED) 
    {
      int iTries;
      iTries=0;

      Serial.println("Starting WIFI connection");
      WiFi.mode(WIFI_STA);
      WiFi.disconnect(); 
      WiFi.begin(ssid, password);
    
      // If not WiFi connected, retry every 2 seconds for 15 minutes
      while (WiFi.status() != WL_CONNECTED) 
      {
        iTries++;
        Serial.print(".");
        delay(2000);
        
        // If can't get to Wifi for 15 minutes, reboot ESP Default 450
        if (iTries > 50)
        {
           Serial.println("TOO MANY WIFI ATTEMPTS, REBOOTING!");
           ESP.reset();
        }
      }

      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println(WiFi.localIP());
      
      // Let network have a chance to start up
      myDelay(1500);

    }

}
