#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/types.h>
#include <stdbool.h>
#include <unistd.h>

#include <arpa/inet.h>

#define MESSAGE_LENGTH 256
#define MAX_SENSOR 20
#define DEFAULT_INTERVAL 5

struct 
{
	int sockid;
	int interval;
}CurrInterval[MAX_SENSOR];

pthread_t tid;
int clnt;
char GPort[7], GIP[30],SensPort[7],SensArea[5],SensIP[16]; 
int SensorCount = 0;

void InitConfiguration(char *filename)
{
	FILE *cfg;
	cfg = fopen(filename,"r");
	if(!cfg)
	{
		printf("Configuration File not found\n");
		exit(1);
	}
	//Tokenize configuration with : separator
	fscanf(cfg,"%[^:]:%s\nsensor:%[^:]:%[^:]:%s",GIP,GPort,SensIP,SensPort,SensArea);
	fclose(cfg);
}

void* InitParams(void *filename)
{
	FILE *file;
	char msg[100];
	int start_time, val;	
	int current_time = 0, end_time = 0;	//to set 0 to check initial condition
	char temp_str[10];
	int tmp_time = 1, i;
	
	file = fopen(filename,"r");	
	
	if(!file)
	{
		printf("Sensor Input not found\n");
		exit(1);
	}		
	
	while(true)
	{	
		if(current_time >= end_time)
		{
			fscanf(file,"%d;%d;%d\n",&start_time,&end_time,&val);
			if(feof(file))
			{
				fseek(file, 0, SEEK_SET);
				current_time = 0;
				end_time = 0;
			}
		}

		bzero(msg,100);

		sprintf(temp_str,"%d",val);
		
		sprintf(msg, "Type:currValue;Action:%s",temp_str);
		
		if( send(clnt,msg,strlen(msg),0) < 0)
		{
			perror("Message Sent Failed");
		}
		for (i = 0; i < SensorCount; i++)
		{
			if(CurrInterval[i].sockid == clnt)
			{
				tmp_time = CurrInterval[i].interval;
				break;
			}
		}

		current_time += tmp_time;
		sleep(tmp_time);
	}
}

int TryConnection()
{
	struct sockaddr_in sock;
	int clnt;	//Client FD
	clnt = socket(AF_INET,SOCK_STREAM,0);
	if(clnt < 0)	//Socket Connetion Failed
	{
		perror("Socket Create Failed");
		exit(0);
	}
	
	sock.sin_family = AF_INET;
	sock.sin_addr.s_addr = inet_addr(GIP);
	sock.sin_port = htons(atoi(GPort));
	
	if(connect(clnt, (struct sockaddr*)&sock, sizeof(sock)) < 0)
	{	
		perror("Connetion Failed");
		close(clnt);
		exit(-1);
	}
	

	/* If Connection is successful, send connected  socket Failed */
	return clnt;
}

void* setTimeInterval()
{
	char client_message[MESSAGE_LENGTH];
	char setValue[10];
	int i;
	
	while(true)
	{	
		bzero(client_message,MESSAGE_LENGTH);
		if(recv(clnt,client_message,sizeof(client_message),0) < 0)
		{
			perror("Message received Failed");
			exit(1);
		}
		if(strlen(client_message) == 0)
		{
			printf("Connetion Closed by peer\n");
			exit(1);
		}


		printf("MSG: %s\n", client_message);
		sscanf(client_message,"Type:setTimeInterval;Action:%s",setValue);
		
		for (i = 0; i < SensorCount; ++i)
		{
			if(CurrInterval[i].sockid == clnt)
			{
				CurrInterval[i].interval = atoi(setValue);
				break;
			}
		}
	}	

}

void registerDevice(int clnt)
{

	char msg[MESSAGE_LENGTH];

	CurrInterval[SensorCount].sockid = clnt;
	CurrInterval[SensorCount].interval = DEFAULT_INTERVAL;

	SensorCount += 1;

	sprintf(msg,"Type:register;Action:sensor-%s-%s-%s",SensIP,SensPort,SensArea);

	if(send(clnt,msg,strlen(msg),0)<0)
	{
		perror("Message Sent Failed");
	}
	sleep(1);
}


int main(int argc, char *argv[])
{
	if(argc < 3)
	{
		printf("Please provide exact number of arguments\n");
		return 0;
	}

	InitConfiguration(argv[1]);
	
	clnt = TryConnection();
	
	registerDevice(clnt);

	if(pthread_create(&tid,NULL,InitParams,(void*)argv[2]) != 0)
	{
		perror("Thread creation failed");
	}
	if(pthread_create(&tid,NULL,setTimeInterval,(void*)argv[2])!=0)
	{
		perror("Thread creation failed");
	}

	pthread_join(tid, NULL);	

	return 0;
}

