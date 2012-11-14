/****
 * Arduino Power Meter Reader (APMR)
 * Copyright (C) 2010-2012 Stephen Makonin and contributors. All rights reserved.
 * This project is here by released under the COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL).
 */
 
#include <SPI.h>
#include <Ethernet.h>
#include <ModbusMaster.h>
#include <Time.h>
#include <Wire.h>  
#include <DS1307RTC.h> 
#include <SD.h>
#include <EEPROM.h>

// Constants Defs
#define MAX_METERS 16
#define ITYPE_INT 0
#define ITYPE_HEX 1
#define ITYPE_STR 2
#define MAX_MEASURES 2
#define MTYPE_W 0
#define MTYPE_WH 1

// Default network settings
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x5A };

// Default webservice settings
char ws_host[33];
char ws_url[65];
word ws_port = 80;
byte console_baud_rate = 1;
byte rs485_baud_rate = 1;
long baud_rates[5] = { 9600, 19200, 38400, 57600, 115200 };

// Default meter settings
byte read_rate = 3;
char home_id[5];
byte meter_count = 0;
byte modbus_id[MAX_METERS];
char meter_id[MAX_METERS][5];
byte meter_type[MAX_METERS];
float readings[MAX_METERS][MAX_MEASURES];
char *read_rates[] = {"1 sec", "5 sec", "30 sec", "1 min", "15 min",  "30 min", "1 hr"};
char *meter_types[] = {" ", "ION6200"/*, add other supported meter types here*/};
char *measure_types[] = {"power", "energy" };

// Global objects, buffer and control settings
EthernetServer server(80);
time_t t;
byte last_min = 255;
byte last_sec = 255;
char *sd_unsent = "t_unsent.txt";
char *sd_json = "t_json.txt";
char sd_dir[16] = {0000000000000000};
char sd_file[32] = {00000000000000000000000000000000};

void setup() 
{
  //read in eeprom settings
  read_settings();
  
  // Setup Arduino serial console for debug if needed
  Serial.begin(baud_rates[console_baud_rate]);
  delay(1000);
  
  Serial.print("Booting");
  // Initialize SD Card
  // On the Ethernet Shield, CS is pin 4. It's set as an output by default.
  // Note that even if it's not used as the CS pin, the hardware SS pin 
  // (10 on most Arduino boards, 53 on the Mega) must be left as an output 
  // or the SD library functions will not work.  
  pinMode(53, OUTPUT);
  digitalWrite(53, HIGH);
  if(!SD.begin(4))
  {
    Serial.println("\r\nERROR: (S1)  unable to initialize SD card");
    // no point in carrying on, so do nothing forevermore:
    while(true);
  }
  delay(1000);
  Serial.print(".");

  // Setup the RTC interface 
  setSyncProvider(RTC.get);
  Serial.print(".");
  
  // Setup connection to RS485/MODBUS
  Modbus.begin(3, baud_rates[rs485_baud_rate]);
  Serial.print(".");
  
  // Setup the ehternet connection, and wait a bit  
  if(!Ethernet.begin(mac))
  {
    Serial.println("\r\nERROR: (S2) unable to get DHCP IP address");
    // no point in carrying on, so do nothing forevermore:
    while(true);
  }
  Serial.println(".");
  Serial.print("IP Address is ");
  Serial.println(Ethernet.localIP());

  // Startup the Arduino web server
  server.begin();
}

void loop() 
{
  boolean do_read = false;

  // update time structures and variables
  t = now();
  sprintf(sd_dir, "%04d/%02d", year(t), month(t));
  sprintf(sd_file, "%s/%02d.txt", sd_dir, day(t));
    
  switch(read_rate)
  {
    case 0: // 1 reading / 1 second
      do_read = (second(t) != last_sec);
      break;
    case 1: // 1 reading / 5 seconds
      do_read = (second(t) != last_sec && !(second(t) % 5));
      break;
      
    case 2: // 1 reading / 30 seconds
      do_read = (second(t) != last_sec && !(second(t) % 30));
      break;  
      
    case 3: // 1 reading / 1 minute
      do_read = (!second(t) && minute(t) != last_min);
      break;
      
    case 4: // 1 reading / 15 minutes
      do_read = (!second(t) && minute(t) != last_min && !(minute(t) % 15));
      break;
      
    case 5: // 1 reading / 30 minutes
      do_read = (!second(t) && minute(t) != last_min && !(minute(t) % 30));
      break;
      
    case 6: // 1 reading / 1 hour
      do_read = (!second(t) && minute(t) != last_min && !minute(t));
      break;
  }
  
  if(do_read)
  {
    if(read_meters() && write_date() && send_data()); 

    last_min = second(t);
    last_sec = minute(t);
  }

  // Did anyone make a web request? 
  handle_web_requests();
}

boolean read_meters()
{
  int result;
    
  for(int i = 0; i < meter_count; i++)
  {
    for(int j; j < MAX_MEASURES; j++)
      readings[i][j] = 0;
    
    if(meter_type[i] == 1)
    {
      result = Modbus.readHoldingRegisters(modbus_id[i], 119, 28);
      if(result == (int)Modbus.MBSuccess)
      {
        // NOTE: the 0.001 will need to change depending on how your meter's CT and PT ratios are set up
        readings[i][MTYPE_W] = (float)Modbus.getResponseBuffer(0) * 0.1; //* 0.0001;
        readings[i][MTYPE_WH] = (float)LONG((long)Modbus.getResponseBuffer(19), (long)Modbus.getResponseBuffer(18))  - //* 0.001 -
                         (float)LONG((long)Modbus.getResponseBuffer(21), (long)Modbus.getResponseBuffer(20)); // * 0.001;
      }
    }
    //else if(!strcmp(meter_type[i], "???????"))
    // ... add other meter models here....
  
    delay(10);
  }
  
  return true;
}

boolean write_date()
{
  if(!SD.mkdir(sd_dir))
  {
    Serial.print("ERROR: (D3) unable to create SD card dir: ");
    Serial.println(sd_dir);
    return false;
  }

  // write data to log and temp unsent log
  if(!write_log(sd_file, 11))
    return false;
  if(!write_log(sd_unsent, 12))
    return false;

  // write out the json code we want to sent over http
  if(!write_json(sd_unsent, sd_json, 13))
    return false;
  
  return true;
}

void print_it(Print &printer)
{
  for(int i = 0; i < meter_count; i++)  
  {
    printer.print("{\"meter\": \"");
    printer.print(meter_id[i]);
    printer.print("\", \"ts\": \"");
    printer.print(year(t));
    printer.print("-");
    
    if(month(t) < 10) printer.print('0');
    printer.print(month(t));
    
    printer.print("-");

    if(day(t) < 10) printer.print('0');
    printer.print(day(t));

    printer.print(" ");

    if(hour(t) < 10) printer.print('0');
    printer.print(hour(t));

    printer.print(":");

    if(minute(t) < 10) printer.print('0');
    printer.print(minute(t));

    printer.print(":");

    if(second(t) < 10) printer.print('0');
    printer.print(second(t));

    printer.print(" UTC\", ");
    
    for(int j = 0; j < MAX_MEASURES; j++)
    {
      printer.print("\"");
      printer.print(measure_types[j]);
      printer.print("\": "); 
      printer.print(readings[i][j]);
      printer.print(", ");
    }
    printer.print("},\r\n");
  }
}

boolean write_log(char *fname, byte errno)
{
  File fp;
  
  if(!(fp = SD.open(fname, FILE_WRITE)))
  {
    Serial.print("ERROR: (");
    Serial.print(errno);
    Serial.print(") unable to open SD card file: ");
    Serial.println(fname);
    return false;
  }

  print_it(fp);

  fp.close();
  return true;
}

boolean write_json(char *source_file, char *target_file, byte errno)
{
  File sfp;
  File tfp;

  SD.remove(target_file);
  
  if(!(tfp = SD.open(target_file, FILE_WRITE)))
  {
    Serial.print("ERROR: (");
    Serial.print(errno);
    Serial.print(") unable to open target SD card file: ");
    Serial.println(target_file);
    return false;
  }
  
  if(!(sfp = SD.open(source_file, FILE_READ)))
  {
    Serial.print("ERROR: (");
    Serial.print(errno);
    Serial.print(") unable to open source SD card file: ");
    Serial.println(source_file);
    return false;
  }
        
  tfp.print("{\"metering\": {");
  tfp.print("\"home\": \"");
  tfp.print(home_id);
  tfp.print("\", ");
  tfp.print("\"readings\": [\r\n");

  while(sfp.available())
    tfp.write(sfp.read());

  tfp.print("] } }\r\n");
        
  sfp.close();
  tfp.close();
  return true;
}

boolean send_file(char *fname, EthernetClient client)
{
  File fp;

  if(!(fp = SD.open(fname, FILE_READ)))
  {
    Serial.print("ERROR: (D3) unable to open SD card file: ");
    Serial.println(fname);
    return false;
  }

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.print("Content-Length: ");
  client.println(fp.size());
  client.println();
      
  while(fp.available())
    client.write(fp.read());

  fp.close();
  delay(1);
  return true;
}

boolean send_data()
{
  File fp;
  EthernetClient web_server;
  char *ok_response = "SUCCESS\n";
  char text[9] = {000000000};
    
  if(!(fp = SD.open(sd_json, FILE_READ)))
  {
    Serial.print("ERROR: (D1) unable to open SD card file: ");
    Serial.println(sd_json);
    return false;
  }
  
  if(!web_server.connect(ws_host, ws_port)) 
  {
    Serial.print("ERROR: (D2) unable to connect to web server: ");
    Serial.print(ws_host);
    Serial.print(", port: ");
    Serial.println(ws_port);
    return false;
  }
  
  web_server.print("POST ");
  web_server.print(ws_url);
  web_server.println(" HTTP/1.1");
  web_server.print("Host: ");
  web_server.println(ws_host);
  web_server.print("Content-Length: ");
  web_server.println(fp.size());
  web_server.println("Content-Type: text/plain");
  web_server.println("Connection: close");
  web_server.println();
  
  while(fp.available())
    web_server.write(fp.read());
  fp.close();
  
  while(web_server.connected()) 
  {
    if(web_server.available())
    {
      char c = web_server.read();
    
      for(int i = 0; i < 7; i++)
        text[i] = text[i+1];      
      text[7] = c;
    }
  } 

  // send successfully, delete unsent temp file
  if(!strcmp(text, ok_response))
  {
    SD.remove(sd_unsent);
  }
  else
  {
    Serial.print("ERROR: (D4) unable POST to web service");
    return false;
  }  
  
  web_server.stop();  
  delay(1);
  return true;
}

void print_html_sep(EthernetClient client)
{
  client.print("<br/><br/>");
}

void print_html_input(EthernetClient client, char *prompt, char *name, int inst, int len, char *value, char *eg)
{
  if(prompt != NULL)
  {
    client.print(prompt);
    client.print(":&nbsp;");
  }
  
  client.print("<input type=\"text\" name=\"");
  client.print(name);
  if(inst > 0)
    client.print(inst);
  client.print("\" size=\"");
  client.print(len);
  client.print("\" maxlength=\"");
  client.print(len);
  client.print("\" value=\"");
  client.print(value);
  client.print("\"/>");
  
  if(eg != NULL)
  {
    client.print("&nbsp;&nbsp;e.g. ");
    client.print(eg);
  }
}

void print_html_input_set(EthernetClient client, char *prompt, char *nameprefix, int len, byte arr[], int arrsize, char *sep, boolean prnhex)
{ 
  client.print(prompt);
  client.print(":&nbsp;");

  for(int i = 0; i < arrsize; i++)
  {
    client.print("<input type=\"text\" style=\"text-align:center\" name=\"");
    client.print(nameprefix);
    client.print(i+1);
    client.print("\" size=\"");
    client.print(len);
    client.print("\" maxlength=\"");
    client.print(len);
    client.print("\" value=\"");

    if(arr != NULL && !prnhex)
    { 
      client.print(arr[i]);
    }
    else if(arr != NULL && prnhex)
    { 
      client.print(arr[i], HEX);
    }

    client.print("\"/>");
    
    if(i < arrsize - 1)
      client.print(sep);
  }
}

void write_settings_page(EthernetClient client)
{
  char num[8];
  int i;
  
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println();
  
  client.print("<html><body>");
  client.print("<form action=\"http://");
  client.print(Ethernet.localIP());
  client.print("/settings\" method=\"post\">");
  client.print("<hr/><h1 style=\"text-align:center\";> APMR Setting Configuration </h1><hr/>");
  print_html_sep(client);
          
  print_html_input_set(client, "MAC address", "M", 2, mac, sizeof(mac), "&nbsp;:&nbsp;", true);
  client.print("<blockquote><b>Note:</b>&nbsp;<em>To have APMR use a fixed IP, configure the your DHCP server to assign once based on the above MAC address.</em></blockquote>");
 
  print_html_input(client, "Web server hostname", "HN", 0, 32, ws_host, "my.server.com");
  print_html_sep(client);

  num[0] = 0;
  String(ws_port).toCharArray(num, sizeof(num));
  print_html_input(client, "Web server port", "PN", 0, 5, num, "80 (default)");
  print_html_sep(client);

  print_html_input(client, "URL path", "path", 0, 64, ws_url, "/ws/save.py");
  print_html_sep(client);

  client.print("Serial console baud rate:&nbsp;"); 
  client.print("<select name=\"cs_rate\">");
  for(i = 0; i < sizeof(baud_rates) / sizeof(long); i++)
  {
    client.print("<option value=\"");
    client.print(i);
    client.print("\"");

    if(console_baud_rate == i)
      client.print(" selected=\"selected\"");

    client.print(">");
    client.print(baud_rates[i]);
    client.print(" </option>"); 
  }
  client.print("</select>");
  print_html_sep(client);

  client.print("RS485/Modbus baud rate:&nbsp;"); 
  client.print("<select name=\"mb_rate\">");
  for(i = 0; i < sizeof(baud_rates) / sizeof(long); i++)
  {
    client.print("<option value=\"");
    client.print(i);
    client.print("\"");

    if(rs485_baud_rate == i)
      client.print(" selected=\"selected\"");

    client.print(">");
    client.print(baud_rates[i]);
    client.print(" </option>"); 
  }
  client.print("</select>");
  print_html_sep(client);

  client.print("Meter reading rate (1 reading per):&nbsp;");
  client.print("<select name=\"r_rate\">");
  for(i = 0; i < sizeof(read_rates) / sizeof(char *); i++)
  {
    client.print("<option value=\"");
    client.print(i);
    client.print("\"");
    
    if(read_rate == i)
      client.print(" selected=\"selected\"");

    client.print(">");
    client.print(read_rates[i]);
    client.print(" </option>"); 
  }
  client.print("</select>");
  print_html_sep(client);
 
  print_html_input(client, "Database HOME ID", "H_id", 0, 4, home_id, NULL);
  print_html_sep(client);

  client.print("Configuration for meters to be read: ");
  client.print("<blockquote><table border=\"1\"><tr><th> Meter Number </th><th> MODBUS ID </th><th> METER ID </th><th> METER TYPE </th></tr>");      
  for(i = 0; i < MAX_METERS; i++)
  {
    client.print("<tr><td align=\"center\"> #");
    client.print(i+1);
    client.print("</td><td align=\"center\">");
    
    num[0] = 0;
    if(modbus_id[i] > 0)
      String(modbus_id[i]).toCharArray(num, sizeof(num));
      
    print_html_input(client, NULL, "mbID", i+1, 3, num, NULL);
    client.print("</td><td align=\"center\">");
    
    print_html_input(client, NULL, "mID", i+1, 4, meter_id[i], NULL);
    client.print("</td><td align=\"center\">");
            
    client.print("<select name=\"t");
    client.print(i+1);
    client.print("\">");
    for(int j = 0; j < sizeof(meter_types) / sizeof(char *); j++)
    {
      client.print("<option value=\"");
      client.print(j);
      client.print("\"");
      
      if(meter_type[i] == j)
        client.print(" selected=\"selected\"");
      
      client.print(">");
      client.print(meter_types[j]);
      client.print("</option>");
      
    }
    client.print("</select>");
    client.print("</td></tr>");        
  }
  client.print("</table></blockquote>");
  print_html_sep(client);

  client.print("<input type=\"submit\" value=\"Save Settings\"/>");
  client.print("<hr/>");
  client.print("</form></body></html>");
}

void send_dirinfo(EthernetClient client)
{
  long fsize = 0;
  File root = SD.open("/");

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println();
  client.print("<html><body>");
  client.print("<b>Arduino Power Meter Reader (APMR)</b><br/><br/>SD card file listings for: /<br/><br/><pre>");
    
  long filled = print_dir(root, 0, "", client);
  
  client.print("</pre><br/><br/>Total space used: ");
  client.print(filled);
  client.println(" Bytes");
  client.print("</body></html>");
}

long print_dir(File dir, int numTabs, char *root, EthernetClient client) 
{
  long fsize = 0;
  char sd_dir[16] = {0000000000000000};
  char ffsize[8] = {00000000};
  
  while(true)
  {
    File entry =  dir.openNextFile();

    if(!entry) 
    break;

    for(uint8_t i=0; i<numTabs; i++) 
      client.print("\t");

    if(entry.isDirectory()) 
    {
      client.print(entry.name());
      client.println("/");
      client.print("</a>");
      sprintf(sd_dir, "%s/%s", root, entry.name());
      fsize += print_dir(entry, numTabs + 1, sd_dir, client);
    } 
    else 
    {
      sprintf(sd_dir, "%s", root);
      client.print("<a href=\"/files");
      client.print(sd_dir);
      client.print("/");
      client.print(entry.name());
      client.print("\">");
      client.print(entry.name());
      client.print("</a>");


      client.print("</a>");
      
      if(strlen(root) <= 1)
        client.print("\t\t");
      else
        client.print("\t");
      
      sprintf(ffsize, "% 7d", entry.size());
      client.print(ffsize);
      client.println(" Bytes");
      fsize += entry.size();
    }
  }
 
  return fsize;
}

// Send JSON text to browser/computer that has made a request
void handle_web_requests()
{
  EthernetClient client = server.available();
  char line[81];
  int i;

  if(client) 
  {
    // read in first line only to get method and url requested
    for(i = 0; i < 81; i++)
    {
      if(!client.connected())
        return;
        
      line[i] = client.read();
      
      if(line[i] == '\n')
        break;
    }
    line[++i] = 0;
    
    // read in remain lines of the header    
    boolean is_blank_line = true;
    while(client.connected()) 
    {
      if(client.available()) 
      {
        char c = client.read();        
        if(c == '\n' && is_blank_line) 
        {
          break;
        }
        
        if(c == '\n') 
          is_blank_line = true;
        else if(c != '\r')
          is_blank_line = false;          
      }
    }
    
    //send response
    if(strstr(line, "GET /settings ") != 0)
    {
      write_settings_page(client);
    }
    else if(strstr(line, "POST /settings ") != 0)
    {
      write_settings(client);
    }    
    else if(strstr(line, "GET /unsent ") != 0)
    {
      if(!send_file(sd_unsent, client))
      {
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/plain");
        client.println();
        client.println("No unsent data.");
      }
    }
    else if(strstr(line, "GET /files ") != 0)
    {
      send_dirinfo(client);
    }    
    else if(strstr(line, "GET /files/") != 0)
    {
      char *ptr = &line[11];
      strchr(ptr, ' ')[0] = 0;
      send_file(ptr, client);      
    }    
    else
    {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/plain");
      client.println();
      
      print_it(client);
    }

    delay(1);
    client.stop();
  }
} 

void write_eeprom(char *var1, char *var2, int itype, char *val, int addr, int maxlen)
{
  byte v = 0;
  word v2 = 0;
  int i = 0;

  if(strcmp(var1, var2) || val == NULL)
    return;
    
  switch(itype)
  {
    case ITYPE_INT:
      if(maxlen == 1)
      {
        v = atoi(val);
        EEPROM.write(addr, v);
      }
      else if(maxlen == 2)
      {
        v2 = atoi(val);
        EEPROM.write(addr, (byte)(v2 >> 8));
        EEPROM.write(addr + 1, (byte)v2);
      }
      break;

    case ITYPE_HEX:
      v = htoi(val);
      EEPROM.write(addr, v);
      break;
      
    case ITYPE_STR:
      for(i = 0; i < strlen(val) && i < maxlen; i++)
      {
        v = val[i];
        EEPROM.write(addr+i, v);
      }
   
      for(; i < maxlen; i++)
      {  
        EEPROM.write(addr+i, 0);
      }
      break;
  }
}

int htoi (char *ptr)
{
  int value = 0;
  char ch = *ptr;

  while (ch == ' ' || ch == '\t')
    ch = *(++ptr);

  for(;;) 
  {
    if(ch >= '0' && ch <= '9')
      value = (value << 4) + (ch - '0');
    else if(ch >= 'A' && ch <= 'F')
      value = (value << 4) + (ch - 'A' + 10);
    else if(ch >= 'a' && ch <= 'f')
      value = (value << 4) + (ch - 'a' + 10);
    else
      return value;
    ch = *(++ptr);
  }
}

void write_settings(EthernetClient client)
{
  char line[81];
  int i;
  int counter = 0;

  Serial.print("Saving new settings");

  while(client.connected()) 
  {
    if(client.available()) 
    {
      line[counter++] =  client.read(); 
      if(line[counter - 1] == '&')//counter > 0 && 
      {
        line[counter - 1] = 0;
        char *var = strtok(line, "=");
        char *val = strtok(NULL, "="); 
        
        String s = String(val);
        char buf[64];
        s.replace("\%2F", "/");
        s.replace("\%3A", ":");
        s.toCharArray(buf, sizeof(buf));
        val = &buf[0];
        
        write_eeprom("M1", var, ITYPE_HEX, val, 1, 1);
        write_eeprom("M2", var, ITYPE_HEX, val, 2, 1);
        write_eeprom("M3", var, ITYPE_HEX, val, 3, 1);
        write_eeprom("M4", var, ITYPE_HEX, val, 4, 1);
        write_eeprom("M5", var, ITYPE_HEX, val, 5, 1);
        write_eeprom("M6", var, ITYPE_HEX, val, 6, 1);
        
        write_eeprom("HN", var, ITYPE_STR, val, 22, 32);
        
        write_eeprom("PN", var, ITYPE_INT, val, 54, 2);

        write_eeprom("path", var, ITYPE_STR, val, 56, 64);
        
        write_eeprom("cs_rate", var, ITYPE_INT, val, 120, 1);
        
        write_eeprom("mb_rate", var, ITYPE_INT, val, 121, 1);
        
        write_eeprom("r_rate", var, ITYPE_INT, val, 122, 1);
        
        write_eeprom("H_id", var, ITYPE_STR, val, 123, 4);

        write_eeprom("mbID1", var, ITYPE_INT, val, 127, 1);
        write_eeprom("mID1", var, ITYPE_STR, val, 128, 4);
        write_eeprom("t1", var, ITYPE_INT, val, 132, 1);
        
        write_eeprom("mbID2", var, ITYPE_INT, val, 133, 1);
        write_eeprom("mID2", var, ITYPE_STR, val, 134, 4);
        write_eeprom("t2", var, ITYPE_INT, val, 138, 1);
        
        write_eeprom("mbID3", var, ITYPE_INT, val, 139, 1);
        write_eeprom("mID3", var, ITYPE_STR, val, 140, 4);
        write_eeprom("t3", var, ITYPE_INT, val, 144, 1);
        
        write_eeprom("mbID4", var, ITYPE_INT, val, 145, 1);
        write_eeprom("mID4", var, ITYPE_STR, val, 146, 4);
        write_eeprom("t4", var, ITYPE_INT, val, 150, 1);
                            
        write_eeprom("mbID5", var, ITYPE_INT, val, 151, 1);
        write_eeprom("mID5", var, ITYPE_STR, val, 152, 4);
        write_eeprom("t5", var, ITYPE_INT, val, 156, 1);
        
        write_eeprom("mbID6", var, ITYPE_INT, val, 157, 1);
        write_eeprom("mID6", var, ITYPE_STR, val, 158, 4);
        write_eeprom("t6", var, ITYPE_INT, val, 162, 1);
        
        write_eeprom("mbID7", var, ITYPE_INT, val, 163, 1);
        write_eeprom("mID7", var, ITYPE_STR, val, 164, 4);
        write_eeprom("t7", var, ITYPE_INT, val, 168, 1);
        
        write_eeprom("mbID8", var, ITYPE_INT, val, 169, 1);
        write_eeprom("mID8", var, ITYPE_STR, val, 170, 4);
        write_eeprom("t8", var, ITYPE_INT, val, 174, 1);
        
        write_eeprom("mbID9", var, ITYPE_INT, val, 175, 1);
        write_eeprom("mID9", var, ITYPE_STR, val, 176, 4);
        write_eeprom("t9", var, ITYPE_INT, val, 180, 1);
        
        write_eeprom("mbID10", var, ITYPE_INT, val, 181, 1);
        write_eeprom("mID10", var, ITYPE_STR, val, 182, 4);
        write_eeprom("t10", var, ITYPE_INT, val, 186, 1);
        
        write_eeprom("mbID11", var, ITYPE_INT, val, 187, 1);
        write_eeprom("mID11", var, ITYPE_STR, val, 188, 4);
        write_eeprom("t11", var, ITYPE_INT, val, 192, 1);
        
        write_eeprom("mbID12", var, ITYPE_INT, val, 193, 1);
        write_eeprom("mID12", var, ITYPE_STR, val, 194, 4);
        write_eeprom("t12", var, ITYPE_INT, val, 198, 1);
        
        write_eeprom("mbID13", var, ITYPE_INT, val, 199, 1);
        write_eeprom("mID13", var, ITYPE_STR, val, 200, 4);
        write_eeprom("t13", var, ITYPE_INT, val, 204, 1);
        
        write_eeprom("mbID14", var, ITYPE_INT, val, 205, 1);
        write_eeprom("mID14", var, ITYPE_STR, val, 206, 4);
        write_eeprom("t14", var, ITYPE_INT, val, 210, 1);
        
        write_eeprom("mbID15", var, ITYPE_INT, val, 211, 1);
        write_eeprom("mID15", var, ITYPE_STR, val, 212, 4);
        write_eeprom("t15", var, ITYPE_INT, val, 216, 1);
        
        write_eeprom("mbID16", var, ITYPE_INT, val, 217, 1);
        write_eeprom("mID16", var, ITYPE_STR, val, 218, 4);
        write_eeprom("t16", var, ITYPE_INT, val, 222, 1);

        counter = 0;
        Serial.print(".");
      } 
    }
    else
    {
      break;
    }
  }  

  EEPROM.write(0, 1);
  Serial.println("DONE!");
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println();
  client.println("Success, settings saved! Arduino will now reset...");
  delay(500);
  client.stop();
  software_reset();
}

void software_reset()
{
  Serial.println("Arduino will now reset...");
  delay(500);
  asm volatile ("  jmp 0");  
}  

byte eeprom_read(int address)
{
  byte val = EEPROM.read(address);
  
  if(val == 255)
    val = 0;
    
  return val;
}

void read_settings()
{
  int i;
  int j;
  
  if(eeprom_read(0) != 1)
  {
    Serial.print("Settings are not configured!! Please go to: http://");
    Serial.print(Ethernet.localIP());
    Serial.println("/settings");
    return;
  }
  else
  {
    for(i=0; i<6; i++)
    {
      mac[i] = eeprom_read(1+i);   
    }
    
    for(i=0 ; i < sizeof(ws_host)-1; i++)
    {
      ws_host[i] = eeprom_read(22+i);
    }
    
    ws_port =  (int)(eeprom_read(54) << 8) +  eeprom_read(55);

    for(i=0 ; i<sizeof(ws_url)-1; i++)
    {
      ws_url[i] = eeprom_read(56+i);
    }
    
    console_baud_rate = eeprom_read(120);
    
    rs485_baud_rate = eeprom_read(121);
    
    read_rate = eeprom_read(122);
     
    for(i=0 ; i<sizeof(home_id)-1; i++)
    {
      home_id[i] = eeprom_read(123+i);
    }
 
    meter_count = 0;
    int rowsize = 6;
    for(j=0; j<MAX_METERS ; j++)
    {
      modbus_id[j] = eeprom_read(127+rowsize*j);
      if(modbus_id[j] != 0 && modbus_id[j] != 255) // When input for MODBUS ID is not empty, do the following
      {
        for(i=0 ; i<sizeof(meter_id[j])-1; i++)
        {
          meter_id[j][i] = eeprom_read(128+(rowsize*j)+i);
        }
        meter_type[j] = eeprom_read(132+(rowsize*j));

        meter_count++;
      }
    }
  }
}

