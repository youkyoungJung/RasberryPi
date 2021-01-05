#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <wiringPi.h>
#include <wiringPiSPI.h>

// #include <mysql/mysql.h>
#include <mariadb/mysql.h>

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include <time.h>
#include <math.h>

// #define CS_MCP3208  8        // BCM_GPIO_8

#define SPI_CHANNEL 0
#define SPI_SPEED   1000000   // 1MHz

#define VCC         4.8       // Supply Voltage

// define farm 
#define MAXTIMINGS 85
#define RETRY 5

#define PUMP	21 // BCM_GPIO 5
#define FAN	22 // BCM_GPIO 6
#define DCMOTOR 23 // BCM_GPIO 13
#define RGBLEDPOWER  24 //BCM_GPIO 19
#define RED	7
#define GREEN	8
#define BLUE	9
#define LIGHTSEN_OUT 0  // BCM 17 - J12 connect
#define COLLISION 3 

int ret_humid, ret_temp;

static int DHTPIN = 11;
static int dht22_dat[5] = {0,0,0,0,0};

int read_dht22_dat_temp();
int read_dht22_dat_humid();
int get_temperature_sensor();
int get_humidity_sensor();
int get_light_sensor();
int get_near_detect();

static int read_dht22_dat();

// here

#define DBHOST "localhost"
#define DBUSER "root"
#define DBPASS "root"
#define DBNAME "bae20180972"

MYSQL *connector;
MYSQL_RES *result;
MYSQL_ROW row;

int read_mcp3208_adc(unsigned char adcChannel)
{
  unsigned char buff[3];
  int adcValue = 0;

  buff[0] = 0x06 | ((adcChannel & 0x07) >> 2);
  buff[1] = ((adcChannel & 0x07) << 6);
  buff[2] = 0x00;

//  digitalWrite(CS_MCP3208, 0);  // Low : CS Active

  wiringPiSPIDataRW(SPI_CHANNEL, buff, 3);

  buff[1] = 0x0F & buff[1];
  adcValue = ( buff[1] << 8) | buff[2];

//  digitalWrite(CS_MCP3208, 1);  // High : CS Inactive

  return adcValue;
}


int main (void)
{
  int adcChannel  = 1;
  int adcValue[8] = {0};

  if (wiringPiSetup() == -1)
    exit(EXIT_FAILURE) ;
	
  if (setuid(getuid()) < 0)
  {
    perror("Dropping privileges failed\n");
    exit(EXIT_FAILURE);
  }

  if(wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED) == -1)
  {
    fprintf (stdout, "wiringPiSPISetup Failed: %s\n", strerror(errno));
    return 1 ;
  }
  
//  pinMode(CS_MCP3208, OUTPUT);

  // MySQL connection
  connector = mysql_init(NULL);
  if (!mysql_real_connect(connector, DBHOST, DBUSER, DBPASS, DBNAME, 3306, NULL, 0))
  {
    fprintf(stderr, "%s\n", mysql_error(connector));
    return 0;
  }

  printf("MySQL(rpidb) opened.\n");
  
  while(1)
  {
    char query[1024];

    adcValue[0] = get_light_sensor(); // Illuminance Sensor

	// In case of using the analog value from the light sensor.
//    adcValue[2] = read_mcp3208_adc(1); // Illuminance Sensor

	adcValue[1] = get_near_detect();
    adcValue[2] = get_temperature_sensor();	
    adcValue[3] = read_mcp3208_adc(3); // Mic Sensor
    adcValue[4] = read_mcp3208_adc(4); // Flame Sensor
    adcValue[5] = read_mcp3208_adc(5); // Acceleration Sensor (Z-Axis)
    adcValue[6] = read_mcp3208_adc(6); // Gas Sensor
    adcValue[7] = read_mcp3208_adc(7); // Distace Sensor
    //adcValue[7] = 27*pow((double)(adcValue[7]*VCC/4095), -1.10);

    sprintf(query,"insert into lab10 values (now(),%d,%d)", adcValue[0],adcValue[1]);
    printf("lightsensor: %d, collision: %d\n", adcValue[0], adcValue[1]);

    if(mysql_query(connector, query))
    {
      fprintf(stderr, "%s\n", mysql_error(connector));
      printf("Write DB error\n");
    }

    delay(3000); //1sec delay
  }

  mysql_close(connector);

  return 0;
}

int get_near_detect(void)
{
	int detect;

	pinMode(COLLISION, INPUT);

	while(1)
	{
		if (digitalRead(COLLISION) == 0)
		{
			detect = 0;

			return detect;
		}
		if (digitalRead(COLLISION) == 1)
		{
			detect = 1;
			return detect;

		}
		delay(200);
	}
	return detect;
}

int get_temperature_sensor()
{
	int received_temp;
	int _retry = RETRY;

	//DHTPIN = 11;

	/*if (wiringPiSetup() == -1)
		exit(EXIT_FAILURE) ;*/
	
	if (setuid(getuid()) < 0)
	{
		perror("Dropping privileges failed\n");
		exit(EXIT_FAILURE);
	}
	while (read_dht22_dat_temp() == 0 && _retry--)
	{
		delay(500); // wait 1sec to refresh
	}
	received_temp = ret_temp ;
	printf("Temperature = %d\n", received_temp);
	
	return received_temp;
}

static uint8_t sizecvt(const int read)
{
  /* digitalRead() and friends from wiringpi are defined as returning a value
  < 256. However, they are returned as int() types. This is a safety function */

  if (read > 255 || read < 0)
  {
    printf("Invalid data from wiringPi library\n");
    exit(EXIT_FAILURE);
  }
  return (uint8_t)read;
}

int read_dht22_dat_temp()
{
  uint8_t laststate = HIGH;
  uint8_t counter = 0;
  uint8_t j = 0, i;

  dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

  pinMode(DHTPIN, INPUT);
  pullUpDnControl (DHTPIN, PUD_UP);
  delay(1);

  pinMode(DHTPIN, OUTPUT);
  digitalWrite(DHTPIN, LOW);
  delayMicroseconds(1100);

  // prepar
  pinMode(DHTPIN, INPUT);

  // detect change and read data
  for ( i=0; i< MAXTIMINGS; i++) {
    counter = 0;
    while (sizecvt(digitalRead(DHTPIN)) == laststate) {
      counter++;
      delayMicroseconds(1);
      if (counter == 255) {
        break;
      }
    }
    laststate = sizecvt(digitalRead(DHTPIN));

    if (counter == 255) break;

    // ignore first 3 transitions
    if ((i >= 4) && (i%2 == 0)) {
      // shove each bit into the storage bytes
      dht22_dat[j/8] <<= 1;
      if (counter > 16)
        dht22_dat[j/8] |= 1;
      j++;
    }
  }

  // check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
  // print it out if data is good
  if ((j >= 40) && 
      (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF)) ) {
        float t, h;
		
        h = (float)dht22_dat[0] * 256 + (float)dht22_dat[1];
        h /= 10;
        t = (float)(dht22_dat[2] & 0x7F)* 256 + (float)dht22_dat[3];
        t /= 10.0;
        if ((dht22_dat[2] & 0x80) != 0)  t *= -1;
		
		ret_humid = (int)h;
		ret_temp = (int)t;
		printf("Humidity = %.2f %% Temperature = %.2f *C \n", h, t );
		printf("Humidity = %d Temperature = %d\n", ret_humid, ret_temp);
		
    return ret_temp;
  }
  else
  {
    printf("Data not good, skip\n");
    return 0;
  }
}


int get_humidity_sensor()
{
	int received_humid;
	int _retry = RETRY;

	//DHTPIN = 11;

	/*if (wiringPiSetup() == -1)
		exit(EXIT_FAILURE) ;*/
	
	if (setuid(getuid()) < 0)
	{
		perror("Dropping privileges failed\n");
		exit(EXIT_FAILURE);
	}

	while (read_dht22_dat_humid() == 0 && _retry--) 
	{
		delay(500); // wait 1sec to refresh
	}

	received_humid = ret_humid;
	printf("Humidity = %d\n", received_humid);
	
	return received_humid;
}

int read_dht22_dat_humid()
{
	uint8_t laststate = HIGH;
	uint8_t counter = 0;
	uint8_t j = 0, i;

	dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

  pinMode(DHTPIN, INPUT);
  pullUpDnControl (DHTPIN, PUD_UP);
  delay(1);

  pinMode(DHTPIN, OUTPUT);
  digitalWrite(DHTPIN, LOW);
  delayMicroseconds(1100);

  // prepar
  pinMode(DHTPIN, INPUT);

	// detect change and read data
	for ( i=0; i< MAXTIMINGS; i++) 
	{
		counter = 0;
		while (sizecvt(digitalRead(DHTPIN)) == laststate) 
		{
			counter++;
			delayMicroseconds(1);
			if (counter == 255) {
			break;
		    }
        }
        laststate = sizecvt(digitalRead(DHTPIN));

        if (counter == 255) break;

        // ignore first 3 transitions
        if ((i >= 4) && (i%2 == 0)) 
        {
            // shove each bit into the storage bytes
            dht22_dat[j/8] <<= 1;
            if (counter > 16)
                dht22_dat[j/8] |= 1;
            j++;
        }
    }

  // check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
  // print it out if data is good
	if ((j >= 40) && 
      (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF)) ) {
        float t, h;
		
        h = (float)dht22_dat[0] * 256 + (float)dht22_dat[1];
        h /= 10;
        t = (float)(dht22_dat[2] & 0x7F)* 256 + (float)dht22_dat[3];
        t /= 10.0;
        if ((dht22_dat[2] & 0x80) != 0)  t *= -1;
		
		ret_humid = (int)h;
		ret_temp = (int)t;
		printf("Humidity = %.2f %% Temperature = %.2f *C \n", h, t );
		printf("Humidity = %d Temperature = %d\n", ret_humid, ret_temp);
		
    return ret_humid;
	}
	else
	{
		printf("Data not good, skip\n");
		return 0;
	}
}

int wiringPicheck(void)
{
	if (wiringPiSetup () == -1)
	{
		fprintf(stdout, "Unable to start wiringPi: %s\n", strerror(errno));
		return 1 ;
	}
	return 0;
}

int get_light_sensor(void)
{
	int adcChannel_light = 1;
	int adcValue_light;

	// sets up the wiringPi library
	/*if (wiringPiSetup () < 0) 
	{
		fprintf (stderr, "Unable to setup wiringPi: %s\n", strerror (errno));
		return 1;
	}*/
	//  modified by Ryan
	if(digitalRead(LIGHTSEN_OUT))	//day
		return 1;
	else //night
		return 0;
	
	//adcValue_light = read_mcp3208_adc(adcChannel_light);
//	return adcValue_light;

}

