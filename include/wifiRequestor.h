#ifndef IR_WIFIREQUESTOR_H
#define IR_WIFIREQUESTOR_H

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

void WifiInit()
{
  server.begin();
}

bool ParseRequest(String request, LabelDef* label)
{
  if(request.length() == 0)
    return false;

  Serial.printf("\nParseRequest: Request: %s\n", request.c_str());
  
  int startOfQueryString = request.indexOf('?') + 1;
  if(startOfQueryString < 2)
    return false;

  String querystring = request.substring(startOfQueryString);
  Serial.printf("ParseRequest: QueryString: %s\n", querystring.c_str());
  
  LinkedList<String> keyValuePairs;

  int from = 0;
  while (from < querystring.length())
  {
    int end = querystring.indexOf('&', from);
    if (end == -1)
      end = querystring.length();

    String keyValuePair = querystring.substring(from, end);
    keyValuePairs.Append(keyValuePair);
    
    Serial.printf("ParseRequest: KeyValuePair: %s\n", keyValuePair.c_str());

    from = end + 1;
  }

  if (keyValuePairs.getLength() < 1)
    return false;

  do
  {
    String pair = keyValuePairs.getCurrent();
    char key = pair[0];
    String value = pair.substring(2);

    switch (key)
    {
    case 'r':
      label->registryID = value.toInt();
      break;
    case 'o':
      label->offset = value.toInt();
      break;
    case 'c':
      label->convid = value.toInt();
      break;
    case 'd':
      label->dataSize = value.toInt();
      break;
    case 't':
      label->dataType = value.toInt();
      break;
    }
  } while (keyValuePairs.next());

  if(label->registryID == 0)
    return false;
    
  return true;
}

void getValue(LabelDef *label, Converter *converter, WiFiClient *wifiClient, unsigned char *data)
{
  unsigned char *input = data;
  input += label->offset + 3;
  converter->convert(label, input);

  bool alpha = false;
  for (size_t j = 0; j < strlen(label->asString); j++)
  {
    char c = label->asString[j];
    if (!isdigit(c) && c != '.' && !(c == '-' && j == 0))
    {
      alpha = true;
      break;
    }
  }

  char output[200];
  if (alpha)
  {
    wifiClient->printf("\"%s\":\"%s\",", label->label, label->asString);
  }
  else
  { // number, no quotes
    wifiClient->printf("\"%s\":%s,", label->label, label->asString);
  }
}

void CheckWifiRequest()
{
  WiFiClient wifiClient = server.available();

  if (!wifiClient)
    return;

  wifiClient.setTimeout(1);

  String request = "";
  while (wifiClient.connected())
  {
    if (wifiClient.available())
      request += wifiClient.readString();
    else
      break;
  }

  LabelDef definition;
  if(!ParseRequest(request, &definition))
    return;

  Serial.printf("ParseRequest: Register %d\n", definition.registryID);
  Serial.printf("ParseRequest: Offset %d\n", definition.offset);
  Serial.printf("ParseRequest: Conversion %d\n", definition.convid);
  Serial.printf("ParseRequest: DataSize %d\n", definition.dataSize);
  Serial.printf("ParseRequest: Type %d\n", definition.dataType);
  Serial.printf("ParseRequest: Label %s\n", definition.label);

  // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
  // and a content-type so the client knows what's coming, then a blank line,
  // followed by the content:
  wifiClient.println("HTTP/1.1 200 OK");
  wifiClient.println("Content-type:text/html");
  wifiClient.println();

  unsigned char buff[64] = {0};
  if (queryRegistry(definition.registryID, buff))
  {
    Serial.println("CheckWifiRequest: GetValue");
    Converter converter;
    getValue(&definition, &converter, &wifiClient, buff);
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