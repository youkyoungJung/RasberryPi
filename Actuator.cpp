#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <wiringPi.h>
#define LEDBAR 24
#define FAN 22
`int n;
int ledFlag = 1;
int fanFlag = 1;
void led_handler(int signo)
{
printf(“process stop\n”);
digitalWrite(LEDBAR, 0);
ledFlag = 0;
}
void fan_handler(int signo)
{
printf(“process stop\n”);
digitalWrite(FAN, 0);
fanFlag = 0;
}
int main(void)
{
n = 0;
if(wiringPiSetup() == -1)
{
fprintf(stdout,“Unable to start WiringPi %s\n”, strerror(errno));
return 1;
}
pinMode(LEDBAR, OUTPUT);
pinMode(FAN, OUTPUT);
while(1){
printf(“Select Menu: ”);
scanf(“%d”, &n);
if(n == 1){
signal(SIGINT, (void *)fan_handler);
while(fanFlag){
digitalWrite(FAN, 1);
}
n = 0;
}
else if(n ==2){
signal(SIGINT, (void *)led_handler);
while(ledFlag){
printf(“here – LED BAR on \n”);
digitalWrite(LEDBAR, 1);
delay(200);
printf(“here – LED BAR on \n”);
digitalWrite(LEDBAR, 0);
delay(200);
}
n = 0;
}
else if(n == 3)
break;
n = 0;
}
exit(0);
return 0;
}