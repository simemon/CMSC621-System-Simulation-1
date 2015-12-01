#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

#include <arpa/inet.h>

#define MESSAGE_LENGTH 256
#define OFF "off"
#define ON "on"
#define TIMER_STATE 30

char GPort[7];
char GIP[30],SDPort[7],SDArea[5],SDIP[16]; 
char action[4] = OFF;
char status[4] = ON;

void registerDevice();
void* data_transfer(void*);
void* sendCurrState(void*);

int TryConnection()
{
	struct sockaddr_in sock;	//To connect to the Gateway
	int clnt;	//client socket FD
	clnt = socket(AF_INET,SOCK_STREAM,0);
	sock.sin_addr.s_addr = inet_addr(GIP);
	sock.sin_family = AF_INET;
	sock.sin_port = htons(atoi(GPort));
	if(clnt < 0)	//Socket Connetion Failed
	{
		perror("Socket Create Failed");
		exit(0);
	}

	
	//Making connection to the socket
	if(connect(clnt,(struct sockaddr*)&sock,sizeof(sock))<0)
	{	
		perror("Connection Error");
		close(clnt);	//Close Socket FD
		exit(0);
	}
	return clnt;
}

/* Configuration File Parsing */
void InitConfiguration(char *filename)
{
	FILE *cfg;
	cfg=fopen(filename,"r");

	if(!cfg)
	{
		printf("Configuration File not found\n");
		exit(1);
	}
	//Tokenize configuration with : separator
	fscanf(cfg,"%[^:]:%s\ndevice:%[^:]:%[^:]:%s",GIP,GPort,SDIP,SDPort,SDArea);
	fclose(cfg);
}

int main(int argc, char *argv[])
{
	int clnt;
	pthread_t pthread_send_state,pthread_data_transfer;

	if(argc < 2)
	{
		printf("Please provide exact number of arguments\n");
		return 0;
	}
	
	InitConfiguration(argv[1]);	//Read Configuration first
	clnt = TryConnection();	//Establish Connection with server first
	registerDevice(clnt);	//Register Smart Device 

	//Created a thread to retrieve message from Server	
	if(pthread_create(&pthread_data_transfer,NULL,data_transfer,(void*)&clnt)!=0)
	{
		perror("Thread Creation Failed");
	}

	//Created a thread to send current state information
	if(pthread_create(&pthread_send_state,NULL,sendCurrState,(void*)&clnt)!=0)
	{
		perror("Thread Creation Failed");
	}
	pthread_join(pthread_send_state,NULL);
	pthread_join(pthread_data_transfer,NULL);
	return 0;
}

void* data_transfer(void* cl)
{
	int clnt;
	char msg[MESSAGE_LENGTH];
	char msg_type[10];
	int msg_size;

	clnt= *(int*)cl; //Typecasting from void to integer
	while(1)
	{	
		bzero(msg,MESSAGE_LENGTH);
		if((msg_size=read(clnt,msg,MESSAGE_LENGTH)) < 0)
		{
			perror("Received Error");
		}
		else if(msg_size > 0)
		{
			sscanf(msg, "Type:%[^;];Action:%s", msg_type,action);	
			printf("Message Read: %s\n",msg);	
		}
		else
		{
			printf("Connection closed by peer\n");
			exit(1);
		}
		
	}
}

void *sendCurrState(void* cl)
{
	int clnt= *(int*)cl;
	char clnt_msg[MESSAGE_LENGTH];

	while(1)
	{
		sleep(TIMER_STATE);

		strcpy(status,action);
		sprintf(clnt_msg,"Type:currState;Action:%s",status);
		if(write(clnt,clnt_msg,strlen(clnt_msg)) < 0)
		{
			perror("Unable to send Message");
		}
		printf("State Message: %s\n", clnt_msg);
	}
}

void registerDevice(int clnt)
{
	char Message[MESSAGE_LENGTH];
	sprintf(Message,"Type:register;Action:device-%s-%s-%s",SDIP,SDPort,SDArea);
	printf("Registration Message : %s\n",Message);
	if(send(clnt,Message,strlen(Message),0) < 0)
	{
		printf("Message Sent Failed\n");
	}
	strcpy(status,ON);
}