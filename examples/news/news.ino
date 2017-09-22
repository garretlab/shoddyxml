#include "SO2002A_I2C.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include "shoddyxml.h"

#define DISPLAY_WIDTH 20
#define DISPLAY_HEIGHT 2

const char *ssid = "YOURSSID";
const char *password = "YOURPASSWORD";
const struct site_t {
  char *title;
  char *url;
  char *contentsToDisplay;
} sites[] = {
  {"CNN.com", "http://rss.cnn.com/rss/edition.rss", "title"},
  {"BBC News", "http://feeds.bbci.co.uk/news/rss.xml", "description"},
};
const int delayPerCharacter = 200;
const int delayPerArticle = 1000;
const int delayPerRSS = 10000;
const char label = 0xfc;

int itemDepth = 0;
int lastTagMatches = 0;
char displayBuffer[DISPLAY_WIDTH + 1];
char *contentsToDisplay;

int httpGetChar();

WiFiMulti wifiMulti;
HTTPClient http;
WiFiClient *stream;
SO2002A_I2C oled(0x3c);
shoddyxml x(httpGetChar);

void clearDisplayBuffer() {
  for (int i = 0; i < DISPLAY_WIDTH + 1; i++) {
    displayBuffer[i] = ' ';
  }
  displayBuffer[DISPLAY_WIDTH - 1] = label;
}

void displayPutChar(char c) {
  displayBuffer[DISPLAY_WIDTH] = c;
  for (int i = 0; i < DISPLAY_WIDTH; i++) {
    displayBuffer[i] = displayBuffer[i + 1];
  }
}

void printDisplayBuffer() {
  for (int i = 0; i < DISPLAY_WIDTH; i++) {
    oled.setCursor(i, 1);
    oled.print(displayBuffer[i]);
  }
}

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
    for (int i = 0; i < DISPLAY_WIDTH; i++) {
      displayPutChar(' ');
      printDisplayBuffer();
      delay(delayPerCharacter);
    }

    clearDisplayBuffer();
    delay(delayPerArticle);
  }
  if (strcmp(s, "item") == 0) {
    itemDepth--;
  }
}

void foundEmptyElemTag(char *s, int numAttributes, attribute_t attributes[]) {

}

void foundCharacter(char c) {
  if ((itemDepth == 1) && (lastTagMatches == 1)) {
    displayPutChar(c);
    printDisplayBuffer();
    delay(200);
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
  oled.begin(DISPLAY_WIDTH, DISPLAY_HEIGHT);
  oled.clear();

  /*
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
  */

  wifiMulti.addAP(ssid, password);

  clearDisplayBuffer();

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

      oled.clear();
      oled.setCursor(0, 0);
      oled.print(sites[i].title);
      contentsToDisplay = sites[i].contentsToDisplay;
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
