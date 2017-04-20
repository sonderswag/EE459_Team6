
#ifndef DigitalIo_h
#define DigitalIo_h



#include <avr/io.h>

#define HIGH 1 
#define LOW 0 
#define OUTPUT 1 
#define INPUT 0 


void digitalWrite(char port, char state);
char digitalRead(char port);
void pinMode(char port, char mode);

#endif 