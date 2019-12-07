#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>


#define BUFFER_SZIE 4096


static volatile int sock = -1;


void * reading(void * args)
{
  int n;
  char response[BUFFER_SZIE + 1];

  n = read(sock, response, BUFFER_SZIE);
  if ( n < 0 )
  {
    perror("read ");
    exit(1);
  }
  else if ( strcmp(response, "clash") == 0)
  {
    printf("Name clashes with an existing client\n");
    exit(1);
  }
  else if ( strcmp(response, "accept") != 0 )
  {
    printf("Some other error occured in connecting to server\n");
    exit(1);
  }
  else
  {
    printf("Connected succesfully. \n Available commands are /quit /list /msg clientname message\n");
  }
  

  while( (n = read(sock, response, BUFFER_SZIE)) > 0){
    response[n] = '\n';
    //write response to stdout
    if(write(1, response, n + 1) < 0){
      perror("write");
      exit(1);
    }
  }

  //read the response until EOF

  if (n<0){
    perror("read");
    exit(1);
  }

}


int checkValidMessage(char * str)
{
  int len = strlen(str);
  if (len < 6)
  {
    return 0;
  }
  char buff[] = "/msg ";
  char buff2[len + 1];
  strcpy(buff2, str);
  buff2[5] = 0;
  if ( (strcmp(buff2, buff) != 0) )
  {
    return 0;
  }
  strcpy(buff2, str);
  buff2[len] = 0;

  char * token;
  int i = 0;

  token = strtok(buff2, " ");
  while ( token != 0)
  {
    i++;
    token = strtok(NULL, " ");
  }

  if(i < 3)
  {
    return 0;
  }

  return 1;
}


int main(int argc, char * argv[]){

  if ( argc < 4 )
  {
    printf("Insufficient number of parameters\n");
    exit(0);
  }

  char * hostname = argv[1];    //the hostname we are looking up
  short port = atoi(argv[2]);                 //the port we are connecting on

  struct addrinfo *result;       //to store results
  struct addrinfo hints;         //to indicate information we want

  struct sockaddr_in *saddr_in;  //socket interent address

  int s,n;                       //for error checking

  pthread_t p;

  //setup our hints
  memset(&hints,0,sizeof(struct addrinfo));  //zero out hints
  hints.ai_family = AF_INET; //we only want IPv4 addresses

  //Convert the hostname to an address
  if( (s = getaddrinfo(hostname, NULL, &hints, &result)) != 0){
    fprintf(stderr, "getaddrinfo: %s\n",gai_strerror(s));
    exit(1);
  }

  //convert generic socket address to inet socket address
  saddr_in = (struct sockaddr_in *) result->ai_addr;

  //set the port in network byte order
  saddr_in->sin_port = htons(port);

  //open a socket
  if( (sock = socket(AF_INET, SOCK_STREAM, 0))  < 0){
    perror("socket");
    exit(1);
  }

  //connect to the server
  if(connect(sock, (struct sockaddr *) saddr_in, sizeof(*saddr_in)) < 0){
    perror("connect");
    exit(1);
  }

  char * request;
  size_t s_message;

  int quit = 0;
  int send = 0;

  if (pthread_create(&p, NULL, reading, NULL) < 0 )
  {
    perror("thread ");
    exit(1);
  }

  if ( write(sock, argv[3], strlen(argv[3])) < 0 )
  {
    perror("write ");
  }


  while(quit == 0)
  {
    request = 0;
    send = 0;
    s_message = getline(&request, &s_message, stdin);
    request[s_message - 1] = 0;
    if (strcmp(request, "/quit") == 0)
    {
      quit = 1;
      send = 1;
      request[0] = '0';
      request[1] = 0;
    }
    else if ( strcmp (request, "/list") == 0)
    {
      send = 1;
      request[0] = '1';
      request[1] = 0;
    }
    else if ( checkValidMessage(request) == 1)
    {
      send = 1;
      request[0] = '2';
      int i = 5;
      while( request [i] != 0)
      {
        request[i - 4] = request[i];
        i++;
      }
      request[i - 4] = 0;
    }
    if ( send != 1)
    {
      printf("Invalid command \n");
    }
    else
    {
    //send the request
      if(write(sock,request,strlen(request)) < 0){
        perror("send");
      }      
    }
    free(request);
  }

  //close the socket
  close(sock);

  return 0; //success
}

