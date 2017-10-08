#ifndef SHODDYXML_H
#define SHODDYXML_H

typedef struct {
  char *name;
  char *attValue;
} attribute_t;

class shoddyxml {
  public:
  shoddyxml(int (*getCharFunction)());
  void parse();

  char stringBuffer[32];
  attribute_t *attributes;
  int numAttributes;

  /* user supplied functions */

  int  (*getCharFunction)();
  void (*foundXMLDecl)();
  void (*foundXMLEnd)();
  void (*foundPI)(char *s);
  void (*foundSTag)(char *s, int numAttributes, attribute_t attributes[]);
  void (*foundETag)(char *s);
  void (*foundEmptyElemTag)(char *s, int numAttributes, attribute_t attributes[]);
  void (*foundSection)(char *s);
  void (*beginCharacter)();
  void (*foundCharacter)(char c);
  void (*endCharacter)();
  void (*foundElement)(char *s);

  private:
  enum status_t {
    INXML, LEFTBRACKET, MAYBELBEX, 
    INPI, INSTAG, INETAG, INSECTION, INCDATA, 
    INCHARACTER, MAYBECOMMENT, INCOMMENT, INELEMENT
  } status;
  int subStatus;

  int xmlStarted;
  int sbPosition;
  char lookAhead[4];
  int laPosition;

  int getChar();
  void unGetChar(int c);

  void resetStatus();

  void parseInXML(int c);
  void parseLeftBracket(int c);
  void parseMayBeLBEx(int c);
  void parseMayBeComment(int c);
  void parseInPI(int c);
  void parseInSTag(int c);
  void parseInETag(int c);
  void parseInLBEx(int c);
  void parseInSection(int c);
  void parseInCDATA(int c);
  void parseInCharacter(int c);
  void parseInComment(int c);
  void parseInElement(int c);
};

#endif /* SHODDYXML_H */
