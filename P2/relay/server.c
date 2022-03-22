
#include <ncurses.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "pong.h"

WINDOW *big_win; 
WINDOW *small_win;

struct sockaddr_in server_address;
struct sockaddr_in client_address;

pthread_mutex_t mux_players;

/* global variables */
int server_socket;
int nr_players = 0;

/* global variable structs */
message msg; 
player *root = NULL;
player *current_player = NULL;


/*  Function used to add a new player to the game
    when the server receives a CONNECT message    */
void add_player(player **root, int new_port, char *new_address)
{
    player *new = malloc(sizeof(player));
    if (new == NULL)
    {
        perror("insert");
        exit(0);
    }
    
    new->next = NULL;
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


/*  Function used to remove player from the list of players
    the player to remove is identified by [address + port]     */
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


/*  Server thread used to comunicate with the clients
    this thread receives CONNECT, DISCONNECT and MOVE_BALL 
    messagens. When a MOVE_BALL is received, has to broadcast 
    the message to every player                                 */
void comunication_thread()
{
    socklen_t client_address_size = sizeof(struct sockaddr_in);
    client_address.sin_family = AF_INET;

    message message_in;
    char buf[12];
    int op;

    while(1)
    {
        // receive messagem from client
        op == recvfrom(server_socket, &message_in, sizeof(message_in), 0, (struct sockaddr *)&client_address, &client_address_size);
        if (op == -1)
            exit("connection unavailable");

        switch (message_in.type)
        {
            // add new player            
            case CONNECT:

                inet_ntop(AF_INET, &client_address.sin_addr, buf, 12);                 
                if (nr_players < MAX_PLAYERS)
                {
                    pthread_mutex_lock(&mux_players); 
                    add_player(&root, ntohs(client_address.sin_port), buf);
                    pthread_mutex_unlock(&mux_players);
                    nr_players++;
                }

                // if there's only one player, he starts playing; otherwise waits
                if (nr_players == 1)
                    current_player = root;

                break;

            // remove player
            case DISCONNECT:

                inet_ntop(AF_INET, &client_address.sin_addr, buf, 12); 

                pthread_mutex_lock(&mux_players); 
                remove_player(&root, ntohs(client_address.sin_port), buf);
                nr_players--;
                pthread_mutex_unlock(&mux_players);

                break;

            // has to broacast the message to every player
            case MOVE_BALL:

                op = ntohs(client_address.sin_port); 
                printf("\n\t[THREAD]: Recebi um MOVE_BALL com %d pontos do porto: %d", message_in.score, op); fflush(stdout);


                // broadcast ball position to every client
                for (player *temp_curr = root; temp_curr != NULL; temp_curr=temp_curr->next)
                {
                    if (temp_curr->address != current_player->address || temp_curr->port != current_player->port)
                    {
                        inet_ntop(AF_INET, temp_curr->address, &client_address.sin_addr, sizeof(client_address.sin_addr));
                        client_address.sin_port = htons(temp_curr->port);

                        message_in.type = MOVE_BALL;
                        op == sendto(server_socket, &message_in, sizeof(message_in), 0, (struct sockaddr*) &client_address, sizeof(client_address));
                        if (op == -1)
                            error("client unavailable");
                    }
                    else
                        temp_curr->score = message_in.score;
                }
                break;

            default:
                break;
        }   
    }
}


/*  Main method is responsible for launching the program 
    threads and for managing the game, deciding who plays and 
    how much time every player have the ball                    */ 
int main()
{

    printf("\n[MAIN]: Server running... "); fflush(stdout);

    /* create socket */
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket == -1)
        error("socket");


    /* define server socket */
    struct sockaddr_in server_address = 
    {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htons(INADDR_ANY),
        .sin_port = htons(SOCK_PORT)
    };


    /* bind */
    int op = bind(server_socket, (struct sockaddr *) &server_address, sizeof server_address);
    if (op == -1)
        error("bind");


    /* client socket */
    socklen_t client_address_size = sizeof(struct sockaddr_in);
    client_address.sin_family = AF_INET;
   

    /* thread that is listening the clients messages */
    pthread_t server_t_id;
    pthread_create(&server_t_id, NULL, comunication_thread, NULL);


    /*  mutex for using the player list, which is shared
        by all the program threads  */
    pthread_mutex_init(&mux_players, NULL); 


    /*  server send a ball to a player and after 10 seconds 
        it gives the ball to another player. if there's only
        one player the ball goes back to him.   */
    while (1)
    {

        //se houver jogadores, começa o jogo 
        if (nr_players > 0)
        {    

            // CHOSE current_player
            if (current_player->next!=NULL)
                current_player = current_player->next;
            else
                current_player = root;


            // SEND BALL
            msg.type = SEND_BALL;
            inet_ntop(AF_INET, current_player->address, &client_address.sin_addr, sizeof(client_address.sin_addr));
            client_address.sin_port = htons(current_player->port);                

            op == sendto(server_socket, &msg, sizeof(msg), 0, (struct sockaddr*) &client_address, sizeof(client_address));
            if (op == -1)
                error("client unavailable");       


            // wait
            sleep(MIN_RELEASE_TIME);

            
            // RELEASE BALL
            msg.type = RELEASE_BALL;
            inet_ntop(AF_INET, current_player->address, &client_address.sin_addr, sizeof(client_address.sin_addr));
            client_address.sin_port = htons(current_player->port);   

            op == sendto(server_socket, &msg, sizeof(msg), 0, (struct sockaddr*) &client_address, sizeof(client_address));
            if (op == -1)
                error("client unavailable"); 

        }
    }

    close(server_socket);
    printf("\n");

    return 0;
}