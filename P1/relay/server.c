#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pong.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>


struct sockaddr_in server_addr;
struct sockaddr_in client_addr;
struct player list;


void error(char *msg)
{
    perror(msg);
    exit(0);
}

void add_player(player **root, int new_port, char *new_address)
{
    player *new = malloc(sizeof(player));
    if (new == NULL)
    {
        perror("insert");
        exit(0);
    }
    
    new->next = NULL;
    new->active = 0;
    new->port = new_port;
    new->address = new_address;

    if(*root == NULL)
    {
        *root = new;
        return;
    }

    player *curr = *root;
    while (curr->next != NULL)
    {
        curr = curr->next;
    }
    curr->next = new;

}

void remove_player(player **root, int port, char *address)
{

    if ((*root)->port == port && (*root)->address == address)
    {
        player *to_remove = *root;
        *root = (*root)->next;
        free(to_remove);

        return;
    }
    
    for (player *curr = *root; curr->next != NULL; curr = curr->next)
    {
        if (curr->next->port == port && curr->next->address == address)
        {
            player *to_remove = curr->next;
            curr->next = curr->next->next;
            free(to_remove);

            return;
        }
    }
}


int main (int argc, char* argv[])
{
    message message; 
    player *root = NULL;
    player *current_player = NULL;

    /* create socket */
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd == -1)
        error("creating socket");


    /* configure socket */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SOCK_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;


    /* bind */
	int op = bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if(op == -1) 
		error("bind");


    /* client destination */
    socklen_t client_addr_size = sizeof(struct sockaddr_in);
    client_addr.sin_family = AF_INET;


    /* initial constraints */
    int nr_players = 0;
    char buf[12];


    while (1)
    {

        printf("\nWaiting message...");
        recvfrom(socket_fd, &message, sizeof(message), 0, (struct sockaddr *)&client_addr, &client_addr_size);
        printf("\nReceived message...");
        
        switch (message.type)
        {
            case conn:
        
                // add new player
                inet_ntop(AF_INET, &client_addr.sin_addr, buf, 12);                 
                if (nr_players < MAX_PLAYERS)
                {
                    add_player(&root, ntohs(client_addr.sin_port), buf);
                    nr_players++;
                }

                // if there's only one player, he starts playing; otherwise waits
                if (nr_players == 1)
                {
                    current_player = root;
                    message.type = send_ball;

                    op = sendto(socket_fd, &message, sizeof(message), 0, (struct sockaddr*) &client_addr, sizeof(client_addr));
                    if (op == -1)
                        error("client unavailable");
                }

                break;

            case disconnect:

                // remove player
                inet_ntop(AF_INET, &client_addr.sin_addr, buf, 12); 
                remove_player(&root, ntohs(client_addr.sin_port), buf);
                nr_players--;

                if (nr_players == 0)
                    break;

                // sends ball to other player
                current_player = root;
                message.type = send_ball;             

                inet_ntop(AF_INET, current_player->address, &client_addr.sin_addr, sizeof(client_addr.sin_addr));
                client_addr.sin_port = htons(current_player->port);                

                op = sendto(socket_fd, &message, sizeof(message), 0, (struct sockaddr*) &client_addr, sizeof(client_addr));
                if (op == -1)
                    error ("client unavailable");                

                break;

            case release_ball:

                // someone released the ball, we must send it to a new client
                message.type = send_ball;

                if (current_player->next != NULL)
                    current_player = current_player->next;
                else
                    current_player = root;

                inet_ntop(AF_INET, current_player->address, &client_addr.sin_addr, sizeof(client_addr.sin_addr));
                client_addr.sin_port = htons(current_player->port);                

                op == sendto(socket_fd, &message, sizeof(message), 0, (struct sockaddr*) &client_addr, sizeof(client_addr));
                if (op == -1)
                    error("client unavailable");

                break;

            case move_ball:

                // update for every client
                for (player *temp_curr = root; temp_curr != NULL; temp_curr=temp_curr->next)
                {
                    if (temp_curr->address != current_player->address || temp_curr->port != current_player->port)
                    {
                        inet_ntop(AF_INET, temp_curr->address, &client_addr.sin_addr, sizeof(client_addr.sin_addr));
                        client_addr.sin_port = htons(temp_curr->port);

                        op == sendto(socket_fd, &message, sizeof(message), 0, (struct sockaddr*) &client_addr, sizeof(client_addr));
                        if (op == -1)
                            error("client unavailable");

                    }
                }

            default:
                break;
        }

        printf("\n");
    }

    printf("\n");
    return 0;

}

