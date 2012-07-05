/****
 * Arduino Power Meter Reader (APMR)
 * Copyright (C) 2010-2011 Stephen William Makonin. All rights reserved.
 * This project is here by released under the COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL).
 */
 
#include <SPI.h>
#include <Ethernet.h>
#include <ModbusMaster.h>
#include <Time.h>
#include <Wire.h>  
#include <DS1307RTC.h>                                              // a basic DS1307 library that returns time as a time_t
#include <PString.h>

// Network settings
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x5A };                // !!! Set the mac address of your Arduino
//byte ip[] = { 10, 0, 1, 91 };                                       // !!! Set the IP address of your Arduino
//byte gateway[] = { 10, 0, 1, 1 };                                   // !!! Set the LAN gateway
//byte subnet[] = { 255, 255, 255, 0 };                               // !!! Set the LAN subnet mask
byte web_server[] = {69, 31, 166, 42 };                             // !!! Set the IP address of the remote web/DB server

// Meter and webservice settings
const char* home_id = "MAK";                                        // !!! Set the HOME ID (used by the remote server web service)
const char* ws_url = "/restful/datalog/push/meters.py";             // !!! Set the URL of the  remote server web service
const char* ws_host = "makonin.com";                                // !!! Set the URL host name of the  remote server web service
const int console_baud_rate = 19200;                                // !!! Set the Arduino serial console baud rate
const int rs485_baud_rate = 19200;                                  // !!! Set the RS485/MODBUS baud rate
const int meter_count = 2;                                          // !!! Set the number of meters there are to read
int   meter_id[meter_count]   = {101, 102};                         // !!! Add the MODBUS ID for each meter to be read
char* meter_descr[meter_count] = {"MHE", "HPE"};                    // !!! Add the DEVICE ID for each meter (used by the remote server web service)
char*  meter_model[meter_count] = {"ION6200", "ION6200"};           // !!! Add the MODEL ID for each meter to be read (used in control loop of update_data())
float reading_wh[meter_count];
float reading_w[meter_count];

// Buffer and control settings
const int buf1_size = 150 * (meter_count + 1) + 1;
char buffer1[buf1_size];
int last_min = -1;

// Global objects
EthernetServer server(80);
EthernetClient remote_server;
PString json(buffer1, buf1_size);

void setup() 
{
  // Setup Arduino serial console for debug if needed
  Serial.begin(console_baud_rate);

  // Setup the ehternet connection, and wait a bit
  //Ethernet.begin(mac, ip, gateway, subnet);  
  if(Ethernet.begin(mac) == 0)
  {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    while(true);    
  }
  Serial.print("APMR IP Address is ");
  Serial.println(Ethernet.localIP());
  delay(1000);
  
  // Setup the RTC interface 
  setSyncProvider(RTC.get);
  
  // Setup connection to RS485/MODBUS
  Modbus.begin(3, rs485_baud_rate);
  
  // Startup the Arduino web server
  server.begin();
}

void loop() 
{
  int cur_min = minute();
  
  // Every minute on the 0 second, read the meters
  if(!second() && cur_min != last_min)
  {
    update_data();
    render_json();
    send_data();
    last_min = cur_min;
    
    //Serial.println(json);
  }

  // Did anyone make a web request? 
  handle_web_requests();

  // Added to pause processing from hard-spin
  delay(100);
}

// Read each meter and recored the data
void update_data()
{
  int result;
    
  for(int i = 0; i < meter_count; i++)
  {

    reading_wh[i] = 0;
    reading_w[i] = 0;
    
    if(!strcmp(meter_model[i], "ION6200"))
    {
      result = Modbus.readHoldingRegisters(meter_id[i], 119, 28);
      if(result == (int)Modbus.MBSuccess)
      {
        // NOTE: the 0.001 will need to change depending on how your meter's CT and PT ratios are set up
        reading_w[i] = (int)Modbus.getResponseBuffer(0) * 0.0001;
        reading_wh[i] = LONG((long)Modbus.getResponseBuffer(19), (long)Modbus.getResponseBuffer(18))  * 0.001 -
                         LONG((long)Modbus.getResponseBuffer(21), (long)Modbus.getResponseBuffer(20)) * 0.001;
      }
    }
    //else if(!strcmp(meter_model[i], "???????"))
    // ... add other meter models here....
  
    delay(10);
  }
}

// Create the JSON text
void render_json()
{
  json.begin();
  json.print("{\"consumption\": {\r\n");
  json.print("\t\"home\": \"");
  json.print(home_id);
  json.print("\",\r\n");
  json.print("\t\"at_ts\": \"");
  json.print(year());
  json.print("-");
  json.print(month());
  json.print("-");
  json.print(day());
  json.print(" ");
  json.print(hour());
  json.print(":");
  json.print(minute());
  json.print(":00\",\r\n");
  json.print("\t\"meters\": [\r\n");

  for(int i = 0; i < meter_count; i++)
  {
    json.print("\t\t{\"id\": \"");
    json.print(meter_descr[i]);
    json.print("\", \"counter\": ");
    json.print(reading_wh[i]);
    json.print(", \"period_rate\": ");
    json.print("null");
    json.print(", \"instantaneous_rate\": ");
    json.print(reading_w[i]);
    
    if(i + 1 == meter_count)
      json.print("}\r\n");
    else
      json.print("},\r\n");
  }

  json.print("\t\t]\r\n\t}\r\n}\r\n");
}

// Send the JSON text to the remote web/DB server
void send_data()
{
  if(remote_server.connect(web_server, 80)) 
  {
    remote_server.print("POST ");
    remote_server.print(ws_url);
    remote_server.println(" HTTP/1.1");
    remote_server.print("Host: ");
    remote_server.println(ws_host);
    remote_server.print("Content-Length: ");
    remote_server.println(strlen(json));
    remote_server.println("Content-Type: text/plain");
    remote_server.println("Connection: close");
    remote_server.println();
    remote_server.println(json); 
    
    while(remote_server.available())
      char c = remote_server.read();
      
    remote_server.stop();
  }
  else
  {
    // TO DO: When failed, add a processs to retry sending data, so not lost
    Serial.println("Unable to POST to web server.");
  }
  
  delay(1);
}

// Send JSON text to browser/computer that has made a request
void handle_web_requests()
{
  EthernetClient client = server.available();
  
  if(client) 
  {
    boolean is_blank_line = true;
    
    while(client.connected()) 
    {
      if(client.available()) 
      {
        char c = client.read();
        
        if(c == '\n' && is_blank_line) 
        {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/plain");
          client.println();
          
          client.println(json);
          break;
        }

        if(c == '\n') 
          is_blank_line = true;
        else if(c != '\r')
          is_blank_line = false;          
      }
    }

    delay(1);
    client.stop();
  }
}



