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

WINDOW * message_win;

paddle_position_type paddle;
ball_position_type ball;

void error(char *msg)
{
    perror(msg);
    exit(0);
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

void moove_ball(ball_position_type *ball, paddle_position_type *paddle){
    
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

    if (paddle->y == ball->y && next_x > (paddle->x - paddle->length) && next_x < (paddle->x + paddle->length) )
    {
        ball->up_hor_down *= -1;
    }

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

void update_score(WINDOW **msg_win, player_state *state)
{

    if (state == WAIT)
    {
        mvwprintw(message_win, 1,1, "                      ");
        mvwprintw(message_win, 2,1, "                      ");
        mvwprintw(message_win, 1,1, "WAIT");
    }
    else if (state == PLAY)
    {
        mvwprintw(message_win, 1,1,"PLAY");
    }

    wrefresh(*msg_win);  
    wrefresh(message_win);  
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


int main (int argc, char *argv[])
{

    // create socket
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd == -1)
        error("socket");


    // configure socket
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SOCK_PORT);

    int op = inet_pton(AF_INET, argv[1], &server_addr.sin_addr);
    if (op == -1)
        error("no valid address");


    // send connection
    message message;
    message.type = conn;

    op = sendto(socket_fd, &message, sizeof(message), 0, (struct sockaddr *) &server_addr, sizeof(server_addr));   
    if (op == -1)
        error("can't connect");


    // begin game
    initscr();		    	
	cbreak();				
    keypad(stdscr, TRUE);   
	noecho();			   

    WINDOW * my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    box(my_win, 0, 0);	
	wrefresh(my_win);
    keypad(my_win, true);

    message_win = newwin(5, WINDOW_SIZE+10, WINDOW_SIZE, 0);
    box(message_win, 0 , 0);	
	wrefresh(message_win);

    new_paddle(&paddle, PADLE_SIZE);
    place_ball_random(&ball);
    player_state player_state = WAIT;

    time_t start, end;

    // play
    int key = -1;
    while (1)
    {
        // WAITING STATE
        if (player_state == WAIT)
        {
            draw_paddle(my_win, &paddle, false);
            update_score(&message_win, WAIT);            
            recv(socket_fd, &message, sizeof(message), 0);

            // começa a jogar
            if (message.type == send_ball)
            {
                player_state = PLAY;
                start = time(NULL);
            }

            // atualiza ecrã
            else if (message.type == move_ball)
            {
                draw_ball(my_win, &ball, false);
                draw_paddle(my_win, &paddle, false);
                ball = get_ball_position(&message.ball_pos);
                draw_ball(my_win, &ball, true);
            }       
        }

        // PLAYING STATE
        else if (player_state == PLAY)
        {

            draw_paddle(my_win, &paddle, true);
            update_score(&message_win, PLAY);
            key = wgetch(my_win);		

            // playing            
            if (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN)
            {
                draw_paddle(my_win, &paddle, false);
                moove_paddle (&paddle, key);
                draw_paddle(my_win, &paddle, true);

                draw_ball(my_win, &ball, false);
                moove_ball(&ball, &paddle);
                draw_ball(my_win, &ball, true);

                message.ball_pos = get_ball_position(&ball);
                message.type = move_ball;

                op = sendto(socket_fd, &message, sizeof(message), 0, (struct sockaddr *) &server_addr, sizeof(server_addr));  
                if (op != -1)
                    player_state = PLAY; 
                else
                    error("server unavailable");
            }

            // release ball
            else if (key == 'r')
            {         
                end = time(NULL);
                if (end - start > MIN_RELEASE_TIME)
                {
                    message.ball_pos = get_ball_position(&ball);
                    message.type = release_ball;
                    
                    op == sendto(socket_fd, &message, sizeof(message), 0, (struct sockaddr *) &server_addr, sizeof(server_addr));  
                    if (op != -1)
                        player_state = WAIT; 
                    else
                        error("server unavailable");
                }
            }

            // disconnect
            else if (key == 'q')
            {         
                message.ball_pos = get_ball_position(&ball);
                message.type = disconnect;

                op == sendto(socket_fd, &message, sizeof(message), 0, (struct sockaddr *) &server_addr, sizeof(server_addr));  
                if (op != -1)
                    break;
                else
                    error("server unavailable");
            }

            mvwprintw(message_win, 1, 1, "PLAY %c key pressed", key);
            wrefresh(message_win);	
        }
    }

  	endwin();
    close(socket_fd);      
    
    printf("\n");
    printf("Disconected from the server");
    printf("\n");

    exit(0);

}