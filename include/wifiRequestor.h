#ifndef IR_URIPARSER_H
#define IR_URIPARSER_H

#ifdef ARDUINO_ARCH_ESP8266
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#include <HardwareSerial.h>

#include <Wstring.h>
#include "labeldef.h"
#include "linkedList.h"
#include "comm.h"
#include "converters.h"

LabelDef ParseUri(String request)
{
    String querystring = request.substring(request.indexOf('?') + 1);
    LinkedList<String> keyValuePairs;

    int from = 0;
    while(from < querystring.length())
    {
        int end = querystring.indexOf('&', from);
        if(end == -1)
            end = querystring.length();

        String keyValuePair = querystring.substring(from + 1, end);
        keyValuePairs.Append(keyValuePair);

        from = end;
    }

    if(keyValuePairs.getLength() != 5)
        return LabelDef();

    int Register, Offset, Conversion, DataSize, Type;
    while(keyValuePairs.next())
    {
        String pair = keyValuePairs.getCurrent();
        char key = pair[0];
        String value = pair.substring(2);
        
        switch(key)
        {
            case 'r':
                Register = value.toInt();
                break;
            case 'o':
                Offset = value.toInt();
                break;
            case 'c':
                Conversion = value.toInt();
                break;
            case 'd':
                DataSize = value.toInt();
                break;
            case 't':
                Type = value.toInt();
                break;
        }
    }
    return LabelDef(Register, Offset, Conversion, DataSize, Type, "None");
}

void getValue(char regID, Converter* converter, WiFiClient* wifiClient)
{
  LabelDef *labels[128];
  int num = 0;
  converter->getLabels(regID, labels, num);

  for (int i = 0; i < num; i++)
  {
    bool alpha = false;
    for (size_t j = 0; j < strlen(labels[i]->asString); j++)
    {
      char c = labels[i]->asString[j];
      if (!isdigit(c) && c!='.' && !(c=='-' && j==0)){
        alpha = true;
        break;
      }
    }

    char output[200];
    if (alpha){  
      wifiClient->printf("\"%s\":\"%s\",", labels[i]->label, labels[i]->asString);
    }
    else{//number, no quotes
      wifiClient->printf("\"%s\":%s,", labels[i]->label, labels[i]->asString);
    }
  }
}

void CheckWifiRequest()
{
    int html_get_request;
    Converter converter;
    WiFiServer server(80);
    WiFiClient wifiClient = server.available();
    if (wifiClient) 
    {  
      // force a disconnect after 1 second
      unsigned long timeout_millis = millis()+1000;
      // a String to hold incoming data from the client line by line        
      String currentLine = "";                
      LabelDef definition;
      // loop while the client's connected
      while (wifiClient.connected())
      { 
        // if the client is still connected after 1 second,
        // something is wrong. So kill the connection
        if(millis() > timeout_millis)
        {
          Serial.println("Force Client stop!");  
          wifiClient.stop();
        } 

        // if there's bytes to read from the client,
        if (wifiClient.available()) 
        {             
          char c = wifiClient.read();            
          Serial.write(c);    
          // if the byte is a newline character             
          if (c == '\n') 
          {    
            // two newline characters in a row (empty line) are indicating
            // the end of the client HTTP request, so send a response:
            if (currentLine.length() == 0)
            {
              // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
              // and a content-type so the client knows what's coming, then a blank line,
              // followed by the content:
              switch (html_get_request)
              {
                case 1: {
                  wifiClient.println("HTTP/1.1 200 OK");
                  wifiClient.println("Content-type:text/html");
                  wifiClient.println();

                  unsigned char buff[64] = {0};
                  if(queryRegistry(definition.registryID, buff, 'I'))
                  {
                    converter.readRegistryValues(buff, 'I'); //process all values from the register
                    getValue(definition.registryID, &converter, &wifiClient);
                  }
                  else
                  {
                    wifiClient.println("Bad Request");
                  }

                  wifiClient.println();
                  //wifiClient.write_P(index_html, sizeof(index_html));
                  break;
                }
                
                default:
                  wifiClient.println("HTTP/1.1 404 Not Found");
                  wifiClient.println("Content-type:text/html");
                  wifiClient.println();
                  wifiClient.print("404 Page not found.<br>");
                  break;
              }
              // The HTTP response ends with another blank line:
              wifiClient.println();
              // break out of the while loop:
              break;
            } 
            else
            {    // if a newline is found
              // Analyze the currentLine:
              // detect the specific GET requests:
              if(currentLine.startsWith("GET /")){
                // if no specific target is requested
                if(currentLine.startsWith("GET / ")){
                  html_get_request = 1;
                }
              }
              definition = ParseUri(currentLine);
              currentLine = "";
            }
          } 
          else if (c != '\r')
          {  
            // add anything else than a carriage return
            // character to the currentLine 
            currentLine += c;      
          }
        }
      }
      // close the connection:
      wifiClient.stop();
    }
}

#endif