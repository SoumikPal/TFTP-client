#include <stdio.h> 
#include <string.h> 
#include <stdlib.h> 
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#define RRQ 1
#define WRQ 2
#define DATA 3
#define ACK 4
#define ERR 5
static const char* MODE="octet";
#define BUFLEN 516
#define PORT 69
#define MTR 5
void error(char *s)
{
    perror(s);
    exit(1);
}
 
int get_sock_fd()
{
    int sockfd;
    if ((sockfd=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        error("socket");
    }
    return sockfd;
}
void fill_struct(char* server_hostname, int portno ,struct sockaddr_in *serv_addr , int addrlen)
{
    memset((char *)serv_addr, 0, addrlen);
    serv_addr->sin_family = AF_INET;
    serv_addr->sin_port = htons(portno);
     
    if (inet_aton(server_hostname , &(serv_addr->sin_addr)) == 0) 
    {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }    
}
struct sockaddr_in serv_addr;
int sockfd, slen;

void display_help()
{
	printf("SIMPLE TFTP Client\n");
	printf("\nImplementation by @Soumik\n");
	printf("\n\n**USAGE**\n");
	printf("The available commands are as follows. They all have their usual meainings.\n");
	printf("put <filename>\n");
	printf("get <filename>\n");
	printf("quit\n");
	printf("help\n");
	printf("__________\n");

}
void send_file(char *hostname,char *filename)
{
	FILE *fp;
	fp=fopen(filename,"r");
	int flen=strlen(filename);
	if(fp==NULL)
	{
		error("Error in opening a file.");
	}
	char message[516];
	char buf[516];
	bzero(message,516);
	message[0]=0x0;
	message[1]=WRQ;
	strcpy(message+2,filename);
	strcpy(message+2+flen+1,MODE);
	int rlen=2+strlen(filename)+1+strlen(MODE)+1;
	if (sendto(sockfd, message, rlen , 0 , (struct sockaddr *) &serv_addr, slen)==-1)
    {
            error("sendto()");
    }
    printf("Sent WRQ.\n");
    int bnum=0;
    struct sockaddr_in reply_addr ;
    int addrlen= sizeof(reply_addr);
    int i;
    int ack;
    while(1)
    {
    	for(i=1;i<=MTR;i++)
    	{
    		bzero(buf,BUFLEN);
    		if (recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr *) &reply_addr, &addrlen) == -1)
		    {
		        error("recvfrom()");
		    }
		    ack = (buf[2]<<8) + buf[3];
		    if(buf[1]==ERR||ack==(bnum-1))
		    {
		    	printf("Error sending blocknum, trying  again.%d\n", bnum);
		    	if (sendto(sockfd, message, BUFLEN , 0 , (struct sockaddr *) &serv_addr, slen)==-1)
			    {
			            error("sendto()");
			    }
		    }
		    else
		    {
		    	break;
		    }
		    

    	}
    	if(i>MTR)
    	{
    		printf("Giving up on sending file. :( \n" );
    			return ;
    	}
    	printf("	ACK received for block number %d.\n", bnum);
	    fflush(stdout);
	    bnum++;
    	bzero(message, BUFLEN);
    	message[1]=DATA;
    	message[2]=bnum>>8;
		message[3]=bnum%(0xff+1);
		int n=fread(message+4,1,512,fp);
		printf("Sending block %d of %d bytes.\n", bnum,n);
    	if (sendto(sockfd, message, n+4 , 0 , (struct sockaddr *) &reply_addr, addrlen)==-1)
	    {
	            error("sendto()");
	    }

    	if(n<512)
    		break;
    }
    fclose(fp);
    printf("Transfer complete.\n");





    
}
void recv_file(char *hostname ,char *filename){
	FILE *fp;
	fp=fopen(filename,"w");
	int flen=strlen(filename);
	char message[516];
	char buff[516];
	bzero(message,BUFLEN);
	message[0]=0x0;
	message[1]=RRQ;
	strcpy(message+2,filename);
	strcpy(message+2+flen+1,MODE);
	if (sendto(sockfd, message, 516 , 0 , (struct sockaddr *) &serv_addr, slen)==-1)
    {
            error("sendto()");
    }
    printf("Sent RRQ.\n");
    int n;
    int bnum=1;
    struct sockaddr_in reply_addr ;
    int addrlen= sizeof(reply_addr);
    while(1)
    {
    	addrlen= sizeof(reply_addr);
    	bzero(buff,516);
    	n = recvfrom(sockfd, buff, 516, 0, (struct sockaddr *) &reply_addr, &addrlen);
    	if(n==-1)
    	{
    		error("recvfrom()");

    	}
    	n-=4;
    	if(buff[1]==ERR)
    	{
    		error("Server transfer failure");
    	}
    	fwrite(&buff[4],1,n,fp);
    	printf("Received block of size n = %d\n", n);
        bzero(message,BUFLEN);
        message[0]=0x0;
		message[1]=ACK;
		message[2]=bnum>>8;
		message[3]=bnum%(0xff+1);

		if (sendto(sockfd, message, 4 , 0 , (struct sockaddr *) &reply_addr, addrlen)==-1)
	    {
	            error("sendto()");
	    }    	
    	printf("Sent ACK for block %d.\n", bnum);
		
		bnum++;
	    if(n<512)
	    {
	    	break;
	    }



    }
    fclose(fp);
    printf("Transfer complete\n");


}








int main(int argc , char **argv)
{
	if(argc < 2)
    {
        printf("Usage %s server_ip\n", argv[0]);
        exit(0);
    }
    char *server_hostname = argv[1];

    int  i;

    slen=sizeof(serv_addr);
    sockfd = get_sock_fd();
    fill_struct(server_hostname, 69 , &serv_addr , slen);
    struct timeval timeout;      
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    if (setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,
                sizeof(timeout)) < 0)
        error("setsockopt failed\n");

    if (setsockopt (sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout,
                sizeof(timeout)) < 0)
        error("setsockopt failed\n");


	char operation[100] , filename[200];
	while(1)
	{
		printf("pal_tftp > ");
		scanf("%s",operation);
		if(strcmp(operation,"quit")==0)
		{
			break;
		}
		if(strcmp(operation,"help")==0)
		{
			display_help();
			continue;
		}
		
		if(strcmp(operation,"put")==0)
		{
			scanf("%s",filename);
			send_file(server_hostname, filename);
		}

		if(strcmp(operation,"get")==0)
		{
			scanf("%s",filename);
			recv_file(server_hostname, filename);
		}

	}
}
