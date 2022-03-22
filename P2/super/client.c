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
#include "pong.c"

WINDOW *game_win;
WINDOW *message_win;

/* client has to know the paddles position and the ball position 
so that he can draw them os the window */

paddle_position_type paddles[MAX_PLAYERS];
ball_position_type ball;


/* client has to know which ID belongs to him */
int client_id;


/* client program lauches this thread that keeps on listening for server messages until
the client disconects from the server. in practice, the only type of messages that can be 
received are BOARD_UPDATE's so the client thread has to refresh de ball, the paddles and the
scoreboard by updating the new values for each one (they came inside de server message) */

void comunication_thread(void *arg)
{
    int server_socket = *(int*) arg;
    int op;

    message msg_received;

    while (1)
    {
        op = recv(server_socket, &msg_received, sizeof(msg_received), 0);
        if (op == -1)
            error("recv");

        else if (msg_received.type == BOARD_UPDATE)
        {
            client_id = msg_received.id;

            // apagar posição antiga da bola
            draw_ball(game_win, &ball, false);

            // apagar posição antiga dos paddles
            for (int i=0; i<MAX_PLAYERS; i++)       
                draw_paddle(game_win, paddles[i], ' ');
 
            // copiar a nova posição da bola
            ball.c = 'o';
            ball.x = msg_received.ball_pos.x;
            ball.y = msg_received.ball_pos.y;
            ball.up_hor_down = msg_received.ball_pos.up_hor_down;
            ball.left_ver_right = msg_received.ball_pos.left_ver_right;

            // copiar a nova posição dos paddles
            for (int i=0; i<MAX_PLAYERS; i++)
            {
                paddles[i].x = msg_received.paddles_pos[i].x;
                paddles[i].y = msg_received.paddles_pos[i].y;
                paddles[i].length = msg_received.paddles_pos[i].length;
            }    

            //desenhar a bola
            draw_ball(game_win, &ball, true);
            
            //desenhar os paddles
            for (int i=0; i<MAX_PLAYERS; i++)
            {
                if (i+1 == client_id)
                    draw_paddle(game_win, msg_received.paddles_pos[i], '=');
                else
                    draw_paddle(game_win, msg_received.paddles_pos[i], '_');
            }

            // update scoreboardx   
            for(int i=0; i<MAX_PLAYERS; i++)
            {
                if (msg_received.scoreboard[i] >= 0)
                    mvwprintw(message_win, i+1, 1, "P%d - %d", i+1, msg_received.scoreboard[i]);
            }
            mvwprintw(message_win, client_id, 8, " <---");

            // refresh message window
            wrefresh(message_win);

            // refresh game window
            wrefresh(game_win);
        }  
    }
}


/* client main method starts by establishing a connection with the socket and them launches
a thread that keep listening the server messages. in the main method the user is allow to 
use the keyboard for playing or for disconecting the server. each time the user presses one valid 
key for moving the paddle, a PADDLE_UPDATE is send to server */

int main (int argc, char *argv[])
{
    // validate serve address
    if (argc < 2)
        error("arg[1]");


    // socket
    int socket_main = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_main == -1)
        error("socket");


    // configure socket
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;    
    server_address.sin_port = htons(SOCK_PORT);    


    // send CONNECT
    int op = connect(socket_main, (struct sockaddr*) &server_address, sizeof(server_address));
    if (op == -1)
        error("connect");


    // server comunication thread
    pthread_t server_t_id;
    pthread_create(&server_t_id, NULL, comunication_thread, &socket_main); 


    //begin game
    initscr();		    	
    cbreak();				
    keypad(stdscr, TRUE);   
    noecho();			   

    game_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    box(game_win, 0, 0);	
    wrefresh(game_win);
    keypad(game_win, true);

    message_win = newwin(5, WINDOW_SIZE+10, WINDOW_SIZE, 0);
    box(message_win, 0 , 0);	
    wrefresh(message_win);


    // initial ball position
    place_ball_random(&ball);


    //setup
    message msg_client;
    int key;


    // until the user disconnects, keeps playing
    while(1)
    {
        key = wgetch(game_win);		

        // playing            
        if ((key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN))
        {             

            msg_client.type = PADDLE_MOVE;
            msg_client.move = key;
            msg_client.id = client_id;
            
            // copy paddles
            for (int i=0; i<MAX_PLAYERS; i++)
            {
                msg_client.paddles_pos[i].x = paddles[i].x;
                msg_client.paddles_pos[i].y = paddles[i].y;
                msg_client.paddles_pos[i].length = paddles[i].length;
            }  

            op == sendto(socket_main, &msg_client, sizeof(msg_client), 0, (struct sockaddr *) &server_address, sizeof(server_address));  
            if (op == -1)
                error("server unavailable");

        }    
        // disconnect
        else if (key = 'q')
        {
            msg_client.type = DISCONNECT;
            msg_client.id = client_id;

            op == sendto(socket_main, &msg_client, sizeof(msg_client), 0, (struct sockaddr *) &server_address, sizeof(server_address));  
            if (op == -1)
                error("server unavailable");
            else
                break;        
        }
    }

    endwin();
    close(socket_main);
    
    //pthread_cancel(comunication_thread, NULL);    
    //printf("\n"); fflush(stdout);

    return 0;
}