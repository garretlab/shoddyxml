#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "SO2002A_I2C.h"
#include "shoddyxml.h"

/* WiFi setting */
const char *ssid = "YOURSSID";
const char *password = "YOURPASSWORD";

/* URL to get */
const char *site = "http://www.asahi.com/sports/baseball/npb/game/";

/* maximum number of games */
const int maxNumberOfGames = 6;
/* display time for each page in msec */
const int displayTime = 10000;

/* OLED display size: 20x2 */
const int displayHeight = 2;
const int displayWidth = 20;
/* shared variable between tasks */
struct {
  char displayBuffer[maxNumberOfGames][displayHeight][displayWidth + 1];
  int numberOfGames;
} gameDisplayInfo;

/* function to get a web page */
int httpGetChar();

/* score information */
struct {
  char team[2][10];  /* two teams */
  int score[2];  /* two teams */
  char info[2][10]; /* inning, status */
} score[maxNumberOfGames];

/* the number of games in the HTML */
int numGames = 0;

/* team and score has two elements. which one to use */
int teamIndex = 0;
int scoreIndex = 0;

/* HTML analyzing status */
enum {NONE, GAMESTATE, TEAM, SCORE} status;
/* buffer to store data from HTML */
char buffer[64];
int bufferPos;

/* HTML refresh interval in msec */
int refreshInterval = 60000;

/* semaphore */
SemaphoreHandle_t semaphore = NULL;

WiFiMulti wifiMulti;
HTTPClient http;
WiFiClient *stream;
SO2002A_I2C oled(0x3c);
shoddyxml x(httpGetChar);

/* print display buffer. */
/* this function is executed on different task */
void printScores(void *args) {
  int numberOfGames;
  while (1) {
    if (xSemaphoreTake(semaphore, 10)) {
      numberOfGames = gameDisplayInfo.numberOfGames;
      xSemaphoreGive(semaphore);
    }
    for (int i = 0; i < numberOfGames; i++) {
      if (xSemaphoreTake(semaphore, 10)) {
        for (int j = 0; j < 2; j++) {
          oled.setCursor(0, j);
          oled.print(gameDisplayInfo.displayBuffer[i][j]);
        }
        xSemaphoreGive(semaphore);
        if (numberOfGames != gameDisplayInfo.numberOfGames) {
          break;
        }
        delay(displayTime);
        continue;
      }
      delay(1);
    }
  }
}

void setDisplayBufferFixed(char *s1, char *s2) {
  if (xSemaphoreTake(semaphore, 0)) {
    gameDisplayInfo.numberOfGames = 1;
    sprintf(gameDisplayInfo.displayBuffer[0][0], "%-20s", s1);
    sprintf(gameDisplayInfo.displayBuffer[0][1], "%-20s", s2);
    xSemaphoreGive(semaphore);
  }
}

/* set display buffer. should be protected by semaphore */
void setDisplayBuffer() {
  if (xSemaphoreTake(semaphore, 0)) {
    gameDisplayInfo.numberOfGames = numGames;
    if (numGames == 0) {
      gameDisplayInfo.numberOfGames = 1;
      sprintf(gameDisplayInfo.displayBuffer[0][0], "No game available.  ");
      sprintf(gameDisplayInfo.displayBuffer[0][1], "                    ");
    } else {
      for (int i = 0; i < numGames; i++) {

        if (score[i].score[0] == -1) {
          sprintf(gameDisplayInfo.displayBuffer[i][0], "%-9s %10s", score[i].team[0], score[i].info[0]);
          sprintf(gameDisplayInfo.displayBuffer[i][1], "%-9s %10s", score[i].team[1], score[i].info[1]);
        } else {
          sprintf(gameDisplayInfo.displayBuffer[i][0], "%-9s %2d %7s", score[i].team[0], score[i].score[0], score[i].info[0]);
          sprintf(gameDisplayInfo.displayBuffer[i][1], "%-9s %2d %7s", score[i].team[1], score[i].score[1], score[i].info[1]);
        }
      }
    }
    xSemaphoreGive(semaphore);
  }
}

/* clear game status with '\0' */
void clearGameInformation() {
  numGames = 0;
  for (int i = 0; i < maxNumberOfGames; i++) {
    score[i].info[0][0] = '\0';
    score[i].info[1][0] = '\0';
  }
}

/* get game state from HTML*/
/* game state looks like
    ヤクルト 対 中日　終了
*/
void getGameInformation(char *s) {
  int loopNum1, loopNum2;

  struct {
    char *inning;
    char *inningString;
  } inning[] {{"　１回", "1st"}, {"　２回", "2nd"}, {"　３回", "3rd"}, {"　４回", "4th"},
    {"　５回", "5th"}, {"　６回", "6th"}, {"　７回", "7th"}, {"　８回", "8th"},
    {"　９回", "9th"}, {"　１０回", "10th"}, {"　１１回", "11th"}, {"　１２回", "12th"},
  };
  char *headOrTail[] = {"表", "裏"};

  struct {
    char *state;
    char *status[2];
  } gameState[] = {
    {"　試合開始前", "Before", "Game"}, {"　終了", "", "Final"}, {"　中止", "", "Postponed"},
  };

  char t[32];
  loopNum1 = sizeof(inning) / sizeof(inning[0]);
  loopNum2 = sizeof(headOrTail) / sizeof(headOrTail[0]);
  for (int j = 0; j < loopNum1; j++) {
    for (int k = 0; k < loopNum2; k++) {
      strcpy(t, inning[j].inning);
      strcat(t, headOrTail[k]);
      if (strstr(s, t)) {
        strcpy(score[numGames - 1].info[0], inning[j].inningString);
        strcpy(score[numGames - 1].info[1], k == 0 ? "Head" : "Tail");
        return;
      }
    }
  }

  if (strstr(s, "回")) {
    strcpy(score[numGames - 1].info[0], "      ");
    strcpy(score[numGames - 1].info[1], " Extra");
    return;
  }

  loopNum1 = sizeof(gameState) / sizeof(gameState[0]);
  for (int i = 0; i < loopNum1; i++) {
    if (strlen(s) < strlen(gameState[i].state)) {
      continue;
    }
    if (strcmp(s + strlen(s) - strlen(gameState[i].state), gameState[i].state) == 0) {
      strcpy(score[numGames - 1].info[0], gameState[i].status[0]);
      strcpy(score[numGames - 1].info[1], gameState[i].status[1]);
      return;
    }
  }
}

/* convert team name in Japanese to alphabet */
void converTeamName(char *s) {
  /* team name in the HTML and how it be displayed on the OLED display */
  struct {
    char *team;
    char *displayName;
  } teamTable[] = {
    {"巨人", "Giants"}, {"ヤクルト", "Swallows"}, {"中日", "Dragons"},
    {"広島", "Carp"}, {"阪神", "Tigers"}, {"ＤｅＮＡ", "BayStars"},
    {"ソフトバンク", "Hawks"}, {"日本ハム", "Fighters"}, {"ロッテ", "Marines"},
    {"西武", "Lions"}, {"オリックス", "Buffaloes"}, {"楽天", "Eagles"},
  };

  int loopNum = sizeof(teamTable) / sizeof(teamTable[0]);
  for (int i = 0; i < loopNum; i++) {
    if (strcmp(s, teamTable[i].team) == 0) {
      strcpy(score[numGames - 1].team[teamIndex], teamTable[i].displayName);
      teamIndex = 1 - teamIndex; /* invert. 0->1, 1->0 */
      return;
    }
  }
}

/* get score from HTML */
void getScore(char *s) {
  if (strcmp(s, "　") != 0) {
    score[numGames - 1].score[scoreIndex] = atol(s);
  } else {
    score[numGames - 1].score[scoreIndex] = -1;
  }
  scoreIndex = 1 - scoreIndex; /* invert. 0->1, 1->0 */
}

/* check if the tag is for game status */
int checkStatus(char *sTag, attribute_t attributes[]) {
  if ((strcmp(sTag, "div") == 0) &&
      (strcmp(attributes[0].name, "class") == 0) &&
      (strcmp(attributes[0].attValue, "GameState") == 0)) {
    return 1;
  }
  return 0;
}

/* check if the tag is for team name */
int checkTeam(char *sTag, attribute_t attributes[]) {
  if ((strcmp(sTag, "th") == 0) &&
      (strcmp(attributes[0].name, "scope") == 0) &&
      (strcmp(attributes[0].attValue, "row") == 0)) {
    return 1;
  }
  return 0;
}

/* check if the tag is for score */
int checkScore(char *sTag, attribute_t attributes[]) {
  if ((strcmp(sTag, "td") == 0) &&
      (strcmp(attributes[0].name, "class") == 0) &&
      (strcmp(attributes[0].attValue, "Total") == 0)) {
    return 1;
  }
  return 0;
}

void foundXMLDeclOrEnd() {
}

void foundPI(char *s) {
}

void foundSTag(char *sTag, int numAttributes, attribute_t attributes[]) {
  static int teamIndex = 0;

  switch (status) {
    case NONE:
      if (numAttributes && checkStatus(sTag, attributes)) {
        status = GAMESTATE;
        bufferPos = 0;
      } else if (numAttributes && checkTeam(sTag, attributes)) {
        status = TEAM;
        bufferPos = 0;
      } else if (numAttributes && checkScore(sTag, attributes)) {
        status = SCORE;
        bufferPos = 0;
      }
      break;
    case GAMESTATE:
      buffer[bufferPos] = '\0';
      numGames++;
      getGameInformation(buffer);
      status = NONE;
      break;
    default:
      status = NONE;
      break;
  }
}

void foundETag(char *s) {
  switch (status) {
    case GAMESTATE:
      buffer[bufferPos] = '\0';
      numGames++;
      getGameInformation(buffer);
      status = NONE;
      break;
    case TEAM:
      buffer[bufferPos] = '\0';
      converTeamName(buffer);
      status = NONE;
      break;
    case SCORE:
      buffer[bufferPos] = '\0';
      getScore(buffer);
      status = NONE;
      break;
    default:
      break;
  }
}

void foundEmptyElemTag(char *s, int numAttributes, attribute_t attributes[]) {
}

void foundCharacter(char c) {
  switch (status) {
    case GAMESTATE:
    case TEAM:
    case SCORE:
      buffer[bufferPos++] = c;
      break;
    default:
      break;
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
  oled.begin(displayWidth, displayHeight);
  oled.clear();

  wifiMulti.addAP(ssid, password);

  // xml parser setting
  status = NONE;
  x.foundXMLDecl = foundXMLDeclOrEnd;
  x.foundXMLEnd = foundXMLDeclOrEnd;
  x.foundPI = foundPI;
  x.foundSTag = foundSTag;
  x.foundETag = foundETag;
  x.foundEmptyElemTag = foundEmptyElemTag;
  x.foundCharacter = foundCharacter;
  x.foundElement = foundElement;

  semaphore = xSemaphoreCreateMutex();
  setDisplayBufferFixed("Initializing", "Please wait");
  xTaskCreatePinnedToCore(printScores, "printScores", 4096, NULL, 1, NULL, 0);
}

void loop() {
  if ((wifiMulti.run() == WL_CONNECTED)) {
    http.begin(site);
    int httpCode = http.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        clearGameInformation();
        stream = http.getStreamPtr();
        x.parse();
      }
    }
    http.end();
    setDisplayBuffer();
    delay(refreshInterval);
  } else {
    wifiMulti.addAP(ssid, password);
  }
}
