/* 
 * Author: Jasan Singh @: Jasan.Singh@colorado.edu
 * Code simulates the http server takes argument port on which the socket opens for listening 
 * code uses tcpechosrv.c - A concurrent TCP echo server using threads as base to build the server 
 * 
 * 
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include <fcntl.h>

#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */

#define UNUSED(arg)  ((void)arg)

struct server_t {
	int sock;
	struct sockaddr_in addr;

};

struct client_t {
	int sock;
	struct sockaddr_in addr;
	socklen_t len;
};

struct request_t {
	char method[8];
	char protocol[12];
	char url[128];
	int content_length;
};

struct response_t {
	char head[256];
};


struct header_t {
	char key[32];
	char value[128];
};


int open_listenfd(int port);
void echo(int connfd);
//void *thread(void *vargp);
void threadServer(int fd);
static int append(char * s, size_t len, char c);
static int str_append(char * s, size_t len, char c);
static int httpParser( int client_sock, struct request_t *r);
static int method_append(struct request_t * r, char c);
static int url_append(struct request_t * r, char c);
static int protocol_append(struct request_t * r, char c);
static int append(char * s, size_t len, char c);
static void request_parsed(int rc, struct request_t * r);
static int error_request(int sock, const struct request_t * req);
static int process_request(int sock, const struct request_t * req);
static void clear(char * s, size_t len);
static void clear_header_property(struct header_t * prop);
static void findExtension (const char * filename, char * extension);

int main(int argc, char **argv) 
{
    int listenfd, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid; 
    struct server_t server;
    int optval = 1; //for reuse
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }
    port = atoi(argv[1]);

     
     memset(&server, 0, sizeof(server));
	server.sock = -1;
    //creates/opens a socket
    //listenfd = open_listenfd(port);
    
    /* Create a socket descriptor */
    if ((server.sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;
    printf("server socket decriptor received\n");
    memset(&server.addr, 0, sizeof(server.addr));
    
    
    printf("setsockopt success:\n");
    /* listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    //bzero((char *) &server.addr, sizeof(server.addr));
    server.addr.sin_family = AF_INET; 
    server.addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    server.addr.sin_port = htons((unsigned short)port); 
    if (bind(server.sock, (const struct sockaddr*)&server.addr, sizeof(server.addr)) < 0){
        perror("Bind");
        return -1; }
    printf("Bind success\n");
    
    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(server.sock, SOL_SOCKET, SO_REUSEADDR, 
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;
        
    /* Make it a listening socket ready to accept connection requests */
    if (listen(server.sock, 2) < 0)
        return -1;
    
    
    //function pointers to functions to give a response to request parsed
    //server.error_func_req = error_request;
    //server.process_func_req = process_request;
    printf("Entering while(1) : \n");
    while (1) {
	connfdp = malloc(sizeof(int));
	*connfdp = accept(server.sock, (struct sockaddr*)&clientaddr, &clientlen);
	printf("spawning new thread:\n");
	if( *connfdp < 0)
		perror("accept failed");
	else
	{
		if(fork() == 0)
		{
			//call the functional thread
			threadServer(*connfdp);
			exit(0);
		}
	}
	
	//pthread_create(&tid, NULL, thread, connfdp);
    }
    printf("exiting while loop");
}	


static int append(char * s, size_t len, char c)
{
	return str_append(s, len, c);
}

static int str_append(char * s, size_t len, char c)
{
	size_t l = strlen(s);
	//printf(" protocol_length = %ld %ld \n", l, len); 
	if (l < len) {
		s[l]= c;
		return 0;
	}
	return -1;
}

// method appends the current character into method of the request
static int method_append(struct request_t * r, char c)
{
	//printf("entering method append\n");
	return str_append(r->method, sizeof(r->method)-1, c);
}

static int url_append(struct request_t * r, char c)
{
	//printf("entering url append\n");
	return str_append(r->url, sizeof(r->url)-1, c);
}

static int protocol_append(struct request_t * r, char c)
{
	//printf("entering protocol append\n");
	return str_append(r->protocol, sizeof(r->protocol)-1, c);
}

static void clear(char * s, size_t len)
{
	memset(s, 0, len);
}

static void clear_header_property(struct header_t * prop)
{
	clear(prop->key, sizeof(prop->key));
	clear(prop->value, sizeof(prop->value));
}

/* Parses the received daata and checks for valid requests */
static int httpParser( int client_sock, struct request_t *r)
{

  	int state = 0;
  	int read_flag = 1; //keeps track to proceed reading or not 
  	char curr_char = 0; 
  	char receive_buf[16] ;
  	int buffer_index;
  	
  	//temp header property storage
  	struct header_t temp_prop;
  	int content_length = -1;
  	
  	
  	while (client_sock >= 0)
  	{
  		
  		//reads the data upon indication
  		if (read_flag)
  		{
  			//reads data if the previous read data was processed (i.e buffer_index >= 16)
  			if( buffer_index >= sizeof(receive_buf) )
  			{
  				int ret;
  				memset(receive_buf, 0, sizeof(receive_buf));
  				ret = read( client_sock, receive_buf, sizeof(receive_buf) );
  				if( ret < 0 ) 
  					return -99; //read error
  				if( ret == 0) 
  					return 0;
  					
  		               // upon successful read of buffer, index is reset to 0
  		              // printf("read data: %s \n", receive_buf);
  		               buffer_index = 0;
  		         }
  		         curr_char = receive_buf[buffer_index];
  		         ++buffer_index; 
  			  
  			 //nextly read indication is turnedd off until all the characters read are processed
  			 read_flag =0;
  		}
  		
  		//state machine 
  		switch (state) 
  		{
  			case 0://removing leading spaces
  				if(isspace(curr_char)) {
  					read_flag = 1;
  				} else {
  					state = 1;
  				} 
  			break;
  			case 1: 
  				if(isspace(curr_char)) {
  					state = 2;   //i.e method has been recorded moving to next state/ item 
  				} else { 
  				  if(method_append(r, curr_char))  //
	        			return -state;     //method allotted space full returns out with negative value
				read_flag = 1; //indicating next read
				}
				break;
			case 2: //ignoring spaces
				if( isspace(curr_char) ) {
					read_flag = 1;
				} else {
					state = 3;
				}
				break;
			case 3: //recording url
				if(isspace(curr_char)) {
				 	state = 4; //recorded url movinfg to next sstate
				 } else {
				 	if( url_append(r, curr_char))
				 		return -state; //error
				 	read_flag = 1; //indicating check for next character
				 }
				 break;
			case 4: // ignoring spaces
				if( isspace(curr_char) ) {
					read_flag = 1;
				} else {
					state = 5;
				}
				break;
			case 5: //recording protocol
				if( isspace(curr_char) ) {
					state = 6;
				 } else { 
				 	if( protocol_append(r,curr_char))
				 		return -state;
				 	read_flag = 1;
				 }
				 break;
		        case 6: // ignoring spaces
				if( isspace(curr_char) ) {
					read_flag = 1;
				} else {
					state = 7;
				}
				break;
			case 7: //recording header line key
				if( curr_char == ':' ) {
					state = 8;
				 } else { 
				 	if( append( temp_prop.key, sizeof(temp_prop.key)-1, curr_char))
				 	return -state; //error
				 	read_flag = 1;
				 }
				 break;
			case 8:
				if( isspace(curr_char) ) {
					read_flag = 1;
				} else {
					state = 9;
				}
				break;
			case 9://recording header value
				if( curr_char == '\r') {
					if(strcmp("Content Length", temp_prop.key) == 0)
					content_length = strtol(temp_prop.value, 0, 0);
					//clear temp_prop 
					clear_header_property(&temp_prop);
					state = 10;
					read_flag = 1;
				} else {
					if( append(temp_prop.value, sizeof(temp_prop.value)-1, curr_char))
					return -state;
					read_flag = 1;
				}
				break;
			case 10:
			if( curr_char == '\n') {
				read_flag = 1;
			} else if( curr_char == '\r') {
				state = 11; read_flag =1;
			} else {
				state = 7; //back to further headers
			}
			break;
			case 11: // finding the end of header
				if( curr_char == '\n') {
					//end of header
					return 0;
				} else {
					state = 7;
				}
				break;	
  		}
  		
  		
  		
  	}//while(1)
        return -99;  	
} 


static void request_parsed(int rc, struct request_t * r)
{
	size_t i;
	if (rc) {
		printf("\nERROR: invalid request: %d", rc);
	} else {
		printf("MET:[%s]\n", r->method);
		printf("PRT:[%s]\n", r->protocol);
		printf("URL:[%s]\n", r->url);
	//	for (i = 0; i < r->nquery; ++i) printf("QRY:[%s]\n", r->query[i].val);
	}
	printf("\n");
}

//provides response to a error request
static int error_request(int sock, const struct request_t * req)
{
	static const char * RESPONSE =
		"HTTP/1.1 400 Bad Request\r\n"
		"Content-Type: text/html\r\n"
		"Pragma: no-cache\r\n"
		"Cache-Control: no-cache\r\n"
		"Connection: close\r\n"
		"\r\n"
		"<html><body>Bad Request</body></html>\r\n";

	int length = 0;

	UNUSED(req);

	length = strlen(RESPONSE);
	return write(sock, RESPONSE, length) == length ? 0 : -1;
}

static void response_init(struct response_t * res)
{
	memset(res->head, 0, sizeof(res->head));
}

static int response_append_content_type(struct response_t * res, const char * mime)
{
	static const char * TEXT = "Content-Type: ";

	if (strlen(res->head) > (sizeof(res->head) - strlen(TEXT) - strlen(mime) - 2))
		return -1;
	strcat(res->head, TEXT);
	strcat(res->head, mime);
	strcat(res->head, "\r\n");
	return 0;
}

static int response_append(struct response_t * res, const char * text, size_t len)
{
	const size_t n = sizeof(res->head) - strlen(res->head);
	if (len > n)
		return -1;
	strncat(res->head, text, n);
	return 0;
}

static int response_append_no_cache(struct response_t * res)
{
	static const char * TEXT =
		"Pragma: no-cache\r\n"
		"Cache-Control: no-cache\r\n";
	return response_append(res, TEXT, strlen(TEXT));
}

static int response_append_connection_close(struct response_t * res)
{
	static const char * TEXT = "Connection: close\r\n";
	return response_append(res, TEXT, strlen(TEXT));
}

static int response_append_header_start(struct response_t * res)
{
	static const char * TEXT = "HTTP/1.1 200 OK\r\n";
	return response_append(res, TEXT, strlen(TEXT));
}

static int response_append_header_end(struct response_t * res)
{
	static const char * TEXT = "\r\n";
	return response_append(res, TEXT, strlen(TEXT));
}

static int send_header_mime(int sock, const char * mime)
{
	int len;
	struct response_t res;

	response_init(&res);
	response_append_header_start(&res);
	response_append_content_type(&res, mime);
	response_append_no_cache(&res);
	response_append_connection_close(&res);
	response_append_header_end(&res);

	len = (int)strlen(res.head);
	return write(sock, res.head, len) == len ? 0 : -1;
}

static void findExtension (const char * filename, char * extension)
{
	int i=0; int j=0;
	while(filename[i++] != '.');
	
	while(filename[i] != '\0')
	{
		extension[j++] = filename[i++];
	}
	extension[j] = '\0';

}

static int request_send_file(int sock, const struct request_t * req, const char * filename)
{
	int fd;
	int rc;
	char buf[256];
	char extension[20];
	char content_type[20];
	UNUSED(req);

	//get file extension from filename 
	findExtension( filename, extension);
	
	//then update the heade content-type accordingly
	if( strcmp( extension, "html") == 0)
	{
		//content_type[] = "text/html";
		strcpy( content_type, "text/html");
	} else if( strcmp( extension, "txt") == 0)
	{
		//*content_type = "text/plain";
		strcpy( content_type, "text/plain");
	} else if( strcmp( extension, "png") == 0)
	{
		//*content_type = "image/png";
		strcpy( content_type, "image/png");
	} else if( strcmp( extension, "gif") == 0)
	{
		//*content_type = "image/gif";
		strcpy( content_type, "image/gif");
	} else if( strcmp( extension, "jpg") == 0)
	{
		//*content_type = "image/jpg";
		strcpy( content_type, "image/jpg");
	} else if( strcmp( extension, "css") == 0)
	{
		//*content_type = "text/css";
		strcpy( content_type, "text/css");
	}
        printf("openig file name %s with content type %s \n", filename, content_type);
	fd = open(filename, O_RDONLY);
	if (fd < 0)
	{
		perror( "fopen");
		return -1;
	}
	if (send_header_mime(sock, content_type) >= 0) {
		for (;;) {
			rc = read(fd, buf, sizeof(buf));
			if (rc <= 0) break;
			rc = write(sock, buf, rc);
			if (rc < 0) break;
		}
	}
	close(fd);
	return 0;
}

static int process_request(int sock, const struct request_t * req)
{
	static const char * RESPONSE =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/html\r\n"
		"Pragma: no-cache\r\n"
		"Cache-Control: no-cache\r\n"
		"Connection: close\r\n"
		"\r\n"
		"<html><body>Welcome (default response)</body></html>\r\n";

	int length = 0;

	UNUSED(req);
	char *request_file;
	request_file = req->url + 1;
	printf("requested file %s \n", request_file);
	if( *request_file == '\0')
	{
		return request_send_file(sock, req, "index.html");
	
	}
	else
	{
		return request_send_file(sock, req, request_file);
	} /*
	if (strcmp(req->url, "/") == 0) {
		return request_send_file(sock, req, "index.html");
	}
	else if (strcmp(req->url, "/images/wine3.jpg") == 0) {
		return request_send_file(sock, req, "images/wine3.jpg");
	} else {
		length = strlen(RESPONSE);
		return (write(sock, RESPONSE, length) == length) ? 0 : -1;
	} */
}

/* thread routine */
void threadServer( int fd)
//void * thread(void * vargp) 
{  
  //  int connfd = *((int *)vargp); //getting the client soc file descripptor
    int connfd = fd;
    int ret;
    struct request_t r = {0};
    //pthread_detach(pthread_self()); 
    //free(vargp);
    
    //clearing request before proceeding 
    //r.method = {0}; r.protocol = {0}; r.url = {0};
    //r.content_length=0;
    //echo(connfd);
    ret = httpParser(connfd, &r);
    request_parsed( ret, &r);  // prints the parsed request
    if (ret == 0)
    {
    	//if (server->process_func_req)
    	//	server->process_func_req (connfd, &r);
    	process_request(connfd, &r);
    } else {
    	 //if (server->error_func_req)
    		//server->error_func_req (connfd, &r);
    		error_request(connfd, &r);
    }
    
    shutdown(connfd, SHUT_WR);
    close(connfd);
    //return NULL;
}

/*
 * echo - read and echo text lines until client closes connection
 */
void echo(int connfd) 
{
    size_t n; 
    char buf[MAXLINE]; 
    char httpmsg[]="HTTP/1.1 200 Document Follows\r\nContent-Type:text/html\r\nContent-Length:32\r\n\r\n<html><h1>Hello CSCI4273 Course!</h1>"; 

    n = read(connfd, buf, MAXLINE);
    printf("server received the following request:\n%s\n",buf);
    strcpy(buf,httpmsg);
    
    //use this request and try to command parse ad check the request
    //http message parser 
    
    
    printf("server returning a http message with the following content.\n%s\n",buf);
    write(connfd, buf,strlen(httpmsg));
    
}

/* 
 * open_listenfd - open and return a listening socket on port
 * Returns -1 in case of failure 
 */
int open_listenfd(int port) 
{
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;
  
    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    /* listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)port); 
    if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
} /* end open_listenfd */

