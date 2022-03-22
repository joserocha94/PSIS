#include <ncurses.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>  
#include <ctype.h> 
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include "pong.h"


struct sockaddr_in server_addr;
struct sockaddr_in client_addr;
ball_position_type ball;
paddle_position_type paddles[MAX_PLAYERS];


void place_ball_random(ball_position_type * ball){
    ball->x = rand() % WINDOW_SIZE ;
    ball->y = rand() % WINDOW_SIZE ;
    ball->c = 'o';
    ball->up_hor_down = rand() % 3 -1; 
    ball->left_ver_right = rand() % 3 -1 ; 
}

void new_paddle (paddle_position_type * paddle, int legth){
    paddle->x = WINDOW_SIZE/2;
    paddle->y = WINDOW_SIZE-2;
    paddle->length = legth;
}

void add_player(player_type **root, int id, int new_port, char *new_address)
{
    player_type *new = malloc(sizeof(player_type));
    if (new == NULL)
    {
        perror("insert");
        exit(0);
    }

    paddle_position_type paddle;
    new_paddle(&paddle, PADLE_SIZE);

    new->id = id+1;    
    new->score = 0;
    new->port = new_port;
    new->address = new_address;
    new->next = NULL;

    if(*root == NULL)
    {
        *root = new;
        return;
    }

    player_type *curr = *root;
    while (curr->next != NULL)
    {
        curr = curr->next;
    }
    curr->next = new;

    paddles[id].x = paddle.x;
    paddles[id].y = paddle.y;
    paddles[id].length = paddle.length;

}

void moove_paddle (paddle_position_type * paddle, move_type direction)
{
    if (direction == up){
        if (paddle->y  != 1){
            paddle->y --;
        }
    }

    if (direction == down){
        if (paddle->y  != WINDOW_SIZE-2){
            paddle->y ++;
        }
    }  

    if (direction == left){
        if (paddle->x - paddle->length != 1){
            paddle->x --;
        }
    }

    if (direction == right)
        if (paddle->x + paddle->length != WINDOW_SIZE-2){
            paddle->x ++;
    }
}

void remove_player(player_type **root, int port, char *address)
{

    if ((*root)->port == port && (*root)->address == address)
    {
        player_type *to_remove = *root;
        *root = (*root)->next;
        free(to_remove);

        return;
    }
    
    for (player_type *curr = *root; curr->next != NULL; curr = curr->next)
    {
        if (curr->next->port == port && curr->next->address == address)
        {
            player_type *to_remove = curr->next;
            curr->next = curr->next->next;
            free(to_remove);

            return;
        }
    }
}



int main (int argc, char* argv[])
{
    printf("\nRUNNING...");

    message message; 
    player_type *root = NULL;


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
    place_ball_random(&ball);   


    /* all paddles disabled */
    for (int i=0; i<MAX_PLAYERS; i++)
    {
        paddles[i].x = 0;
        paddles[i].y = 0;
        paddles[i].length = 0;
    } 

    /* run */
    while (1)
    {

        printf("\nWaiting message...");
        recvfrom(socket_fd, &message, sizeof(message), 0, (struct sockaddr *)&client_addr, &client_addr_size);
        printf("\nReceived message...");
        
        switch (message.type)
        {
            // add new player    
            case conn:
        
                inet_ntop(AF_INET, &client_addr.sin_addr, buf, 12); 

                //verifica se há vaga
                if (nr_players < MAX_PLAYERS)
                {
                    add_player(&root, nr_players, ntohs(client_addr.sin_port), buf);
                    nr_players++;
                }

                // send message
                message.type = board_update;
                message.player_id = nr_players;

                // send ball position
                message.ball.x = ball.x;
                message.ball.y = ball.y;
                message.ball.left_ver_right = ball.left_ver_right;
                message.ball.up_hor_down = ball.up_hor_down;

                // send paddle position
                message.paddle[nr_players-1].x = paddles[nr_players-1].x;
                message.paddle[nr_players-1].y = paddles[nr_players-1].y;
                message.paddle[nr_players-1].length = paddles[nr_players-1].length = PADLE_SIZE;

                // send ok
                op = sendto(socket_fd, &message, sizeof(message), 0, (struct sockaddr*) &client_addr, sizeof(client_addr));
                if (op == -1)
                    error("client unavailable");

                break;

            case disconnect:

                // remove player
                inet_ntop(AF_INET, &client_addr.sin_addr, buf, 12); 
                remove_player(&root, ntohs(client_addr.sin_port), buf);
                nr_players--;   

                // remove paddle
                paddles[message.player_id-1].length = 0;
                paddles[message.player_id-1].x = 0;
                paddles[message.player_id-1].x = 0;

                // update screen without this player
                message.type = paddle_move; 
                break;

            case paddle_move:

                // calculate paddle moviment
                for (int i=0; i<MAX_PLAYERS; i++)
                    moove_paddle(&paddles[i], message.move);


                // send paddles position updated
                for (int i=0; i<MAX_PLAYERS; i++)
                {
                    message.paddle[i].x = paddles[i].x;
                    message.paddle[i].y = paddles[i].y;
                    message.paddle[i].length = paddles[i].length;

                }

                // send to client
                message.type = board_update;

                // update for every client
                for (player_type *temp_curr = root; temp_curr != NULL; temp_curr=temp_curr->next)
                {

                    inet_ntop(AF_INET, temp_curr->address, &client_addr.sin_addr, sizeof(client_addr.sin_addr));
                    client_addr.sin_port = htons(temp_curr->port);

                    op == sendto(socket_fd, &message, sizeof(message), 0, (struct sockaddr*) &client_addr, sizeof(client_addr));
                    if (op == -1)
                        error("client unavailable");

                }
                

            default:
                break;
        }

        printf("\n");
    }

    printf("\n");
    return 0;

}

