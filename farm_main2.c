#include <stdio.h>			
#include <stdlib.h>		
#include <string.h>			
#include <unistd.h>		
#include <wiringPi.h>		
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>	
#include <arpa/inet.h>		

#include "my_types.h"
#include "farm_sensor.h"
#include "farm_query.h"
#include "iot_db.h"

#define PUMP 	21
#define FAN 	22
#define MOTOR	23
#define LED	24

#define DB_NAME "iotfarm"
#define SENSOR_LOG_INTERVAL 1*10
#define ACTUATOR_LOG_INTERVAL 1*60*10; 

void 	initialize();
void 	ClientRecv(int);
int 	parse_message(char * data, Message * msg);
int 	parse_data(char * parameter, Data * _data);

int get_temperature_sensor();
void *SensorInterruptLoop(void *);

pthread_mutex_t MLock;

int _use_pump = 0;
int _use_fan = 0;
int _use_dcmotor = 0;
int _use_rgbled = 0;

int main(int argc, char *argv[])
{
	int 	servSock, clntSock; 
	struct 	sockaddr_in echoServAddr;
	struct 	sockaddr_in echoClntAddr;
	unsigned short 	echoServPort;
	unsigned int 	clntLen;
	int 	iRet;

	int sensor_loop;
	int int_loop_result;

	iot_connect_to_db(DB_NAME);
	printf("validate database..\n");

	iot_send_query(QUERY_CREATE_TABLE_SENSOR_VALUE);
	iot_send_query(QUERY_CREATE_TABLE_SENSOR_CHECK);
	iot_send_query(QUERY_CREATE_TABLE_ACTURATOR_VALUE);
	iot_send_query(QUERY_CREATE_TABLE_ACTURATOR_CHECK);
	iot_send_query(QUERY_CREATE_TABLE_SETTING);

 	iRet = iot_count_setting_data_from_db(QUERY_SELECT_COUNT_SETTING);	
	
	initialize();

	if( argc == 2 )
		echoServPort = atoi( argv[1] );
	else	echoServPort = 11000;


	servSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if( 0 > servSock )
	{ 	printf("socket() failed"); return 0; 	}

	memset(&echoServAddr, 0, sizeof(echoServAddr));
	echoServAddr.sin_family = AF_INET;
	echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	echoServAddr.sin_port = htons(echoServPort);

	iRet = bind(servSock, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr));
	if( iRet < 0 )
	{ 	printf("bind() failed\n"); close(servSock); return 0; }

	iRet = listen(servSock, 5);
	if( iRet < 0 )
	{ 	printf("listen() failed\n"); close(servSock); return 0; }

	clntLen = sizeof( echoClntAddr );

	pthread_mutex_lock (&MLock);
	pthread_create (&sensor_loop, 0, SensorInterruptLoop, &int_loop_result);
	pthread_mutex_unlock (&MLock);

	printf("starting server...\n");
	clntSock = accept(servSock, (struct sockaddr *)&echoClntAddr, &clntLen);
	if( clntSock < 0 )			
	{ 	printf("accept() failed \n"); 	}	

	printf("Handling client ip : %s\n", inet_ntoa(echoClntAddr.sin_addr));
	printf("Handling client port : %d\n", ntohs(echoClntAddr.sin_port));
	printf("Handling client socket number : %d\n", clntSock);


	ClientRecv( clntSock );

	close( clntSock );
	close( servSock );

	iot_disconnect_from_db();
	return 0;
}


void  initialize( )
{
	if( wiringPiSetup() == -1 )
	{ 	printf(" Fail to start wiringPi.. \n"); exit( 0 ); }

	pinMode( PUMP, OUTPUT );
	pinMode( FAN, OUTPUT );
	pinMode( MOTOR, OUTPUT );
	pinMode( LED, OUTPUT );
}


void *SensorInterruptLoop(void *vp)
{
	char _query[256] = {0};

	float _temperature_sensor = 1;
	float _humid_sensor = 1;
	int _light_sensor = 1;


	while(1)
	{
#ifndef TEST_MODE
		_temperature_sensor = get_temperature_sensor();
		_humid_sensor = get_humidity_sensor();
		_light_sensor = get_light_sensor();
#endif
sprintf(_query,QUERY_INSERT_SENSOR_DATA, _temperature_sensor, _humid_sensor, _light_sensor);
		printf("%s : write to DB - %s\n", __func__, _query);
		iot_insert_data_from_db(_query);
		sprintf(_query,QUERY_INSERT_SENSOR_CHECK, _temperature_sensor, _humid_sensor, _light_sensor);
		printf("%s : write to DB - %s\n", __func__, _query);
		iot_insert_data_from_db(_query);
		sprintf(_query,QUERY_INSERT_ACTUATOR_VALUE, _use_pump, _use_fan, _use_dcmotor, _use_rgbled);
		printf("%s : write to DB - %s\n", __func__, _query);
		iot_insert_data_from_db(_query);
#ifndef TEST_MODE
		sprintf(_query,QUERY_INSERT_ACTUATOR_CHECK, get_pump_functionality(), get_fan_functionality(), get_dcmotor_functionality(), get_rgbled_functionality());
		printf("%s : write to DB - %s\n", __func__, _query);
		iot_insert_data_from_db(_query);

#endif
		sleep(SENSOR_LOG_INTERVAL);
	}
}




void  ClientRecv( int  clntSock )						
{
	unsigned char	ucBuff[500];
	unsigned char	ucSBuff[500];
	int		iRet;

	Data *_data;
    	Message *_message;

	_data = (Data *)malloc(sizeof(Data));
    	_message = (Message *)malloc(sizeof(Message));

	while( 1 )
	{
    		memset(ucBuff, 0x0, sizeof(ucBuff));
		iRet = read( clntSock, ucBuff, 500);
		if( iRet < 1 ) 	break;
		printf ("[%d Sock]: [%s] \n", clntSock, ucBuff);

		memset(_message, 0x0, sizeof(Message));
        		iRet = parse_message(ucBuff, _message);

        		printf(" app type : 0x%04x \n", _message->_apptype);
        		printf(" command : 0x%04x \n", _message->_command);
        		printf(" data : %s \n", _message->_data);

		if( _message->_command == GET_SETTING )
		{
			printf(" Arrive a setting message.. \n");
			char *_result = (char *)malloc(sizeof(char)*4096);
			memset( _result, 0x0, 4096);
			strcpy( _result, "12,0,0");

			memset( ucSBuff, 0x0, sizeof(ucSBuff));
			iRet = sprintf(ucSBuff, "%d|%d|%s,%d,%d,%d,%d", _message->_apptype, _message->_command, _result, 0,0 , 0, 0 );
			printf("Send: %s \n", ucSBuff);
			write( clntSock, ucSBuff, iRet );
		}

	
	if(_message->_command == GET_STATUS)
				{
					int _temperature_sensor = 1;
					int _humid_sensor = 1;
					int _light_sensor = 1;
					//TODO:: get sensor data
					//
					_temperature_sensor = get_temperature_sensor();
					_humid_sensor = get_humidity_sensor();
					_light_sensor = get_light_sensor();

					iRet = sprintf (ucSBuff, "%d|%d|%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", _message->_apptype, _message->_command, _temperature_sensor, _humid_sensor, _light_sensor, get_light_functionality(), get_humid_functionality(), get_temp_functionality(), get_pump_functionality(), get_fan_functionality(), get_dcmotor_functionality(), get_rgbled_functionality());
			
				write(clntSock, ucSBuff, iRet);	
				
				}
	

		
		else if(_message->_command == GET_HISTORY)
			{

			printf("Arrive a message for HISTORY... \n");
		
			char * _result = (char *)malloc(sizeof(char)*4096);

			memset(_result, 0x0, 4096);
		
			iot_get_history_data_from_db(QUERY_SELECT_SENSOR_DATA, _result);

			iRet = sprintf (ucSBuff, "%d|%d|%s", _message->_apptype, _message->_command, _result);
			
		//	iRet = sprintf (ucSBuff, "%d|%d", _message->_apptype, _message->_command);
			

			printf("%s\n",ucSBuff);
	
			free(_result);	
			write (clntSock, ucSBuff, iRet);

		}
	
		else if( _message->_command == WATER_SUPPLY )
		{
			printf(" Arrive a message for PUMP.. \n");
			memset(_data, 0x0, sizeof(Data));
			iRet = parse_data(_message->_data, _data);
			digitalWrite( PUMP, _data->_data1 );

			iRet = sprintf(ucSBuff, "%d|%d", _message->_apptype, _message->_command );
			write( clntSock, ucSBuff, iRet );
		}
		else if( _message->_command == WATER_FAN )
		{
			printf(" Arrive a message for FAN.. \n");
			memset(_data, 0x0, sizeof(Data));
			iRet = parse_data(_message->_data, _data);
			digitalWrite( FAN, _data->_data1);

			iRet = sprintf(ucSBuff, "%d|%d", _message->_apptype, _message->_command );
			write( clntSock, ucSBuff, iRet );
		}
		else if( _message->_command == WATER_MOTOR )
		{
			printf(" Arrive a message for MOTOR.. \n");
			memset(_data, 0x0, sizeof(Data));
			iRet = parse_data(_message->_data, _data);
			digitalWrite( MOTOR, _data->_data1);
		       
			iRet = sprintf(ucSBuff, "%d|%d", _message->_apptype, _message->_command );
		       	write( clntSock, ucSBuff, iRet );
		}
		else if( _message->_command == WATER_LED )
		{
			printf(" Arrive a message for LED.. \n");
			memset(_data, 0x0, sizeof(Data));
			iRet = parse_data(_message->_data, _data);
			digitalWrite( LED, _data->_data1);

			iRet = sprintf(ucSBuff, "%d|%d", _message->_apptype, _message->_command );
			write( clntSock, ucSBuff, iRet );
		}

		else 
		{
			printf(" Arrive an unkown message.. \n");
		}

		sleep(1);
	}

	free( _data );
	free( _message );
}


int parse_message(char * data, Message * msg)
{
    char  tmp[128]   = {0};
    char* _parsed_data;
    memcpy(tmp, data, strlen(data));
    
    if(strlen(data) <= 0)
    {
        printf("no data to parse\n");
        return ERROR_PARSE;
    }

    printf("message parsing start :%s \n", tmp);

    _parsed_data = strtok(tmp, "|");
    msg->_apptype = atoi(_parsed_data);

    if(_parsed_data = strtok(NULL, "|"))
        msg->_command = atoi(_parsed_data);
    if(_parsed_data = strtok(NULL, "|"))
        memcpy(msg->_data, _parsed_data, strlen( _parsed_data));

    return TYPE_TRUE;
}


int parse_data(char * parameter, Data * _data)
{
    char* _parsed_data;
    char  tmp[128]   = {0};

    memcpy(tmp, parameter, strlen(parameter));

    if(strlen(parameter) <= 0)
    {
        printf("no data to parse\n");
        return ERROR_PARSE;
    }

    printf("data parsing start :%s \n", parameter);

    _parsed_data = strtok(tmp, ","); _data->_data1 = atoi(_parsed_data);
    if(_parsed_data = strtok(NULL, ",")) _data->_data2 = atoi(_parsed_data);
    if(_parsed_data = strtok(NULL, ",")) _data->_data3 = atoi(_parsed_data);
    if(_parsed_data = strtok(NULL, ",")) _data->_data4 = atoi(_parsed_data);
    if(_parsed_data = strtok(NULL, ",")) _data->_data5 = atoi(_parsed_data);
    if(_parsed_data = strtok(NULL, ",")) _data->_data6 = atoi(_parsed_data);

    return TYPE_TRUE;
}



