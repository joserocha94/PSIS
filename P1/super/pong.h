#define PADLE_SIZE 2
#define MAX_PLAYERS 2
#define MIN_RELEASE_TIME 10
#define WINDOW_SIZE 20
#define SOCK_PORT 5000
#define SOCK_ADDRESS "/tmp/sock_16"


typedef enum player_state { WAIT, PLAY } player_state;


typedef struct paddle_position_type
{
    int x, y;
    int length;
} paddle_position_type;


typedef struct player_type
{
    int id;
    int port;
    int score;
    char *address;
    struct player_type *next;
} player_type;


typedef struct ball_position_type 
{
    int x, y;
    int up_hor_down;    
    int left_ver_right; 
    char c;
} ball_position_type;


typedef enum message_type 
{ 
    conn, 
    disconnect,
    board_update,
    paddle_move 
} message_type;


typedef enum move_type
{
    right,
    left,
    up,
    down
} move_type;


typedef struct message
{
    int player_id;
    move_type move;
    message_type type;
    paddle_position_type paddle[MAX_PLAYERS];
    ball_position_type ball;
} message;
