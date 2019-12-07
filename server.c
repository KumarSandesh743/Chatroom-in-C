#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>


#define BUF_SIZE 4096


pthread_mutex_t list_loc;


struct Node
{
	char * name;
	int socket;
	struct Node * prev;
};


typedef struct Node Node;


static volatile Node * head = 0;


void addClient(int socket, char * name)
{
	pthread_mutex_lock(&list_loc);

	Node * n = malloc(sizeof(Node));
	int len = strlen(name);
	char * new_name = malloc(len + 1);
	strcpy(new_name, name);
	new_name[len] = 0;
	n->name = new_name;
	n->socket = socket;	
	n->prev = head;
	head = n;

	pthread_mutex_unlock(&list_loc);
}


int checkClient(char * name)
{
	pthread_mutex_lock(&list_loc);

	Node t = *head;
	Node * temp = &t;
	while ( temp->prev != 0)
	{
		if (strcmp ( temp->name, name) == 0)
		{
			pthread_mutex_unlock(&list_loc);
			return 0;
		}
		temp = temp -> prev;
	}

	pthread_mutex_unlock(&list_loc);
	return 1;
}


int messageClient(char * response, char * name, int client_sock)
{
	int n_size = strlen(name) + 2;

	response = response + 1;
	int i = 0;
	while ( response[i] != ' ' )
	{
		i++;
	}

	char dest_name[i + 1];
	response[i] = 0;
	strcpy(dest_name, response);
	dest_name[i] = 0;

	char * tp = &response[i + 1];
	i = strlen(tp);

	char actual_response[i + n_size + 1];

	strcpy(actual_response, name);
	actual_response[n_size - 2] = ':';
	actual_response[n_size - 1] = ' ';
	actual_response[n_size] = 0;

	strcpy(&actual_response[n_size], tp);
	actual_response[i + n_size] = 0;

	response = actual_response;

	pthread_mutex_lock(&list_loc);

	Node * temp = head;
	while ( temp->prev != 0)
	{
		if (strcmp ( temp->name, dest_name) == 0)
		{
			if ( write(temp->socket, response, strlen(response)) < 0 )
			{
				perror("write ");
				pthread_mutex_unlock(&list_loc);
				removeClient(name);
				close(client_sock);
				pthread_exit(1);
			}
			pthread_mutex_unlock(&list_loc);
			return 0;
		}
		temp = temp -> prev;
	}

	pthread_mutex_unlock(&list_loc);
	return 1;
}


void populateClient(char * response)
{
	pthread_mutex_lock(&list_loc);

	Node * temp = head;
	int i = 0;
	int name_size = 0;
	int j = 0;
	while ( temp->prev != 0 && i < BUF_SIZE - 1)
	{
		name_size = strlen(temp->name);
		j = 0;
		while ( j < name_size && i < BUF_SIZE - 1)
		{
			response[i] = temp->name[j];
			i++;
			j++;
		}
		response[i] = '\n';
		i++;
		temp = temp->prev;
	}
	response[i - 1] = 0;

	pthread_mutex_unlock(&list_loc);
}


int removeClient(char * name)
{
	pthread_mutex_lock(&list_loc);

	if ( head-> prev == 0)
	{
		pthread_mutex_unlock(&list_loc);
		return 1;
	}
	if ( strcmp(name, head->name) == 0)
	{
		Node * temp = head;
		head = head -> prev;
		free(temp->name);
		free(temp);
		pthread_mutex_unlock(&list_loc);
		return 0;
	}

	Node * n = head;
	while ( n-> prev -> prev != 0)
	{
		if (strcmp(name, n->prev->name) == 0)
		{
			Node * tmp = n->prev;
			n->prev = n->prev->prev;
			free(tmp->name);
			free(tmp);
			pthread_mutex_unlock(&list_loc);
			return 0;
		}
		n = n-> prev;
	}

	pthread_mutex_unlock(&list_loc);

	return 1;
}


void * readClient(void * client_s)
{
	int client_sock = client_s;
	char response[BUF_SIZE];           //what to send to the client
	int n;                             //length measure

	response[0] = 0;
	n = read(client_sock, response, BUF_SIZE - 1);

	if (n < 0)
	{
		perror("read ");
		close(client_sock);
		pthread_exit(1);
	}

	char name[strlen(response) + 1];
	strcpy(name, response);
	name[strlen(response)] = 0;

	if ( checkClient(response) == 0)
	{
		strcpy(response, "clash");
		if ( write(client_sock, response, strlen(response)) < 0 )
		{
			perror("write ");
		}
		close(client_sock);
		pthread_exit(1);
	}
	else
	{
		strcpy(response, "accept");
	}

	if ( write(client_sock, response, strlen(response)) < 0 )
	{
		perror("write ");
		close(client_sock);
		pthread_exit(1);
	}

	addClient(client_sock, name);

	int send;

	while(1){

		send = 0;

		if((n = read(client_sock,response, BUF_SIZE-1)) < 0){
			perror("read");
			removeClient(name);
			close(client_sock);
			pthread_exit(1);
		}

		if ( response[0] == '0')
		{
			removeClient(name);
			close(client_sock);
			pthread_exit(1);
		}
		else if ( response[0] == '1')
		{
			populateClient(response);
			send = 1;
		}
		else if ( response[0] == '2')
		{
			response[n] = 0;
			int r = messageClient(response, name, client_sock);
			if ( r == 1 )
			{
				char back_rsp[] = "Client name not found ";
				strcpy(response, back_rsp);
				response[strlen(back_rsp)] = 0;
				send = 1;
			}
		}

		//send response
		if ( send  == 1)
		{
			if(write(client_sock, response, strlen(response)) < 0){
				removeClient(name);
				close(client_sock);
				pthread_exit(1);
			}			
		}
	}
}





int main(int argc, char * argv[])
{
	head = malloc(sizeof(Node));
	head->prev = 0;
	head->socket = -1;
	head->name = 0;

	if (argc < 2)
	{
		printf("No port specified\n");
		exit(0);
	}

  	char hostname[] = "127.0.0.1";   //localhost ip address to bind to
  	short port = atoi(argv[1]);               //the port we are to bind to

	struct sockaddr_in saddr_in;  //socket interent address of server
	struct sockaddr_in client_saddr_in;  //socket interent address of client

	socklen_t saddr_len = sizeof(struct sockaddr_in); //length of address

	int server_sock, client_sock;         //socket file descriptor

	//set up the address information
	saddr_in.sin_family = AF_INET;
	inet_aton(hostname, &saddr_in.sin_addr);
	saddr_in.sin_port = htons(port);

	//open a socket
	if( (server_sock = socket(AF_INET, SOCK_STREAM, 0))  < 0){
	    perror("socket");
	    exit(1);
	}

	//bind the socket
	if( bind(server_sock, (struct sockaddr *) &saddr_in, saddr_len) < 0 ){
		perror("bind");
		exit(1);
	}

	//ready to listen, queue up to 5 pending connectinos
	if( listen(server_sock, 5)  < 0 ){
		perror("listen");
		exit(1);
	}

	saddr_len = sizeof(struct sockaddr_in); //length of address

	printf("Listening On: %s:%d\n", inet_ntoa(saddr_in.sin_addr), ntohs(saddr_in.sin_port));

	//accept incoming connections

	pthread_t p;

	while (1 == 1)
	{
		if((client_sock = accept(server_sock, (struct sockaddr *) &client_saddr_in, &saddr_len)) < 0){
			perror("accept");
			exit(1);
		}
		//read from client
		if ( pthread_create(&p, NULL, readClient, (void *)client_sock) < 0 )
		{
			perror("thread ");
		}
	}

	printf("Closing socket\n\n");

	//close the socket
	close(server_sock);

	return 0; //success
}

