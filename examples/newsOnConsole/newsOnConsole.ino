#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include "shoddyxml.h"

const char *ssid = "YOURSSID";
const char *password = "YOURPASSWORD";

const struct site_t {
  const char *title;
  const char *url;
  const char *contentsToDisplay;
} sites[] = {
  {"CNN.com", "http://rss.cnn.com/rss/edition.rss", "title"},
  {"BBC News", "http://feeds.bbci.co.uk/news/rss.xml", "description"},
};

const int delayPerRSS = 10000;

int itemDepth = 0;
int lastTagMatches = 0;
char *contentsToDisplay;

int httpGetChar();

WiFiMulti wifiMulti;
HTTPClient http;
WiFiClient *stream;
shoddyxml x(httpGetChar);

void foundXMLDeclOrEnd() {

}

void foundPI(char *s) {

}

void foundSTag(char *s, int numAttributes, attribute_t attributes[]) {
  if (strcmp(s, "item") == 0) {
    itemDepth++;
  }

  if (strcmp(s, contentsToDisplay) == 0) {
    lastTagMatches = 1;
  } else {
    lastTagMatches = 0;
  }
}

void foundETag(char *s) {
  if ((itemDepth == 1) && (strcmp(s, contentsToDisplay) == 0)) {
    Serial.println("");
  }
  if (strcmp(s, "item") == 0) {
    itemDepth--;
  }
}

void foundEmptyElemTag(char *s, int numAttributes, attribute_t attributes[]) {

}

void foundCharacter(char c) {
  if ((itemDepth == 1) && (lastTagMatches == 1)) {
    Serial.print(c);
  }
}

void foundElement(char *s) {

}

int httpGetChar() {
  if (http.connected()) {
    if (stream->available()) {
      return stream->read();
    } else {
      return 0;
    }
  }
  return EOF;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  /*
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
  */

  wifiMulti.addAP(ssid, password);

  x.foundXMLDecl = foundXMLDeclOrEnd;
  x.foundXMLEnd = foundXMLDeclOrEnd;
  x.foundPI = foundPI;
  x.foundSTag = foundSTag;
  x.foundETag = foundETag;
  x.foundEmptyElemTag = foundEmptyElemTag;
  x.foundCharacter = foundCharacter;
  x.foundElement = foundElement;
}

void loop() {
  for (int i = 0; i < sizeof(sites) / sizeof(struct site_t); i++) {
    if ((wifiMulti.run() == WL_CONNECTED)) {
      itemDepth = 0;
      lastTagMatches = 0;

      Serial.println(sites[i].title);
      contentsToDisplay = const_cast<char*>(sites[i].contentsToDisplay);
      http.begin(sites[i].url);
      int httpCode = http.GET();
      if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
          stream = http.getStreamPtr();
          x.parse();
        }
      }
      http.end();
      delay(delayPerRSS);
    } else {
      wifiMulti.addAP(ssid, password);
    }
  }
}
