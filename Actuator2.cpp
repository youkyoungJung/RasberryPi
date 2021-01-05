#include<signal.h>
#include<unistd.h>
#include<stdio.h>
#include<string.h>
#include<errno.h>
#include<stdlib.h>
#include<wiringPi.h>
#define COLLISION 24
#define FAN 22
int alarmFlag = 1;
void (*old_handler)();
void collisionInterrupt(void){
printf(¡°Carefull!!!\n¡±);
old_handler = signal(SIGINT, SIG_IGN);
alarm(6);
digitalWrite(FAN, 1);
}
void alarmHandler()
{
digitalWrite(FAN, 0);
signal(SIGINT, SIG_IGN);
}
int main(void){
if(wiringPiSetup() < 0){
fprintf(stderr, ¡°Unable to setup wiringPi: %s\n¡±, strerror(errno))
return 1;
}
pinMode(COLLISION, INPUT);
pinMode(FANM OUTPUT);
while(1){
if(wiringPiISR(COLLISION, INT_EDGE_RISING, &collisionInterrupt) < 0){
fprintf(stderr, ¡°Unable to setup ISR: %s\n¡±, strerror(errno));
return 1;
}
signal(SIGALRM, alarmHandler);
if(digitalRead(COLLISION) == 1){
delay(3000);
printf(¡°monitoring¡¦\n¡±);
}
}
return 0;
}