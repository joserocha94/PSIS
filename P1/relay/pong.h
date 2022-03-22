#define PADLE_SIZE 2
#define MAX_PLAYERS 10
#define MIN_RELEASE_TIME 10
#define WINDOW_SIZE 20
#define SOCK_PORT 5000
#define SOCK_ADDRESS "/tmp/sock_16"

typedef enum player_state { WAIT, PLAY } player_state;

typedef struct player
{
    int port;
    int active;
    char *address;
    struct player *next;
} player;

typedef struct ball_position_type 
{
    int x, y;
    int up_hor_down;    
    int left_ver_right; 
    char c;
} ball_position_type;


typedef struct paddle_position_type
{
    int x, y;
    int length;
} paddle_position_type;


typedef enum message_type 
{ 
    conn, 
    release_ball, 
    send_ball, 
    move_ball, 
    disconnect 
} message_type;


typedef struct message
{
    message_type type;
    ball_position_type ball_pos;
} message;
