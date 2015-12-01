#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>

#define MAX_CONNECTION 20
#define MESSAGE_LENGTH 256

char *status_list[] = {"off", "on"};
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct 
{
	char IP[16], Port[7], Area[5];
	bool isSensor;
	int sockid;
	int lastValue;
	int identifier;
	bool isON;
}sens_dev;

typedef struct
{
	int sockid;
	struct sockaddr addr;
	int addr_len;
}connection_ds;

int connCount = 0; 
int SensorCount = 0;
FILE *devStatus;
sens_dev connList[MAX_CONNECTION];

char GPort[7],GIP[20];
FILE *file_output;

pthread_t thread, thread_setVal;
int sockfd;

connection_ds *CurrClient = NULL;
bool killer = false;

void * setTime()
{
	char msg[MESSAGE_LENGTH];
	int setInterval = 0;
	int i,sensorid;
	int option;
	bool flag ;

	while(true)
	{
		printf("Do you want to change the interval? (1: Yes, 0: No)\n");
		scanf("%d",&option);

		if(option == 0)
			continue;

		flag = false;

		printf("ID: IP Address : Port: Area Code\n");

		for(i=0;i<connCount;i++)
		{	
			if(connList[i].isSensor==true)
			{
				flag = true;
				//printf("Index: %d\n", i);
				printf("%d. %s : %s : %s \n",connList[i].identifier,connList[i].IP,connList[i].Port,connList[i].Area);
			}

		}
		if(!flag)
		{
			printf("Sorry, list is empty\n");
			continue;
		}
		printf("Enter Sensor ID to change interval : \n");
		scanf("%d", &sensorid);

		printf("Enter the Interval to set :\n");
		scanf("%d", &setInterval);

		sprintf(msg,"Type:setInterval;Action:%d",setInterval);
		printf("%s\n",msg);
		
		for(i=0; i < connCount; i++)
		{	
			if(connList[i].identifier == sensorid)
				break;
		}
		write(connList[i].sockid, msg, sizeof(msg)); 				
	}	
}

void * connection(void *clnt)
{
	int client = *(int*)clnt;
		
	int message_size;
	char message[MESSAGE_LENGTH];
	char status[4];
	char message_type[10];
	char action[100];
	char type_temp[10];
	
	int value,i,j,ind;
	char temp_area[3];
	bool flag_to_on = false;
	
	if(file_output==NULL)
	{
		printf("Error opening file\n");
		exit(1);
	}

	while(true)
	{
		if(killer)
			break;

		bzero(message,MESSAGE_LENGTH);

		if((message_size = read(client,message,256))<0)
		{
			perror("Received Message Failed");
			return 0;
		}

		if(message_size != 0)
		{
			memset(action,0,100);
			sscanf(message, "Type:%[^;];Action:%s", message_type,action);
			if(strcmp("currValue", message_type) == 0)	//to check type of a msg 
			{
			
				value = atoi(action);

				for(i=0;i<connCount;i++)
				{
					if(connList[i].sockid == client)
					{
						break;
					}
				}
				connList[i].lastValue = value;
				strcpy(temp_area,connList[i].Area);
				
				for(i=0; i < connCount;i++)
				{
					if(strcmp(connList[i].Area, temp_area) == 0)
					{
						if(connList[i].isSensor == false)
						{
							break;
						}
					}	
				}

				if(value < 32)	//to set the heater
				{

					if(i != connCount && (connList[i].isON == false))
					{
						write(connList[i].sockid, "Type:Switch;Action:on", sizeof("Type:Switch;Action:on")); 
						strcpy(status,"ON");
						connList[i].isON = true;
					}
				}
				if(value > 34)
				{
					if(i != connCount)
					{
						flag_to_on = true;
						for(j=0; j<connCount; j++)
						{
							if(((strcmp(connList[j].Area,connList[i].Area))==0) && connList[j].isSensor==true && connList[j].sockid!=client)
							{	
								if(connList[j].lastValue < 34)
								{
									flag_to_on = false;
									break;	
								}	

							}
							
						}
						if(flag_to_on && connList[i].isON)
						{
							write(connList[i].sockid, "Type:Switch;Action:off", sizeof("Type:Switch;Action:on")); 
							strcpy(status,"OFF");
							connList[i].isON = false;	
						}
					}
				}
						
				pthread_mutex_lock(&mutex);
				fprintf(file_output,"-------------------------------------------------------\n");
				fflush(file_output);

				for(ind = 0; ind < connCount; ind++)
				{
					if(connList[ind].isSensor)
					{
						fprintf(file_output,"%d ---- %s:%s ---- sensor ---- %s ---- %d\n",connList[ind].sockid,connList[ind].IP,connList[ind].Port,connList[ind].Area,connList[ind].lastValue);
						fflush(file_output);
					}
					else
					{
							fprintf(file_output,"%d ---- %s:%s ---- device ---- %s ---- %s\n",connList[ind].sockid,connList[ind].IP,connList[ind].Port,connList[ind].Area,status_list[connList[ind].isON]);
							fflush(file_output);
					}
				}

				fprintf(file_output,"-------------------------------------------------------\n");
				fflush(file_output);
				pthread_mutex_unlock(&mutex);

				bzero(message,MESSAGE_LENGTH);				

			}
			else if(strcmp("currState", message_type) == 0)
			{
				printf("Currstate Gateway received message!\n");
			}
			else if(strcmp("register", message_type) == 0)
			{
				sscanf(action, "%[^-]-%[^-]-%[^-]-%s", type_temp, connList[connCount].IP, connList[connCount].Port, connList[connCount].Area);
				connList[connCount].sockid = client;
				if(strcmp(type_temp, "sensor") == 0)
				{
					connList[connCount].isSensor = true;
					connCount++;
					SensorCount++;
					connList[connCount].identifier = SensorCount;

					//printf("connList[connCount].identifier: %d, SensorCount: %d\n", connList[connCount].identifier, SensorCount );
				}
				else
				{
					connList[connCount].isSensor = false; 
					connList[connCount].identifier = -1;
					connCount++;

					break;
				}
			}
		}
		else
		{
			break;
		}
	}
	
	return 0;
}

void  KillHandler(int sig)
{
	killer = true;
	signal(sig, SIG_IGN);
	pthread_mutex_lock(&mutex);
	pthread_mutex_unlock(&mutex);
	kill(getpid(),SIGKILL);

}

void InitConfig(char *fname)
{
	FILE *cfg;
	cfg = fopen(fname,"r");
	if(!cfg)
	{
		printf("Configuration File not found\n");
		exit(1);
	}
	fscanf(cfg,"%[^:]:%s\n",GIP,GPort);
	fclose(cfg);					
}

void TryConnection(char *fname)
{
	struct sockaddr_in server;
	//initialize Socket
	int userThread = 0;
	int yes=1,clnt=0;
	int temp_sockfd;

	sockfd = socket(AF_INET,SOCK_STREAM,0);
	//Socket Creation

	if(sockfd < 0)
	{
		perror("Socket Creation Failed");
		exit(1);
	}	

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(atoi(GPort));	

	if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes))==-1)
	{
		perror("Socket opt Failed");
		close(sockfd);
		exit(1);
	}

	if(bind(sockfd,(struct sockaddr*)&server,sizeof(server))<0)
	{
		perror("Binding Failed");
		close(sockfd);
		exit(1);
	}
	
	listen(sockfd,3);
	file_output=fopen(fname,"w");

	if(!file_output)
	{
		printf("File Output opening Failed\n");
		exit(1);
	}
	printf("Gateway Started Successfully \n");

	CurrClient = (connection_ds*) malloc (sizeof(connection));
	while((CurrClient->sockid = accept(sockfd, &CurrClient->addr, (socklen_t*)&CurrClient->addr_len)))
	{
		temp_sockfd = CurrClient->sockid;
		if((pthread_create(&thread,NULL,connection,(void*)&temp_sockfd))!=0)
		{
			perror("Thread Creation Failed");
		}

		if(userThread == 0)
		{

			if((pthread_create(&thread_setVal,NULL,setTime,NULL)!=0))
			{
				perror("Thread Creation Failed");
			}

			userThread=1;	
		}
	}

	if(clnt < 0)
	{
		perror("Accept Failed");
		close(sockfd);
		exit(1);
	}

	fclose(file_output);
	close(sockfd);
				
}


int main(int argc, char *argv[])
{

	if(argc < 3)
	{
		printf("Please provide exact number of arguments\n");
		return 0;
	}

	signal(SIGINT, KillHandler); 
	signal(SIGTSTP, KillHandler); 
	InitConfig(argv[1]);
	TryConnection(argv[2]);
	getchar();
	return 0;
}