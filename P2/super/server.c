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
#include "pong.c"
#include <errno.h>

struct sockaddr_in server_address;
struct sockaddr_in client_address;


pthread_t server_thread_id;
pthread_t client_thread_id;


pthread_mutex_t mux_scoreboard;
pthread_mutex_t mux_players;
pthread_mutex_t mux_paddles;
pthread_mutex_t mux_ball;
pthread_mutex_t mux_send;


paddle_position_type paddles[MAX_PLAYERS];
ball_position_type ball;
player *root = NULL;


int scoreboard[MAX_PLAYERS];
int nr_players = 0;


/* this thread is launch when the client is connected to the game. every client has it's own, 
so, the server has one of these listening each client's messages. Client's can DISCONNECT or 
MOVE_PADDLE. In the first case the player is removed from the game; in the second the server
broadcasts the new paddle position for everyone  */

void player_comunication_thread(void *arg)
{
    int player_socket = *(int*) arg;

    message msg;
    char buf[12];
    int id;
    
    while (1)
    {
        
        int op = recv(player_socket, &msg, sizeof(msg), 0);
        if (op == -1) 
            error("\nclient unreachable");
        else
        {    
            switch(msg.type)
            {
                case (PADDLE_MOVE):
                    printf("\n\t\t[THREAD CLIENT]: PADDLE_MOVE received %d from player %d", msg.move, msg.id); fflush (stdout);

                    // update paddle position on server
                    pthread_mutex_lock(&mux_paddles); 
                    move_paddle(&paddles, &ball, msg.move, msg.id);
                    pthread_mutex_unlock(&mux_paddles); 

                    // broadcast update
                    msg.type = PADDLE_MOVE;

                    // send broadcast
                    for (player *curr = root; curr != NULL; curr=curr->next)
                    {
                        //copy ball
                        pthread_mutex_lock(&mux_ball);
                        msg.ball_pos = ball;
                        pthread_mutex_unlock(&mux_ball);

                        // copy paddle position
                        pthread_mutex_lock(&mux_paddles); 
                        for (int i=0; i<MAX_PLAYERS; i++)
                        {
                            msg.paddles_pos[i].x = paddles[i].x;
                            msg.paddles_pos[i].y = paddles[i].y;
                            msg.paddles_pos[i].length = paddles[i].length;
                        }       
                        pthread_mutex_unlock(&mux_paddles); 

                        // update player id
                        msg.id = curr->id;

                        // sync send mensage with main thread
                        pthread_mutex_lock(&mux_send); 
                        send(curr->socket, &msg, sizeof(msg), 0);   
                        pthread_mutex_unlock(&mux_send); 
                }
    
                    break;

                case (DISCONNECT):

                    // get client address
                    inet_ntop(AF_INET, &client_address.sin_addr, buf, 12); 
                    
                    // find player ID:
                    id = msg.id;
                    printf("\n\t\t[THREAD CLIENT]: DISCONNECT received from player %d", id); fflush (stdout);

                    //remove player
                    pthread_mutex_lock(&mux_players); 
                    remove_player(&root, msg.id);
                    nr_players--;
                    pthread_mutex_unlock(&mux_players);

                    // list players
                    pthread_mutex_lock(&mux_players);
                    for (player *curr = root; curr != NULL; curr=curr->next)
                        printf("\n\t[THREAD]: player id: %d \tport: %d, address: %s, socket: %d", curr->id, curr->port, curr->address, curr->socket); fflush(stdout);
                    pthread_mutex_unlock(&mux_players);

                    //remove paddle
                    pthread_mutex_lock(&mux_paddles); 
                    paddles[id-1].x = paddles[id-1].y = -1;
                    pthread_mutex_unlock(&mux_paddles); 

                    //remove score
                    pthread_mutex_lock(&mux_scoreboard); 
                    scoreboard[id-1] = -1;
                    pthread_mutex_unlock(&mux_scoreboard); 

                    // close socket
                    close(player_socket);

                    // end thread
                    pthread_exit(NULL);

                    break;

                default:
                    break;
            }
        }
    }
}


/* this thread is lauched by the server main method and is responsible for keep on listening
new CONNECT messages. The game has a maximum number of players which should be respected, so
this thread deals with that number os players management */

void server_thread(void *arg)
{
    printf("\n\t[THREAD]: Start waiting connections..."); fflush(stdout);

    /* client socket */
    socklen_t client_address_size = sizeof(struct sockaddr_in);
    client_address.sin_family = AF_INET;

    int server_socket = *(int*) arg;
    
    char buf[12];
    int new_client_socket;

    while(1)
    {
        // receive player
        if (nr_players < MAX_PLAYERS)
        {
            new_client_socket = accept(server_socket, (struct sockaddr*) &client_address, &client_address_size);
            printf("\n\t[THREAD]: Client arrived!"); fflush(stdout);

            if (new_client_socket == -1)
                error("accept"); 
            else
            {
                // get player address
                inet_ntop(AF_INET, &client_address.sin_addr, buf, 12);                 

                // add new player
                pthread_mutex_lock(&mux_players);
                nr_players++;
                add_player(&root, ntohs(client_address.sin_port), buf, new_client_socket, nr_players);
                pthread_mutex_unlock(&mux_players);

                //thread to comunicate with this player
                pthread_create(&client_thread_id, NULL, player_comunication_thread, &new_client_socket);
        
                // show active players
                pthread_mutex_lock(&mux_players);
                for (player *curr = root; curr != NULL; curr=curr->next)
                    printf("\n\t[THREAD]: player id: %d \tport: %d, address: %s, socket: %d", curr->id, curr->port, curr->address, curr->socket); fflush(stdout);
                pthread_mutex_unlock(&mux_players);

                // update scoreboard
                pthread_mutex_lock(&mux_scoreboard);
                scoreboard[nr_players-1] = 0;
                pthread_mutex_unlock(&mux_scoreboard);
            }
        }
    }

}


/* In the super pong game version is the server that has to calculate the ball_position from
time to time. that work is made on the main method, which calculates the new ball position and
broadcast it to all the players in a BOARD_UPDATE.
   If the are no players in the game, it waits for connections */
int main()
{
    printf("\n[MAIN]: Server running... "); fflush(stdout);

    /* create socket */
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
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


    /* listen */
    op = listen(server_socket, 10);
    if(op == -1) 
        error("listen");


    /* game setup */
    message msg_send;
    msg_send.type = BOARD_UPDATE;
    place_ball_random(&ball);

    /* init paddles */
    init_paddles(&paddles, &scoreboard);
    int mvp;

    /* muxes */
    pthread_mutex_init(&mux_players, NULL);
    pthread_mutex_init(&mux_paddles, NULL);
    pthread_mutex_init(&mux_ball, NULL);
    pthread_mutex_init(&mux_scoreboard, NULL);
    pthread_mutex_init(&mux_send, NULL);

    /* thread takes care of the incomming connections */
    pthread_create(&server_thread_id, NULL, server_thread, &server_socket);


    while(1)
    {
        sleep(BALL_SPEED);

        if (nr_players > 0)
        {
            printf("\n[MAIN]: From second to second I send a board_update"); fflush(stdout);
            sleep(BALL_SPEED);

            mvp = move_ball(&ball, &paddles);
            printf("\n[MAIN]: New ball position: (%d,%d)", ball.x, ball.y);

            /* update score + copy to send*/
            pthread_mutex_lock(&mux_scoreboard); 
            if (mvp > 0)
                scoreboard[mvp-1]++;
            for(int i=0; i<MAX_PLAYERS; i++)
                msg_send.scoreboard[i] = scoreboard[i];
            pthread_mutex_unlock(&mux_scoreboard); 

            for (player *curr = root; curr != NULL; curr=curr->next)
            {
                // copy ball to the message that is gonna be sended
                pthread_mutex_lock(&mux_ball);
                msg_send.ball_pos = ball;
                pthread_mutex_unlock(&mux_ball);

                // copy paddle position to the message that is gonna be sended
                pthread_mutex_lock(&mux_paddles); 
                for (int i=0; i<MAX_PLAYERS; i++)
                {
                    msg_send.paddles_pos[i].x = paddles[i].x;
                    msg_send.paddles_pos[i].y = paddles[i].y;
                    msg_send.paddles_pos[i].length = paddles[i].length;
                }       
                pthread_mutex_unlock(&mux_paddles); 

                // update player id
                msg_send.id = curr->id;

                // send message with sync with threads
                pthread_mutex_lock(&mux_send); 
                send(curr->socket, &msg_send, sizeof(msg_send), 0);   
                pthread_mutex_unlock(&mux_send); 
            }
        }
        else
            printf("\n[MAIN]: Waiting for players to get in...");
    }


    // close thread
    pthread_cancel(server_thread, NULL);

    // close socket
    close(server_socket);

    return 0;

}