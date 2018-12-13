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
#include <signal.h>
#include <sys/stat.h>

#define MAX_BUFFER 1024

void chatloop(int socketFd);
void buildMessage(char *result,char *msg);
void setupAndConnect(struct sockaddr_in *serverAddr,struct hostent *host,int socketFd,long port);
void setNonBlock(int fd);
void interruptHandler(int sig);


static int socketFd;

int main(int argc,char *argv[])
{
	
	struct sockaddr_in serverAddr;
	struct hostent *host;
	long port;
	
	if(argc!=3)
	{
		fprintf(stderr,"./client [username] [host] [port]\n");
		exit(1);
	}
	
	
	if((host = gethostbyname(argv[1]))==NULL)
	{
		fprintf(stderr,"Couldn't get host name\n");
		exit(1);
			
	}
	port = strtol(argv[2],NULL,0);
	if((socketFd = socket(AF_INET,SOCK_STREAM,0))==-1)
	{
		fprintf(stderr,"Couldn't creat socket");
		exit(1);
	}
	setupAndConnect(&serverAddr,host,socketFd,port);
	setNonBlock(socketFd);
	setNonBlock(0);
	
	signal(SIGINT,interruptHandler);
	
	chatloop(socketFd);
	
}
void chatloop(int socketFd)
{
	fd_set clientFds;
	char chatMsg[MAX_BUFFER];
	char chatBuffer[MAX_BUFFER],msgBuffer[MAX_BUFFER];
	
	while(1)
	{
		FD_ZERO(&clientFds);
		FD_SET(socketFd,&clientFds);
		FD_SET(0,&clientFds);
		if(select(FD_SETSIZE,&clientFds,NULL,NULL,NULL)!=-1)
		{
			for(int fd=0;fd<FD_SETSIZE;fd++)
			{
				if(FD_ISSET(fd,&clientFds))
				{
					
					if(fd == socketFd)
					{
						memset(&msgBuffer,0,sizeof(msgBuffer));
						int numBytesRead = read(socketFd,msgBuffer,MAX_BUFFER - 1);
						msgBuffer[numBytesRead] ='\0';
						
						if(strncmp(msgBuffer,"/get ",5)==0)
						{
							char filename[MAX_BUFFER];
							strcpy(filename,msgBuffer + 5);
							
							
							char fileBuffer[MAX_BUFFER];
							char upload[]="/file ";
							
							int fd = open(filename,O_RDONLY);
							
							if(fd < 0)
							{
								fprintf(stderr,"open file failed:%s\n",filename);
								break;
							}
							int rBytes=read(fd,fileBuffer,MAX_BUFFER);
							if(rBytes >MAX_BUFFER)
							{
								perror("The file is too large\n");
								break;
							}
							strcat(upload,fileBuffer);
							if(strlen(upload) >MAX_BUFFER)
							{
								perror("The file is too large\n");
								break;
							}
							int wBytes=strlen("/file ")+strlen(fileBuffer);
							if((wBytes=write(socketFd,upload,MAX_BUFFER-1))<0)perror("write failed\n");
							
							close(fd);
						}
						else if(strncmp(msgBuffer,"/file ",6)==0)
						{
							char filename[MAX_BUFFER];
							int fd;
							int offset=0;
							memset(&filename,0,MAX_BUFFER);
							strcpy(msgBuffer,msgBuffer + 6);

							offset = strchr(msgBuffer,' ')-msgBuffer;

							strncpy(filename,msgBuffer,offset);
							strcpy(msgBuffer,msgBuffer+offset+1);
						
							if((fd=open(filename,O_CREAT,S_IRWXU))<0)
							{
								perror("creat file failed\n");
								
							}
							close(fd);
							if((fd=open(filename,O_WRONLY))<0)
							{
								perror("open file error\n");
								break;
							}
							if(write(fd,msgBuffer,strlen(msgBuffer))<0)
							{
								perror("write file error\n");
								break;
							}

							close(fd);
						}
						else
							fprintf(stderr,"%s",msgBuffer);
						
					}
					else if(fd==0)
					{
						memset(&chatBuffer,0,sizeof(chatBuffer));
						fgets(chatBuffer,MAX_BUFFER - 1,stdin);
						if(strcmp(chatBuffer,"/exit\n")==0)
							interruptHandler(-1);
						else
						{
							strcpy(chatMsg,chatBuffer);
							if(write(socketFd,chatMsg,MAX_BUFFER -1)==-1)perror("write failed: ");
							
							memset(&chatBuffer,0,sizeof(chatBuffer));
						}
						
					}
				}
			}
		}
	
	}
}

void setupAndConnect(struct sockaddr_in *serverAddr,struct hostent *host ,int socketFd,long port)
{
	memset(serverAddr,0,sizeof(serverAddr));
	serverAddr->sin_family = AF_INET;
	(serverAddr->sin_addr) = *((struct in_addr*)host->h_addr_list[0]);
	serverAddr->sin_port = htons(port);
	fprintf(stderr,"Please enter your name : ");
	if(connect(socketFd,(struct sockaddr *)serverAddr,sizeof(struct sockaddr))<0)
	{
		perror("Couldn't connect to serveer");
		exit(1);
	}

}

void setNonBlock(int fd)
{
	int flags = fcntl(fd,F_GETFL);
	if(flags <0)
		perror("fcntl failed");
	
	flags |= O_NONBLOCK;
	fcntl(fd,F_SETFL,flags);

}
void interruptHandler(int sig_unused)
{

	if(write(socketFd,"/exit\n",MAX_BUFFER-1)==-1)
		perror("write failed : ");
	
	close(socketFd);
	exit(1);
}




