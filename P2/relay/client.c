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

WINDOW *message_win;
WINDOW *my_win;

player_state my_state;
paddle_position_type paddle;
ball_position_type ball;

pthread_mutex_t mux_board;
pthread_mutex_t mux_state;
pthread_mutex_t mux_score;
pthread_mutex_t mux_paddle;
pthread_mutex_t mux_ball;

int score = 0;

void change_state(player_state new_state)
{
    pthread_mutex_lock(&mux_state); 
    my_state = new_state;
    pthread_mutex_unlock(&mux_state);  
}

void new_paddle (paddle_position_type * paddle, int legth){
    paddle->x = WINDOW_SIZE/2;
    paddle->y = WINDOW_SIZE-2;
    paddle->length = legth;
}

void draw_paddle(WINDOW *win, paddle_position_type * paddle, int delete){
    int ch;
    if(delete){
        ch = '_';
    }else{
        ch = ' ';
    }
    int start_x = paddle->x - paddle->length;
    int end_x = paddle->x + paddle->length;
    for (int x = start_x; x <= end_x; x++){
        wmove(win, paddle->y, x);
        waddch(win,ch);
    }
    wrefresh(win);
}

void moove_paddle (paddle_position_type * paddle, int direction){
    if (direction == KEY_UP){
        if (paddle->y  != 1){
            paddle->y --;
        }
    }
    if (direction == KEY_DOWN){
        if (paddle->y  != WINDOW_SIZE-2){
            paddle->y ++;
        }
    }
    

    if (direction == KEY_LEFT){
        if (paddle->x - paddle->length != 1){
            paddle->x --;
        }
    }
    if (direction == KEY_RIGHT)
        if (paddle->x + paddle->length != WINDOW_SIZE-2){
            paddle->x ++;
    }
}

void place_ball_random(ball_position_type * ball){
    ball->x = rand() % WINDOW_SIZE ;
    ball->y = rand() % WINDOW_SIZE ;
    ball->c = 'o';
    ball->up_hor_down = rand() % 3 -1; 
    ball->left_ver_right = rand() % 3 -1 ; 
}

void draw_ball(WINDOW *win, ball_position_type * ball, int draw){
    int ch;
    if(draw){
        ch = ball->c;
    }else{
        ch = ' ';
    }
    wmove(win, ball->y, ball->x);
    waddch(win,ch);
    wrefresh(win);
}

ball_position_type get_ball_position(ball_position_type *current_ball)
{
    ball_position_type msg;

    msg.x = current_ball->x;
    msg.y = current_ball->y;

    msg.left_ver_right = current_ball->left_ver_right;
    msg.up_hor_down = current_ball->up_hor_down;

    msg.c = current_ball->c;

    return msg;
}


/* function responsible for updating the score window,
which should reflect the player state: ILDE or PLAYING */   

void update_score_win(WINDOW **msg_win)
{

    pthread_mutex_lock(&mux_score); 
    if (my_state == IDLE)
    {
        mvwprintw(message_win, 1,1, "                      ");
        wrefresh(message_win);      
        mvwprintw(message_win, 2,1, "                      ");
        mvwprintw(message_win, 1,1, "IDLE                  ");
    }
    else if (my_state == PLAY)
    {
        mvwprintw(message_win, 1,1, "                      ");
        wrefresh(message_win);      
        mvwprintw(message_win, 2,1, "                      ");
        mvwprintw(message_win, 1,1, "PLAY                  ");
    }

    wrefresh(*msg_win);  
    wrefresh(message_win);  
    pthread_mutex_unlock(&mux_score); 
}


/* this function is responsible for calculating the new 
ball position and checks if it has it the player paddle. 
in positive case it returns a point (>0) that should be
given to the player who owns the paddle */

int moove_ball(ball_position_type *ball, paddle_position_type *paddle){

    int point = 0;

    if (my_state == PLAY)
    {

        int next_x = ball->x + ball->left_ver_right;
        if( next_x == 0 || next_x == WINDOW_SIZE-1)
        {
            ball->up_hor_down = rand() % 3 -1 ;
            ball->left_ver_right *= -1;

            mvwprintw(message_win, 2, 1, "left right win");
            wrefresh(message_win);
        }
        else
        {
            ball->x = next_x;
        }
    
        int next_y = ball->y + ball->up_hor_down;
        if( next_y == 0 || next_y == WINDOW_SIZE-1)
        {
            ball->up_hor_down *= -1;
            ball->left_ver_right = rand() % 3 -1;
            mvwprintw(message_win, 2, 1, "bottom top win");
            wrefresh(message_win);
        }
        else
        {
            ball -> y = next_y;
        }

        // hit
        if (paddle->y == ball->y && next_x > (paddle->x - paddle->length) && next_x < (paddle->x + paddle->length) )
        {
            ball->up_hor_down *= -1;
            point++;
        }
    }
    return point;
}


/*  since the ball has to be updated every second
    this thread calculates the new ball position 
    and sends it to the server. também faz a atualização
    da bola no ecrã do jogador atual       */

void *move_ball_thread(void * arg)
{
    // socket
    int socket_fd = *(int*) arg;
    int op;

    // configure socket
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;    
    server_addr.sin_port = htons(SOCK_PORT);

    message mbt_msg;

    while(1)
    {
        sleep(BALL_SPEED);
     
        if (my_state == PLAY)
        {
            // Step 1
            // limpar posição anterior da bola
            pthread_mutex_lock(&mux_board);
            draw_ball(my_win, &ball, false);
            pthread_mutex_unlock(&mux_board);
            
            // calcular nova posição da bola
            pthread_mutex_lock(&mux_ball);
            score += moove_ball(&ball, &paddle);
            pthread_mutex_unlock(&mux_ball);
            
            // desenhar nova bola
            pthread_mutex_lock(&mux_board);
            draw_ball(my_win, &ball, true);
            pthread_mutex_unlock(&mux_board);

            // Step 2
            // enviar a nova posição da bola ao servidor
            mbt_msg.ball_pos = get_ball_position(&ball);
            mbt_msg.type = MOVE_BALL;
            mbt_msg.score = score;

            op = sendto(socket_fd, &mbt_msg, sizeof(mbt_msg), 0, (struct sockaddr *) &server_addr, sizeof(server_addr));  

            if (op == -1)
                error("server unavailable");
            else
                my_state = PLAY;  
        }

    }
}


/*  this thread is responsible for maintain a communication 
    with the server about the state of the game. 
    it starts by sending a connection, and after that keeps 
    on waiting for server messages. from them, the player state
    changes from IDLE to PLAY which allows the user to user the
    keyboard for playing the game */

void comunication_thread(void *arg)
{
    int socket_fd = *(int*) arg;

    // configure socket
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;    
    server_addr.sin_port = htons(SOCK_PORT);
  
    // send connection
    message message;
    message.type = CONNECT;

    int op = sendto(socket_fd, &message, sizeof(message), 0, (struct sockaddr *) &server_addr, sizeof(server_addr));   
    if (op == -1)
        error("can't connect");

    change_state(IDLE);
    pthread_t client_t_id;
    pthread_create(&client_t_id, NULL, move_ball_thread, &socket_fd);
    
    while (1)
    {   
        // recebe nova mensagem do servidor
        recv(socket_fd, &message, sizeof(message), 0);    

        if (message.type == MOVE_BALL)
        {
            //apagar bola antiga
            pthread_mutex_lock(&mux_ball); 
            draw_ball(my_win, &ball, false);
            pthread_mutex_unlock(&mux_ball); 

            // get new ball
            ball = get_ball_position(&message.ball_pos);

            //draw new ball
            pthread_mutex_lock(&mux_ball); 
            draw_ball(my_win, &ball, true);
            pthread_mutex_unlock(&mux_ball);     

            // keep state IDLE
            change_state(IDLE);
        }

        else if (message.type == RELEASE_BALL)
        {
            // atualiza estado do jogador
            change_state(IDLE);   

            // apagar antigo paddle
            pthread_mutex_lock(&mux_board); 
            draw_paddle(my_win, &paddle, false);
            pthread_mutex_unlock(&mux_board);  

            // update score
            update_score_win(&message_win);          
        }

        else if (message.type == SEND_BALL)
        {
            // atualiza estado do jogador
            change_state(PLAY);   
            
            // desenhar novo paddle
            pthread_mutex_lock(&mux_board); 
            draw_paddle(my_win, &paddle, true);
            pthread_mutex_unlock(&mux_board);

            // update score
            update_score_win(&message_win); 
        }
    }
    close(socket_fd);
}


/* the main is responsible for lauching the thread
that connects with the server; after that when the player
is on PLAYING STATE it read keyboard inputs and sends
them back to the server */

int main (int argc, char *argv[])
{    
    // check server address
    if(argc < 2)
        error("missing server address");    

    // socket
    int socket_main = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_main == -1)
        error("socket");

    // configure socket
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;    
    server_addr.sin_port = htons(SOCK_PORT);    

    // server comunication thread
    pthread_t server_t_id;
    pthread_create(&server_t_id, NULL, comunication_thread, &socket_main);

    // mutexes 
    pthread_mutex_init(&mux_board, NULL);  
    pthread_mutex_init(&mux_score, NULL);    
    pthread_mutex_init(&mux_ball, NULL); 
    pthread_mutex_init(&mux_state, NULL); 
    pthread_mutex_init(&mux_paddle, NULL);    

    // message
    message msg_quit;

    // begin game
    initscr();		    	
    cbreak();				
    keypad(stdscr, TRUE);   
    noecho();			   

    my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    box(my_win, 0, 0);	
    wrefresh(my_win);
    keypad(my_win, true);

    message_win = newwin(5, WINDOW_SIZE+10, WINDOW_SIZE, 0);
    box(message_win, 0 , 0);	
    wrefresh(message_win);

    new_paddle(&paddle, PADLE_SIZE);
    place_ball_random(&ball); 
    update_score_win(&message_win);    

    int key, op;

    // until the player quits the game ('q') it reads inputs
    while (1)
    {
        key = wgetch(my_win);		

        // playing            
        if ((key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN) && my_state == PLAY)
        {             
            // apagar antigo paddle
            pthread_mutex_lock(&mux_board); 
            draw_paddle(my_win, &paddle, false);
            pthread_mutex_unlock(&mux_board);  

            // mover paddle
            pthread_mutex_lock(&mux_paddle); 
            moove_paddle (&paddle, key);
            pthread_mutex_unlock(&mux_paddle);  

            // desenhar novo paddle
            pthread_mutex_lock(&mux_board); 
            draw_paddle(my_win, &paddle, true);
            pthread_mutex_unlock(&mux_board);              

            //refresh window
            mvwprintw(message_win, 1, 1, "                   ", 0);
            mvwprintw(message_win, 1, 1, "PLAY %c key pressed", key);
            wrefresh(message_win);	 
        }    
        else if (key = 'q')
        {
            msg_quit.type = DISCONNECT;
            op == sendto(socket_main, &msg_quit, sizeof(msg_quit), 0, (struct sockaddr *) &server_addr, sizeof(server_addr));  
            if (op == -1)
                error("server unavailable");
            else
                break;
        }      
    }

  	endwin();

    printf("\n");
    return 0;
}