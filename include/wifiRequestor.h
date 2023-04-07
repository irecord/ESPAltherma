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

WiFiServer server(80);

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
  WiFiClient wifiClient = server.available();

  if (!wifiClient)
    return;
  
  Serial.println("CheckWifiRequest: Have Client");

  wifiClient.setTimeout(1);

  if (!wifiClient.connected() || !wifiClient.available())
  {
    wifiClient.stop();
    return;
  }

  Serial.println("CheckWifiRequest: Available");
  String request = wifiClient.readStringUntil('\n');

  if (request.length() == 0)
  {
    wifiClient.stop();
    return;
  }

  Serial.println("CheckWifiRequest: Parsing");
  LabelDef definition = ParseUri(request);
  Converter converter;
  unsigned char buff[64] = {0};
  converter.readRegistryValues(buff, 'I'); //process all values from the register

  // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
  // and a content-type so the client knows what's coming, then a blank line,
  // followed by the content:
  wifiClient.println("HTTP/1.1 200 OK");
  wifiClient.println("Content-type:text/html");
  wifiClient.println();

  if(queryRegistry(definition.registryID, buff, 'I'))
  {
    getValue(definition.registryID, &converter, &wifiClient);
  }
  else
  {
    wifiClient.println("Bad Request");
  }

  wifiClient.println();

  Serial.println("CheckWifiRequest: Out");
  wifiClient.stop();
}

#endif