#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>                                                                  
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

//Create a web server
WebServer server ( 80 );

// Create a Websocket server....
WebSocketsServer webSocket(81);


unsigned int state;
  
#define SEQUENCE_IDLE 0x00
#define GET_SAMPLE 0x10

#define GET_SAMPLE__WAITING 0x12

const char WiFiAPPSK[] = "Livewell";

#define USE_SERIAL Serial
#define DBG_OUTPUT_PORT Serial

byte promValues[] = {0,0,0};

uint8_t remote_ip;
uint8_t socketNumber;
uint8_t send_num;
float value;
int ontime;   //On time setting from mobile web app
int offtime;  //Off time setting from mobile web app

//These will need to be updated to the GPIO pins for each control circuit.
int POWER = 13;
int TIMER_SWITCH = 2; 
int WIFI_CONNECTION = 15;
int WIFI_CLIENT_CONNECTED = 16;
int Timer_LED = 17;
int SPEED = 14; 
int LEFT = 12; 
int RIGHT = 13;
const int ANALOG_PIN = A0;

int onoff = 1; 

volatile byte switch_state = HIGH;
boolean pumpOn = false;
boolean timer_state = false;
boolean timer_started = false;
boolean wifi_state = false;
boolean wifi_client_conn = false;
int startup_state;
int ontime_value;  //number of ON minutes store in EEPROM
int offtime_value; //number of OFF minutes store in EEPROM

int Clock_seconds;

 
//Keep this code for reference to determine how to get voltage for pump
//extern "C" {
//uint16 readvdd33(void);
//}

hw_timer_t * timer = NULL;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
    String text = String((char *) &payload[0]);
    char * textC = (char *) &payload[0];
    String voltage;
    String temp1, temp2, temp3, temp4;
    float percentage;
    float actual_voltage;  //voltage calculated based on percentage drop from 5.0 circuit.
    int nr;
    //int onof;
    uint32_t rmask;
    int i;
    
    switch(type) {
        case WStype_DISCONNECTED:
            //Reset the control for sending samples of ADC to idle to allow for web server to respond.
            USE_SERIAL.printf("[%u] Disconnected!\n", num);
            state = SEQUENCE_IDLE;
            digitalWrite(WIFI_CLIENT_CONNECTED, LOW);
            break;
        case WStype_CONNECTED:
          {
            //Display client IP info that is connected in Serial monitor and set control to enable samples to be sent every two seconds (see analogsample() function)
            IPAddress ip = webSocket.remoteIP(num);
            USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
            digitalWrite(WIFI_CLIENT_CONNECTED, HIGH);
            socketNumber = num;
            state = GET_SAMPLE;

            StaticJsonDocument<200> doc;
         
            doc["message_type"] = "startup";
            doc["startup_state"] = String(startup_state);
            doc["ontime"] = String(ontime);
            doc["offtime"] = String(offtime);
            
            String databuf;
            serializeJson(doc, databuf);
            //serializeJsonPretty(doc, Serial);
            
            webSocket.sendTXT(num, databuf);
            //webSocket.sendTXT(num, temp4);
            wifi_client_conn = true;
          }
            break;
 
        case WStype_TEXT:

            if (payload[0] == 'I') 
            {
              Serial.println("Communicating to device...");
            }
            if (payload[0] == '+')
                {
                Serial.printf("[%u] On Time Setting Control Msg: %s\n", num, payload);
                Serial.println((char *)payload);
                ontime = abs(atoi((char *)payload));
                //uint32_t ontime = (uint32_t) strtol((const char *) &payload[1], NULL, 2);
                Serial.print("On Time = ");
                Serial.println(ontime);
                EEPROM.write(1,ontime);
                EEPROM.commit();
                delay(500);
                byte value = EEPROM.read(1);
                Serial.println("On Time Stored in EEPROM " + String(value));
                }
            if (payload[0] == '-')
                {
                Serial.printf("[%u] Off Time Setting Control Msg: %s\n", num, payload);
                Serial.println((char *)&payload[1]);
                offtime = abs(atoi((char *)payload));
                //uint32_t offtime = (uint32_t) strtol((const char *) &payload[1], NULL, 2);
                Serial.printf("Off Time = ");
                Serial.println(offtime);
                EEPROM.write(2,offtime);
                EEPROM.commit();
                delay(500);
                byte value = EEPROM.read(2);
                Serial.println("Off Time stored in EEPROM " + String(value));
                }
            if (payload[0] == 'O')
                {
                Serial.printf("[%u] Timer state event %s\n", num, payload);
                //Serial.println((char *)&payload);
                if (payload[1] == 'N')
                  {
                    startup_state = 1;
                    EEPROM.write(0,startup_state);
                    EEPROM.commit();
                    byte value = EEPROM.read(0);
                    Serial.println("Timer turned ON " + String(value));
                    timer_state = true;
                    pumpOn = true;
                  }
                if (payload[2] == 'F')
                  {
                    startup_state = 0;
                    EEPROM.write(0,startup_state);
                    EEPROM.commit();
                    byte value = EEPROM.read(0);
                    Serial.println("Timer turned OFF " + String(value));
                    timer_state = false;
                    pumpOn = false;
                  }
                }

            //Hang on to this code until you are done!!!!
            /*if (payload[0] == '#')  
                Serial.printf("[%u] Digital GPIO Control Msg: %s\n", num, payload);
                if (payload[1] == 'I')
                {
                  if (payload[2] == 'D')
                    {
                    Serial.printf("Direction Down");
                    //value=readvdd33();
                    Serial.print("Vcc:");
                    Serial.println(value/1000);
                    percentage = (value/1000)/3.0;
                    Serial.print("percentage:");
                    Serial.println(percentage);
                    actual_voltage = 11.8*percentage; //Using 11.8 to conservative
                    Serial.print("act_volt:");
                    Serial.println(percentage);
                    voltage = String(actual_voltage);
                    webSocket.sendTXT(num, voltage);
                    //digitalWrite(POWER, HIGH);
                    }
                  if (payload[2] == 'U')
                    {
                    Serial.printf("Direction Up");
                    //digitalWrite(POWER, LOW);
                    }
                  break;
                }
                if (payload[1] == 'M')
                {
                  if (payload[2] == 'D')
                    {
                    Serial.printf("Direction Down");
                    //digitalWrite(MOMENTARY, HIGH);
                    }
                  if (payload[2] == 'U')
                    {
                    Serial.printf("Direction Up");
                    //digitalWrite(MOMENTARY, LOW);
                    }
                  break;
                }
                if (payload[1] == 'L')
                {
                  if (payload[2] == 'D')
                    {
                    Serial.printf("Direction Down");
                    digitalWrite(LEFT, HIGH);
                    }
                  if (payload[2] == 'U')
                    {
                    Serial.printf("Direction Up");
                    digitalWrite(LEFT, LOW);
                    }
                  break;
                 }
                 if (payload[1] == 'R')
                 {
                  if (payload[2] == 'D')
                    {
                    Serial.printf("Direction Down");
                    digitalWrite(RIGHT, HIGH);
                    }
                  if (payload[2] == 'U')
                    {
                    Serial.printf("Direction Up");
                    digitalWrite(RIGHT, LOW);
                    }
                  break;
                 }
            if (payload[0] == 'S')
              {
                Serial.printf("[%u] Analog GPIO Control Msg: %s\n", num, payload);
              }*/
              
         case WStype_BIN:
         {
            /*USE_SERIAL.printf("[%u] get binary lenght: %u\n", num, lenght);
            //hexdump(payload, lenght);
            //analogWrite(13,atoi((const char *)payload));
            Serial.printf("Payload");
            Serial.printf("[%u] Analog GPIO Control Msg: %s\n", num, payload);
            Serial.println((char *)payload);
            int temp = atoi((char *)payload);
            uint32_t time_setting = (uint32_t) strtol((const char *) &payload[1], NULL, 8);
            //analogWrite(SPEED,temp);   ******************** NEED TO CREATE ANALOG FUNCTION ***********
            //webSocket.sendTXT(num,"Got Speed Change");*/
         }
         break;
         
         case WStype_ERROR:
            USE_SERIAL.printf(WStype_ERROR + " Error [%u] , %s\n",num, payload); 
    }
}

void handleRoot() {
  server.send(200, "text/html", "<h1>You are connected</h1>");
}

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  else if(filename.endsWith(".svg")) return "image/svg+xml";
  return "text/plain";
}

bool handleFileRead(String path){
  DBG_OUTPUT_PORT.println("handleFileRead: " + path);
  if(path.endsWith("/"))
    {
      path += "relay.html";
      state = SEQUENCE_IDLE;
      File file = SPIFFS.open(path, "r");
      Serial.print("Sending relay.html ");
      Serial.print(path);
      String contentType = getContentType(path);
      size_t sent = server.streamFile(file, contentType);
      file.close();
      return true;
    }
  
  String pathWithGz = path + ".gz";
  DBG_OUTPUT_PORT.println("PathFile: " + pathWithGz);
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    //size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void setupWiFi()
{
  WiFi.mode(WIFI_AP);
  
  String AP_NameString = "SS Regulator";

  char AP_NameChar[AP_NameString.length() + 1];
  memset(AP_NameChar, 0, AP_NameString.length() + 1);

  for (int i=0; i<AP_NameString.length(); i++)
    AP_NameChar[i] = AP_NameString.charAt(i);

  WiFi.softAP(AP_NameChar);
  wifi_state = true;
  digitalWrite(WIFI_CONNECTION, HIGH);
}

void IRAM_ATTR onoffTimer(){

  switch (onoff) {

    case 0:
      if (pumpOn == false) 
      {
        digitalWrite(TIMER_SWITCH,LOW);  //We only need to set TIMER_SWITCH once....set pumpOn to TRUE in prep for end of OFFTIME.
        pumpOn = true;
        Serial.println("Turning OFF pump");
        digitalWrite(Timer_LED, LOW);
      }
      Serial.println("Pump has been OFF for " + String(Clock_seconds) + " seconds");
      Clock_seconds++;
      if (Clock_seconds > (offtime*60)){
        onoff = 1;
        Clock_seconds = 1;
      }
      break;
    
    case 1:
      if (pumpOn == true)
      {
        digitalWrite(TIMER_SWITCH,HIGH);  //We only need to set TIMER_SWITCH once....set pumpOn to FALSE in prep for end of OFFTIME.
        pumpOn = false;
        Serial.println("Turning ON pump");
        digitalWrite(Timer_LED, HIGH);
      }
      Serial.println("Pump is running for " + String(Clock_seconds) + " seconds");
      Clock_seconds++;
      if (Clock_seconds > (ontime*60)) {
        onoff = 0;
        Clock_seconds = 1;
      }
      break;
    }
}

void startTimer(){
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onoffTimer, true);
  timerAlarmWrite(timer, 1000000, true);
  yield();
  timerAlarmEnable(timer);
  timer_started = true;
  Serial.println("Timer Started");
}

void stopTimer(){
  if (timer != NULL) {
    timerAlarmDisable(timer);
    timerDetachInterrupt(timer);
    timerEnd(timer);
    timer = NULL;
    timer_started = false;
    Serial.println("Timer Stopped");
    digitalWrite(TIMER_SWITCH,LOW);
    digitalWrite(Timer_LED,LOW);
  }
}

void setup() {
  pinMode(POWER, OUTPUT);
  //pinMode(TIMER_SWITCH, OUTPUT);
  pinMode(WIFI_CONNECTION, OUTPUT);
  pinMode(WIFI_CLIENT_CONNECTED, OUTPUT);
  pinMode(Timer_LED, OUTPUT);
  pinMode(SPEED, OUTPUT);
  
  digitalWrite(POWER, LOW);
  //digitalWrite(TIMER_SWITCH, LOW);
  digitalWrite(WIFI_CONNECTION, LOW);
  digitalWrite(WIFI_CLIENT_CONNECTED, LOW);
  digitalWrite(Timer_LED, LOW);
  digitalWrite(SPEED, HIGH);
  
  
  Serial.begin(115200);
  SPIFFS.begin();
  Serial.println();
  Serial.print("Configuring access point...");
  /* You can remove the password parameter if you want the AP to be open. */
  setupWiFi();
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  EEPROM.begin(3); //Index of three for - On/Off state 1 or 0, OnTime value, OffTime value


  //Determine state of timer before it was powered off
  //startup_state = EEPROM.read(0);
  //if (startup_state == 0) 
 
    Serial.println("Start up state of pump is OFF");
    timer_state = false;
    pumpOn = false;
    pinMode(TIMER_SWITCH, OUTPUT);
    digitalWrite(TIMER_SWITCH, LOW);
    digitalWrite(Timer_LED, LOW);
  //}
  //else
  //{
  //  Serial.println("State of pump is ON...turning it off");
  //  timer_state = true;
  //  pumpOn = true;
  //  pinMode(TIMER_SWITCH, OUTPUT);
  //  digitalWrite(TIMER_SWITCH, LOW);
  //}

  //Determine the value set for ON Time in EEPROM
  ontime_value = EEPROM.read(1);
  if (ontime_value > 0) 
  {
    ontime = ontime_value;
    Serial.println("On Time setting is " + String(ontime));
  }
    
  //Determine the value set for OFF Time inEEPROM
  offtime_value = EEPROM.read(2);
  if (offtime_value > 0) 
  {
    offtime = offtime_value;
    Serial.println("OFF Time setting is " + String(offtime));
  }

  server.on("/", HTTP_GET, [](){
    handleFileRead("/");
    //handleRoot();
  });

//Handle when user requests a file that does not exist
  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
  server.send(404, "text/plain", "FileNotFound");
  });
  server.begin();
  Serial.println("HTTP server started");

  // start webSocket server
  webSocket.begin();
  Serial.println("Websocket server started");
  webSocket.onEvent(webSocketEvent);

//+++++++ MDNS will not work when WiFi is in AP mode but I am leave this code in place incase this changes++++++
//if (!MDNS.begin("esp8266")) {
//    Serial.println("Error setting up MDNS responder!");
//    while(1) { 
//      delay(1000);
//    }0;9
//  }
//  Serial.println("mDNS responder started");

  // Add service to MDNS
  //  MDNS.addService("http", "tcp", 80);
  //  MDNS.addService("ws", "tcp", 81);

}

void loop() {

  /*StaticJsonDocument<200> doc;
  
  doc["sensor"] = "gps";
  doc["time"] = 1351824120;
  
  JsonArray data = doc.createNestedArray("data");

  data.add(48.756080);  // 6 is the number of decimals to print
  data.add(2.302038);   // if not specified, 2 digits are printed
  String databuf;
  serializeJson(doc, databuf);
  //serializeJsonPretty(doc, Serial);
  
  webSocket.sendTXT(0, databuf);

  delay(10);*/

  webSocket.loop();
  server.handleClient();
  if ((timer_state == true) && (timer_started == false)) startTimer();
  if ((timer_state == false) && (timer_started == true)) stopTimer();

   StaticJsonDocument<200> clock_time;
  clock_time["message_type"] = "clock_update";
  clock_time["value"] = Clock_seconds;
  JsonObject time_data = clock_time.createNestedObject();
  
  
  String databuf;
  serializeJson(clock_time, databuf);
 
 
  //webSocket.sendTXT(0, databuf);
}