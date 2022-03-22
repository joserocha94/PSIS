#define MAX_PLAYERS 10
#define MIN_RELEASE_TIME 10
#define PADLE_SIZE 2
#define BALL_SPEED 1
#define WINDOW_SIZE 20
#define SOCK_PORT 5000
#define SOCK_ADDRESS "/tmp/sock_16"


typedef struct player
{
    int id;
    int port;
    int score;
    int socket;
    char *address;
    struct player *next;
} player;


typedef struct ball_position_type 
{
    char c;
    int x, y;
    int up_hor_down;    
    int left_ver_right; 
} ball_position_type;


typedef struct paddle_position_type
{
    int x, y;
    int length;
} paddle_position_type;


typedef enum message_type 
{ 
    CONNECT, 
    BOARD_UPDATE, 
    PADDLE_MOVE, 
    DISCONNECT 
} message_type;


typedef struct message
{
    int id;
    int move;
    int scoreboard[MAX_PLAYERS];
    message_type type;
    ball_position_type ball_pos;
    paddle_position_type paddles_pos[MAX_PLAYERS];
} message;

