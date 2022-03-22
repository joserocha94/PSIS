/* SUPER PONG lib */

/* lida com os erros da vida e do jogo*/
void error(char *msg)
{
    perror(msg);
    exit(0);
}


/* add new client to the game*/
void add_player(player **root, int new_port, char *new_address, int new_socket, int new_id)
{
    player *new = malloc(sizeof(player));
    if (new == NULL)
    {
        perror("insert");
        exit(0);
    }
    
    new->next = NULL;
    new->score = 0;
    new->id = new_id;
    new->port = new_port;
    new->address = new_address;
    new->socket = new_socket;

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


/* remove player from the game */
void remove_player(player **root, int id)
{

    if ((*root)->id == id)
    {
        player *to_remove = *root;
        *root = (*root)->next;
        free(to_remove);

        return;
    }
    
    for (player *curr = *root; curr->next != NULL; curr = curr->next)
    {
        if (curr->next->id == id)
        {
            player *to_remove = curr->next;
            curr->next = curr->next->next;
            free(to_remove);

            return;
        }
    }
}


/* coloca a bola numa posição random */
void place_ball_random(ball_position_type * ball)
{
    ball->c = 'o';

    ball->x = rand() % WINDOW_SIZE ;
    ball->y = rand() % WINDOW_SIZE ;

    ball->up_hor_down = rand() % 3-1; 
    ball->left_ver_right = rand() % 3-1 ; 
}


/*  calcula o movimento da bola tendo em conta duas situações: verifica se bate nalgum paddle (1); 
e verifica se bate nos limites da janela (2); no caso de bater no paddle de um jogador identifica 
quem foi e devolve o seu plyer_id ao servidor para este atualizar o score */
int move_ball(ball_position_type *ball, paddle_position_type *paddles){

    int next_x = ball->x + ball->left_ver_right;
    int next_y = ball->y + ball->up_hor_down;
    
    // constraint 1
    for (int i=0; i<MAX_PLAYERS; i++)
    {
        // bateu no paddle
        if (paddles[i].y == ball->y && 
            next_x < (paddles[i].x + paddles[i].length) &&
            next_x > (paddles[i].x - paddles[i].length))
        {
            ball->up_hor_down *= -1;
            ball->left_ver_right *= -1;

            ball->x += ball->up_hor_down;
            ball->y += ball->left_ver_right;

            return i+1;
        }
    }
    
    // constraint 2
    if( next_x == 0 || next_x == WINDOW_SIZE-1)
    {
        ball->up_hor_down = rand() % 3 -1 ;
        ball->left_ver_right *= -1;

    }
    else
        ball->x = next_x;

    if( next_y == 0 || next_y == WINDOW_SIZE-1)
    {
        ball->up_hor_down *= -1;
        ball->left_ver_right = rand() % 3 -1;
    }
    else
        ball -> y = next_y;


    return 0;
}


/* desenha a bola */
void draw_ball(WINDOW *win, ball_position_type * ball, int draw)
{
    int ch;
    if(draw)
        ch = ball->c;
    else
        ch = ' ';
    
    wmove(win, ball->y, ball->x);
    waddch(win,ch);
    wrefresh(win);
}


/* obtem player_id via player_socket */
int get_player_id(player **root, int player_socket)
{
    for (player *curr = root; curr != NULL; curr=curr->next)
        if (curr->socket == player_socket)
            return (curr->id);
}


/* move o paddle tendo em conta que este não pode bater noutros paddles (2)
e que também não pode bater na bola (3); só há ponto se for a bola a bater no paddle! */
void move_paddle (paddle_position_type *paddles, ball_position_type *ball, int direction, int player_id)
{

    int index = player_id-1;
    int old_x = paddles[index].x;
    int old_y = paddles[index].y;

    // constraint 1
    if (direction == KEY_UP)
        if (paddles[index].y != 1)
            paddles[index].y --;

    if (direction == KEY_DOWN)
        if (paddles[index].y != WINDOW_SIZE-2)
            paddles[index].y ++;

    if (direction == KEY_LEFT)
        if (paddles[index].x - paddles[index].length != 1)
            paddles[index].x --;
        
    if (direction == KEY_RIGHT)
        if (paddles[index].x + paddles[index].length != WINDOW_SIZE-2)
            paddles[index].x ++;
    
    // constraint 2
    for(int i=0; i<MAX_PLAYERS; i++)
    {
        if (i != index 
            && paddles[index].y == paddles[i].y 
            && (paddles[index].x+PADLE_SIZE) >= (paddles[i].x-PADLE_SIZE) 
            && (paddles[index].x-PADLE_SIZE) <= (paddles[i].x+PADLE_SIZE))
        {
            paddles[index].x = old_x;
            paddles[index].y = old_y;
        }
    }

    // constraint 3
    if (paddles[index].x + PADLE_SIZE >= ball->x && paddles[index].y == ball->y ||
        paddles[index].x - PADLE_SIZE <= ball->x && paddles[index].y == ball->y )
    {
        paddles[index].x = old_x;
        paddles[index].y = old_y;   
    }


}


/* desenha o paddle */
void draw_paddle(WINDOW *win, paddle_position_type paddle, int ch)
{
    int start_x = paddle.x - paddle.length;
    int end_x = paddle.x + paddle.length;

    if(paddle.x <= 0 || paddle.y <= 0)
        return;

    for (int x = start_x; x <= end_x; x++)
    {
        wmove(win, paddle.y, x);
        waddch(win,ch);
    }
    wrefresh(win);
}


/* coloca os valores a -1 para o inicio do jogo */
void init_paddles(paddle_position_type *paddles, int scoreboard[MAX_PLAYERS])
{
    for (int i=0; i<MAX_PLAYERS; i++)
    {
        paddles[i].x = paddles[i].y = scoreboard[i] = -1;
        paddles[i].length = PADLE_SIZE;
    }
}