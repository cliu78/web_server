/** @file server.c */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <queue.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <stddef.h>
#include <arpa/inet.h>

#include "queue.h"
#include "libhttp.h"
#include "libdictionary.h"

const char *HTTP_404_CONTENT = "<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1>The requested resource could not be found but may be available again in the future.<div style=\"color: #eeeeee; font-size: 8pt;\">Actually, it probably won't ever be available unless this is showing up because of a bug in your program. :(</div></html>";
const char *HTTP_501_CONTENT = "<html><head><title>501 Not Implemented</title></head><body><h1>501 Not Implemented</h1>The server either does not recognise the request method, or it lacks the ability to fulfill the request.</body></html>";

const char *HTTP_200_STRING = "OK";
const char *HTTP_404_STRING = "Not Found";
const char *HTTP_501_STRING = "Not Implemented";
#define MAX_CONNECTIONS    (10)
#define MAX_LINE           (1000)

/****************************/
pthread_t tid[MAX_CONNECTIONS] ;
int terminator_flag=0;
char IParray[INET_ADDRSTRLEN];
queue_t queue_clientfd;
typedef struct 
{
	int status_code;
	int content_length;
	char content_type[20];
	
} response_t;
void * worker(void * client_fd);
char* write_header(response_t response_info, char* connection );
// sighandler_t signal_handler();
void signal_handler();
int sockfd;
/****************************/

/**
 * Processes the request line of the HTTP header.
 * 
 * @param request The request line of the HTTP header.  This should be
 *                the first line of an HTTP request header and must
 *                NOT include the HTTP line terminator ("\r\n").
 *
 * @return The filename of the requested document or NULL if the
 *         request is not supported by the server.  If a filename
 *         is returned, the string must be free'd by a call to free().
 */
char* process_http_header_request(const char *request)
{
	// Ensure our request type is correct...
	if (strncmp(request, "GET ", 4) != 0)
		return NULL;

	// Ensure the function was called properly...
	assert( strstr(request, "\r") == NULL );
	assert( strstr(request, "\n") == NULL );

	// Find the length, minus "GET "(4) and " HTTP/1.1"(9)...
	int len = strlen(request) - 4 - 9;

	// Copy the filename portion to our new string...
	char *filename = malloc(len + 1);
	strncpy(filename, request + 4, len);
	filename[len] = '\0';

	// Prevent a directory attack...
	//  (You don't want someone to go to http://server:1234/../server.c to view your source code.)
	if (strstr(filename, ".."))
	{
		free(filename);
		return NULL;
	}

	return filename;
}


int main(int argc, char **argv)
{
		//set signal
		signal( SIGINT, signal_handler );
		//signal( SIGKILL, signal_handler_thread);
	    if (argc != 2)
        {
                fprintf(stderr, "Usage: %s [port number]\n", argv[0]);
                return 1;
        }		
		int port = atoi(argv[1]);
		if (port <= 0 || port >= 65536)
		{
			fprintf(stderr, "Illegal port number.\n");
			return 1;
		}

		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		queue_init(&queue_clientfd);

		// struct sockaddr_in my_addr;
		// my_addr.sin_family = AF_INET;
		// my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		// my_addr.sin_port = htons(port);
		struct addrinfo hints, *addrinfo;
		memset(&hints,0,sizeof(hints) );
		hints.ai_flags = AI_PASSIVE;
  		hints.ai_family = AF_INET;
  		hints.ai_socktype = SOCK_STREAM; // TCP socket
  		hints.ai_protocol = 0;//tcp
		if (getaddrinfo(  NULL, argv[1], &hints,&addrinfo)  != 0)
		{
			perror("getaddrinfo");
			exit(1);
		}

		int bind_flag = bind(sockfd,addrinfo->ai_addr, addrinfo->ai_addrlen);
		if (bind_flag < 0)
		{
		perror("binding socket");
		exit(1);
		}
		freeaddrinfo(addrinfo);// no need addrinfo

		int listen_flag =  listen(sockfd, MAX_CONNECTIONS);
		if (listen_flag < 0)
		{
			perror("listen");
			exit(2);
		}
		
		pthread_attr_t attr;
    	pthread_attr_init(&attr);
    	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
		
		int count = 0;
		fd_set master, slave;
		int terminator_fd=0;
		FD_ZERO( &master );
		FD_ZERO( &slave );
		FD_SET( sockfd, &master );
		FD_SET( terminator_fd, &master );
	  	while(1){
	  		slave = master;
			struct sockaddr_in client;
			socklen_t client_len = sizeof(client);
			

			struct timeval t2sec;
				  t2sec.tv_sec = 2;
				  t2sec.tv_usec = 0;
			//int retval = select(sockfd+1, &slave, NULL, NULL, &t2sec );
			int retval = select(FD_SETSIZE, &slave, NULL, NULL, &t2sec );
			if (terminator_flag ==1)
			{
				break;
			}
			if (retval==-1|| retval == 0)
			{
				continue;
			}
			int * client_fd = (int *)malloc(sizeof(int));
			*client_fd = accept(sockfd, (struct sockaddr *) &client,  &client_len);
			//printf("A NEW CLIENT FD%d\n", *client_fd);
			if (*client_fd < 0)
			{
				free(client_fd);
				continue;
			}
			//printf("server gets connection with %d\n", *client_fd);

		
			//int thread_flag = pthread_create(&tid[count], &attr,worker, (void *)(client_fd) );
			int thread_flag = pthread_create(&tid[count], NULL,worker, (void *)(client_fd) );
			if (thread_flag != 0)
			{
				close(*client_fd);
				perror("thread create");
				exit(1);
			}
			//pthread_detach(tid[count]);///-----------?
			count++;

			queue_enqueue( &queue_clientfd, client_fd  );
			
	    	
	  	}
	  	int i;
	  	for (i = 0; i < count; ++i)
		{
			pthread_join(tid[i] , NULL);
		}

	  	//close(sockfd); 
	  	queue_destroy(&queue_clientfd);
		
		return 0;
}

void * worker(void * client_fd_pass)
{
	//signal( SIGINT, signal_handler ); 
	sigset_t mask;
	sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
	pthread_sigmask(SIG_SETMASK, &mask, NULL);
	//int * client_fd = (int*)(client_fd_pass);
	int client_fd = *(int*)(client_fd_pass);
	//printf("fd: %d\n", client_fd);

	int alive =1;
		while(alive)
			{
			  
				  int res;
				  if (terminator_flag == 1 )
					{
						break;
					}
				  http_t * http_request = (http_t *)malloc( sizeof(http_t) );
				  fd_set master;
				  FD_ZERO(&master);
				  FD_SET(client_fd, &master);
				  struct timeval t2sec;
				  t2sec.tv_sec = 2;
				  t2sec.tv_usec=0;
				  //int ret = select(*client_fd + 1, &master, NULL, NULL, &t2sec);
				  int ret = select(FD_SETSIZE, &master, NULL, NULL, &t2sec);
				  if (ret==-1 || ret==0)
				  {
				  	free(http_request);
				    continue;//break;// nothing to receive
				  }
				  res = http_read( http_request, client_fd );
				  if(res <= 0)
				  {
				    free(http_request);
				    continue;//break;// nothing to receive
				  }
				  
				  //set response
				  response_t response_info;
				  response_info.status_code = 200;

				  // check request file
				  char * filename_return=NULL;
				  filename_return = process_http_header_request( http_get_status(http_request) );


				  //set Connection
				  char * connection = (char *)http_get_header(http_request, "Connection"); 
				  if (!connection )
				  {
				  	connection = "close";
				  }
				  if (strcasecmp( connection, "Keep-Alive" ) != 0)
				  {
				  	//strcpy(connection, "close");
				  	alive=0;

				  }

				  //check file
				  if (filename_return == NULL)
				  {
				  	response_info.status_code = 501;
				  	response_info.content_length = strlen(HTTP_501_CONTENT);
				  	strcpy( response_info.content_type, "text/html" );
				  	
				  	//send response_header
				  	char * response_header = write_header( response_info, connection );
					send(client_fd, response_header, strlen(response_header) + 1, 0);
					free(response_header);
				  }
				  else
				  {
					  	if (strcmp(filename_return, "/") == 0)
					  	{
					  		free(filename_return);
					  		filename_return = (char *)malloc( sizeof(char)*(strlen("/index.html")+1) );
					  		filename_return[0] = '\0';
					  		strcpy(filename_return, "/index.html");
					  	}

					  	//load in web directory
					  	char * webdir = malloc(sizeof(char)*(strlen(filename_return)+strlen("web")+1)  );
					  	webdir[0] = '\0';
					  	strcpy(webdir, "web" );
					  	strcat(webdir, filename_return);
					  	//sprintf(webdir, "web%s", filename_return);
					  	

					  	//open the file
					  	FILE * file;
			  			long filesize;
			  			file = fopen( webdir, "r" );
					  	if ( file == NULL )//access(webdir, 'R_OK') == 0
					  	{
					  		response_info.status_code = 404;
					  		response_info.content_length = strlen(HTTP_404_CONTENT);	
					  		strcpy( response_info.content_type, "text/html" );
					  		//send response_header
				  			char * response_header = write_header( response_info, connection );
				  			//printf("ResHeader:----- \n%s\n", response_header);

							send(client_fd, response_header, strlen(response_header), 0);
							free(response_header);
					  	}
					  	else
					  	{
					  			//set content type
					  			response_info.content_type[0] = '\0';
						  		strcpy(response_info.content_type, "text/plain");
						  		char * pch;
						  		pch=strchr(filename_return,'.');
						  		if (strcmp( pch, ".html" ) == 0)
						  		{
						  			strcpy(response_info.content_type, "text/html");
						  		}
						  		else if (strcmp( pch, ".css" ) == 0)
						  		{
						  			strcpy(response_info.content_type, "text/css");
						  		}
						  		else if (strcmp( pch, ".jpg" ) == 0)
						  		{
						  			strcpy(response_info.content_type, "image/jpeg");
						  		}
						  		else if (strcmp( pch, ".png" ) == 0)
						  		{
						  			strcpy(response_info.content_type, "image/png");
						  		}
						  		//printf("content_type is:%s!\n", response_info.content_type);


						  		//read file
					  			if ( strcmp(response_info.content_type, "image/png")==0  || strcmp(response_info.content_type, "image/jpeg")==0 )
					  			{
					  				file = freopen(webdir, "rb", file);
					  			}
					  			fseek (file , 0 , SEEK_END);
					  			filesize = ftell(file);
					  			rewind (file);
					  			char * response_body = (char*) malloc (sizeof(char)*filesize);
  								if (response_body == NULL) {fputs ("Memory error",stderr); exit (2);}
  								size_t result = fread(response_body,1,filesize,file);
  								if (result != (size_t)filesize) {fputs ("Reading error",stderr); exit (3);}
  								response_info.content_length = (int)filesize;
						  		fclose(file);
						  		

						  		//send response_header
						  		
						  		char * response_header = write_header( response_info, connection );
						  		if (!response_header) {perror("writing"); exit(1);}
						  		//printf("response_header is:------\n%s-----\n", response_header);
						  		send(client_fd, response_header, strlen(response_header), 0);
						  		send(client_fd, response_body, (int)filesize, 0);
						  		free(response_header);
						  		free(response_body);

					  	}

					  	free(webdir);
				  }

				  free(filename_return);
				  http_free( http_request );
				  free(http_request);
			}
			printf("go out of while\n");

		close(client_fd);
		//check the client is closed or not
		//printf("ending this thread of fd:%d\n", client_fd);
		// fd_set checkclose;
		// 		  FD_ZERO(&checkclose);
		// 		  FD_SET(client_fd, &checkclose);
		// 		  struct timeval timeout;
		// 		  timeout.tv_sec = 1;
		// 		  timeout.tv_usec=0;
		// 		  //int ret = select(client_fd + 1, &master, NULL, NULL, &t2sec);
		// 		  int ret = select(FD_SETSIZE, &checkclose, NULL, NULL, &timeout);
		// 		  if (ret==-1 || ret==0)// the client_fd is closed 
		// 				free(client_fd);
		
		//free(client_fd);
		
	return NULL;
	//pthread_exit( client_fd_pass  );
}

char* write_header( response_t response_info, char* connection )
{
	char * response_string;
	switch(response_info.status_code)
	{
		case 404:
			response_string = HTTP_404_STRING;
			break;
		case 501:
			response_string = HTTP_501_STRING;
			break;
		case 200:
			response_string = HTTP_200_STRING;
			break;
	}


	char * response_header = (char *)malloc(sizeof(char)*MAX_LINE*32 );
	response_header[0] = '\0';

	int scan_size = 0;

	scan_size += sprintf(response_header, "HTTP/1.1 %d %s\r\n", response_info.status_code, response_string);
	scan_size += sprintf(response_header + (ptrdiff_t)scan_size, "Content-Type: %s\r\n", response_info.content_type);
	scan_size += sprintf(response_header + (ptrdiff_t)scan_size, "Content-Length: %d\r\n", response_info.content_length);
	scan_size += sprintf(response_header + (ptrdiff_t)scan_size, "Connection: %s\r\n", connection);
	scan_size += sprintf(response_header + (ptrdiff_t)scan_size, "\r\n");
	

	if (response_info.status_code == 404)
	{
		scan_size += sprintf(response_header + (ptrdiff_t)scan_size, "%s", HTTP_404_CONTENT);
	}
	if (response_info.status_code == 501)
	{
		scan_size += sprintf(response_header + (ptrdiff_t)scan_size, "%s", HTTP_501_CONTENT);
	}
	return response_header;

}

void signal_handler()
{
	terminator_flag =1;
	unsigned int size_queue = queue_size( &queue_clientfd );
	//printf(" the sizeof the queue is :%i\n", size_queue);
	int i=0;
	for (i = 0; i < size_queue; ++i)
	{
		int * current_client_fd = queue_at( &queue_clientfd, i );
		int ret_close = close(*current_client_fd);
		//printf("%d\n", ret_close);
		free(current_client_fd);
	}
	// for (i = 0; i < size_queue; ++i)
	// {
	// 	int * current_client_fd = queue_at( &queue_clientfd, i );
	// 	free(current_client_fd);
	// }
	//printf("will exit signal signal_handler\n");
	close(sockfd);
	//exit(0);

}