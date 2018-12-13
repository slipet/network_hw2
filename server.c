#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>


#define MAX_BUFFER 1024
typedef struct {
    fd_set serverReadFds;
    int socketFd;
    int clientSockets[MAX_BUFFER];
    int numClients;
    pthread_mutex_t *clientListMutex;

} chatData;


typedef struct {
    chatData *data;
    int clientSocketFd;
} clientHandlerVars;
typedef struct{
	char name[20];
	int client;
}loginInfo;

void startChat(int socketFd);

void bindSocket(struct sockaddr_in *serverAddr, int socketFd, long port);
void removeClient(chatData *data, int clientSocketFd);

void *newClientHandler(void *data);
void *clientHandler(void *chv);

loginInfo login[MAX_BUFFER];
int main(int argc, char *argv[])
{
    struct sockaddr_in serverAddr;
    long port = 9999;
    int socketFd;

    if(argc == 2) port = strtol(argv[1], NULL, 0);

    if((socketFd = socket(AF_INET, SOCK_STREAM, 0))== -1)
    {
        perror("Socket creation failed");
        exit(1);
    }

    bindSocket(&serverAddr, socketFd, port);
    if(listen(socketFd, 1) == -1)
    {
        perror("listen failed: ");
        exit(1);
    }

    startChat(socketFd);
    
    close(socketFd);
}
//Spawns the new client handler thread and message consumer thread
void startChat(int socketFd)
{
    chatData data;
    data.numClients = 0;
    data.socketFd = socketFd;
//    data.queue = queueInit();
    data.clientListMutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(data.clientListMutex, NULL);

    //Start thread to handle new client connections
    pthread_t connectionThread;
	for(int i=0;i<MAX_BUFFER;++i)
	{	
		//login[i].name=NULL;
		login[i].client=-1;
	}	
    if((pthread_create(&connectionThread, NULL, (void *)&newClientHandler, (void *)&data)) == 0)
    {
        fprintf(stderr, "Connection handler started\n");
    }

    FD_ZERO(&(data.serverReadFds));
    FD_SET(socketFd, &(data.serverReadFds));


    pthread_join(connectionThread, NULL);

    pthread_mutex_destroy(data.clientListMutex);
    free(data.clientListMutex);
}
void bindSocket(struct sockaddr_in *serverAddr, int socketFd, long port)
{
    memset(serverAddr, 0, sizeof(*serverAddr));
    serverAddr->sin_family = AF_INET;
    serverAddr->sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr->sin_port = htons(port);

    if(bind(socketFd, (struct sockaddr *)serverAddr, sizeof(struct sockaddr_in)) == -1)
    {
        perror("Socket bind failed: ");
        exit(1);
    }
}
void removeClient(chatData *data, int clientSocketFd)
{
    pthread_mutex_lock(data->clientListMutex);
    for(int i = 0; i < MAX_BUFFER; i++)
    {
        if(data->clientSockets[i] == clientSocketFd)
        {
			strcpy(login[i].name , "");
			login[i].client = -1;
            data->clientSockets[i] = 0;
            close(clientSocketFd);
            data->numClients--;
            i = MAX_BUFFER;
        }
    }
    pthread_mutex_unlock(data->clientListMutex);
}
void *newClientHandler(void *data)
{
    chatData *chatdata = (chatData *) data;
    while(1)
    {
        int clientSocketFd = accept(chatdata->socketFd, NULL, NULL);
        if(clientSocketFd > 0)
        {
            fprintf(stderr, "Server accepted new client. Socket: %d\n", clientSocketFd);

            //Obtain lock on clients list and add new client in
            pthread_mutex_lock(chatdata->clientListMutex);
			
			char msgBuffer[MAX_BUFFER];
			int numBytesRead = read(clientSocketFd,msgBuffer,MAX_BUFFER - 1);
			msgBuffer[numBytesRead] = '\0';
			
            if(chatdata->numClients < MAX_BUFFER)
            {
                //Add new client to list
                for(int i = 0; i < MAX_BUFFER; i++)
                {
                    if(!FD_ISSET(chatdata->clientSockets[i], &(chatdata->serverReadFds)))
                    {
                        chatdata->clientSockets[i] = clientSocketFd;
                        i = MAX_BUFFER;
                    }
                }

                FD_SET(clientSocketFd, &(chatdata->serverReadFds));
				if(FD_ISSET(clientSocketFd,&(chatdata->serverReadFds)))
				{
					for(int i=0;i<MAX_BUFFER;++i)
					{
						
						
						if(login[i].client==-1)
						{
								strcpy(login[i].name , msgBuffer);
								login[i].name[strlen(msgBuffer)-1]='\0';
								login[i].client = clientSocketFd;
								
								break;
						}

					}
				}
                //Spawn new thread to handle client's messages
                clientHandlerVars chv;
                chv.clientSocketFd = clientSocketFd;
                chv.data = chatdata;

                pthread_t clientThread;
                if((pthread_create(&clientThread, NULL, (void *)&clientHandler, (void *)&chv)) == 0)
                {
                    chatdata->numClients++;
                    fprintf(stderr, "Client has joined chat. Socket: %d\n", clientSocketFd);
                }
                else
                    close(clientSocketFd);
            }
            pthread_mutex_unlock(chatdata->clientListMutex);
        }
    }
}


void *clientHandler(void *chv)
{
    clientHandlerVars *vars = (clientHandlerVars *)chv;
    chatData *data = (chatData *)vars->data;

    int clientSocketFd = vars->clientSocketFd;

    char msgBuffer[MAX_BUFFER];
    while(1)
    {
		memset(&msgBuffer,0,sizeof(msgBuffer));
        int numBytesRead = read(clientSocketFd, msgBuffer, MAX_BUFFER - 1);
        msgBuffer[numBytesRead] = '\0';

        //If the client sent /exit\n, remove them from the client list and close their socket
		if((strcmp(msgBuffer,"/exit\n")==0) ||(strncmp(msgBuffer,"/broadcast ",11)==0) || strcmp(msgBuffer,"/who\n")==0 || strncmp(msgBuffer,"/send ",5)==0 || (strncmp(msgBuffer,"/trans ",7)==0))
		{
			if(strcmp(msgBuffer, "/exit\n") == 0)
        	{
            	fprintf(stderr, "Client on socket %d has disconnected.\n", clientSocketFd);
            	removeClient(data, clientSocketFd);
            	return NULL;
        	}
			else if(strncmp(msgBuffer,"/send ",5)==0)
			{
				
				int isExist = -1;
				int sendFd = 0;
				char sendto[100];
				char tmp [1024];
				

				pthread_mutex_lock(data->clientListMutex);
				
				strcpy(msgBuffer,msgBuffer+6);
				int spceOffset= strchr(msgBuffer,' ')-msgBuffer;
				if(spceOffset > 0)
				{
					strncpy(sendto,msgBuffer,spceOffset);
					strcpy(msgBuffer,msgBuffer+spceOffset);
					
					
					
					for(int i=0;i<MAX_BUFFER;++i)
					{
						if(strcmp(sendto,login[i].name)==0 && isExist ==-1)
						{
							isExist = 1;
							sendFd = login[i].client;
							
							
						}
					}
					if(isExist == 1)
					{
					
					for(int i=0;i<MAX_BUFFER;++i)
					{
						if(clientSocketFd == login[i].client)
						{
							strcpy(sendto,login[i].name);
							
						}
					}					
					strcpy(tmp,"[");
					strcat(tmp,sendto);
					strcat(tmp,"]:");
					strcat(tmp,msgBuffer);
					strcpy(msgBuffer,tmp);
										
					write(sendFd,msgBuffer,MAX_BUFFER);
					fprintf(stderr,"%s",msgBuffer);
					}
					else 
					{
						write(clientSocketFd,"Eorror\n",MAX_BUFFER);
					}
				}
				
				pthread_mutex_unlock(data->clientListMutex);

			}
        	else if(strcmp(msgBuffer,"/who\n")==0)
        	{
				
            	//Obtain lock, push message to queue, unlock, set condition variable
            	pthread_mutex_lock(data->clientListMutex);
				char tmp[1024];
				int t=-1;
				for(int i=0;i<MAX_BUFFER;++i)
				{
					if(login[i].client!=-1)
					{
						if(t==-1)
						{
							strcpy(tmp,login[i].name);
							strcat(tmp,"\n");
							t=1;
						}
						else
						{
							strcat(tmp,login[i].name);
							strcat(tmp,"\n");
						}

					}
				}
				write(clientSocketFd,tmp,MAX_BUFFER);
            	pthread_mutex_unlock(data->clientListMutex);
            	//pthread_cond_signal(q->notEmpty);
        	}
			else if(strncmp(msgBuffer,"/broadcast ",11)==0)
			{

				
				

					char cli[100];
					strcpy(cli,"Broadcast ");

					for(int i=0;i<MAX_BUFFER;++i)
					{
						if(clientSocketFd==login[i].client)
						{
							strcat(cli,"[");
							strcat(cli,login[i].name);
							strcat(cli,"]: ");
							break;
						}
					}

					strcpy(msgBuffer,msgBuffer+11);
					strcat(cli,msgBuffer);
					strcpy(msgBuffer,cli);

					pthread_mutex_lock(data->clientListMutex);
					
					for(int i=0;i<MAX_BUFFER;++i)
					{
						if(login[i].client != -1)
						{
							write(login[i].client,msgBuffer,MAX_BUFFER);
						}
					}

					pthread_mutex_unlock(data->clientListMutex);
					

				
			}
			else if(strncmp(msgBuffer,"/trans ",7)==0)
			{
				int isExist = -1;
				int sendFd = 0;
				char sendto[100];
				char filename [MAX_BUFFER];
				char request[]="Do you want to receive a file from ";
				char acceptFile[]="/get ";
				
				fprintf(stderr,"ok\n");
				pthread_mutex_lock(data->clientListMutex);
				
				strcpy(msgBuffer,msgBuffer+7);
				int spceOffset= strchr(msgBuffer,' ')-msgBuffer;
				fprintf(stderr,"%s\n",msgBuffer);
				if(spceOffset > 0)
				{
					strncpy(sendto,msgBuffer,spceOffset);
					strcpy(msgBuffer,msgBuffer+spceOffset);
					
					
					
					for(int i=0;i<MAX_BUFFER;++i)
					{
						if(strcmp(sendto,login[i].name)==0 && isExist ==-1)
						{
							isExist = 1;
							sendFd = login[i].client;
							
						}
					}
					if(isExist == 1)
					{
					
					for(int i=0;i<MAX_BUFFER;++i)
					{
						if(clientSocketFd == login[i].client)
						{
							strcpy(sendto,login[i].name);
							
						}
					}	

					strcpy(filename,msgBuffer+1);
					filename[strlen(filename)-1]='\0';
					
					
					strcat(request,sendto);
					strcat(request,"?(y/n) ");
					
					write(sendFd,request,MAX_BUFFER);
					pthread_mutex_unlock(data->clientListMutex);
					
					pthread_mutex_lock(data->clientListMutex);
					
					numBytesRead = read(sendFd, msgBuffer, MAX_BUFFER - 1);
        			msgBuffer[strlen(msgBuffer)-1] = '\0';
					
					pthread_mutex_unlock(data->clientListMutex);
					if(strcmp(msgBuffer,"y")==0)
					{
				
						strcat(acceptFile,filename);
						
						pthread_mutex_lock(data->clientListMutex);
						write(clientSocketFd,acceptFile,MAX_BUFFER);
						pthread_mutex_unlock(data->clientListMutex);						
					
						pthread_mutex_lock(data->clientListMutex);					
						numBytesRead = read(clientSocketFd, msgBuffer, MAX_BUFFER - 1);
        				msgBuffer[strlen(msgBuffer)-1] = '\0';					
						pthread_mutex_unlock(data->clientListMutex);
						
						if(strncmp(msgBuffer,"/file ",6)==0)
						{
							
							char tmp[]="/file ";
							strcat(tmp,filename);
							strcat(tmp," ");
							strcat(tmp,msgBuffer + 6);
							strcpy(msgBuffer,tmp);
							
							pthread_mutex_lock(data->clientListMutex);
							
							write(sendFd,msgBuffer,MAX_BUFFER);
							pthread_mutex_unlock(data->clientListMutex);						
						
						}
					}
					else
					{
						pthread_mutex_lock(data->clientListMutex);
						write(clientSocketFd,"The file was rejected.\n",MAX_BUFFER);
						pthread_mutex_unlock(data->clientListMutex);
					}
					
					}
					else 
					{
						pthread_mutex_lock(data->clientListMutex);
						write(clientSocketFd,"Eorror\n",MAX_BUFFER);
						pthread_mutex_unlock(data->clientListMutex);
					}
				}
				
				
			}
			else
			{
			char hint[MAX_BUFFER];
			strcpy(hint,"Please enter message with these rules\n");
			strcat(hint,"/who\n");
			strcat(hint,"/broadcast [message]\n");
			strcat(hint,"/send [username] [message]\n");
			strcat(hint,"/trans [username] [file]\n");

			
			pthread_mutex_lock(data->clientListMutex);
			write(clientSocketFd,hint,MAX_BUFFER);
			pthread_mutex_unlock(data->clientListMutex);
						
			}
    	}
	
	}
}