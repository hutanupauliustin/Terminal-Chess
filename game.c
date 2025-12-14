#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/*** defines ***/

#define Version "0.0.1"
#define X_SIZE 8
#define Y_SIZE 8


#define CTRL_KEY(k) ((k) & 0x1f)

enum chessPieces
{
	WHITE_PAWN = 0,
	WHITE_KNIGHT,
	WHITE_BISHOP,
	WHITE_ROOK,
	WHITE_QUEEN,
	WHITE_KING,
	EMPTYSPACE,
	CURSOR,
	BLACK_PAWN,
	BLACK_KNIGHT,
	BLACK_BISHOP,
	BLACK_ROOK,
	BLACK_QUEEN,
	BLACK_KING

};

char *render[14] = {"\033[37m♟\033[30m", "\033[37m♞\033[30m", "\033[37m♝\033[30m",
					"\033[37m♜\033[30m", "\033[37m♛\033[30m", "\033[37m♚\033[30m",
					" \033[30m", "",
					"\033[30m♟\033[30m", "\033[30m♞\033[30m", "\033[30m♝\033[30m",
					"\033[30m♜\033[30m", "\033[30m♛\033[30m", "\033[30m♚\033[30m"};

enum inputKey
{

	BACKSPACE = 127,
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN

};

/*** data ***/

typedef struct color{
	int r;
	int g;
	int b;
} color;

color RGBtoColor(int r, int g, int b){
	color buff;
	buff.r = r;
	buff.g = g;
	buff.b = b;

	return buff;
}

typedef struct gameConfigColors{
	color darkSquare;
	color lightSquare;
	color darkSelectedSquare;
	color lightSelectedSquare;
	color darkAvailableSquare;
	color lightAvailableSquare;

} gameConfigColors;

typedef struct renderRow{
	char *chars;
}renderRow;

typedef struct config{

	int map[X_SIZE][Y_SIZE];
	int pieceMoveMap [X_SIZE][Y_SIZE];
	struct termios orig_termios;
	renderRow *row;
	int screenrows;
	int screencols;
	int cursorx;
	int cursory;
	int selectx;
	int selecty;
	gameConfigColors theme;
}config;

config game;

/*** terminal ***/

void die(const char *s)
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	write(STDOUT_FILENO, "\x1b[?25h", 6);

	perror(s);
	exit(1);
}

void disableRawMode()
{
	
	if(tcsetattr(STDIN_FILENO,TCSAFLUSH, &game.orig_termios) == -1)
	die("tcsetattr");
	
}

void enableRawMode()
{
	if( tcgetattr(STDIN_FILENO, &game.orig_termios) == -1)
	die("tcgetattr");
	atexit(disableRawMode);
	
	struct termios raw = game.orig_termios;
	
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	
	
	if( tcsetattr(STDIN_FILENO,TCSAFLUSH, &raw) == -1)
	die("tcsetattr");
	
}

int getCursorPosition(int *rows, int *cols)
{
	char buf[32];
	unsigned int i = 0;

	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
		return -1;

	while (i < sizeof(buf) - 1)
	{
		if (read(STDIN_FILENO, &buf[i], 1) != 1)
			break;
		if (buf[i] == 'R')
			break;
		i++;
	}

	buf[i] = '\0';
	if (buf[0] != '\x1b' || buf[1] != '[')
		return -1;
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
		return -1;

	return 0;
}

int getWindowSize(int *rows, int *cols)
{
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
	{
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
			return -1;
		return getCursorPosition(rows, cols);
	}
	else
	{
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/*** input ***/

int outOfBounds(int x, int y){
	return x < 0 || y < 0 || x > X_SIZE-1 || y > Y_SIZE;  
}

int moveCursorLocation ( int xnew, int ynew){

	if(!outOfBounds(xnew,ynew)){
		game.cursorx = xnew;
		game.cursory = ynew;
		return 1;
	}

	return 0;
}

void LoadselectedPieceMap()
{
	for (int i = 0; i < X_SIZE; i++)
		for (int j = 0; j < Y_SIZE; j++)
			game.pieceMoveMap[i][j] = 0;

	if (game.selectx == -1 || game.selecty == -1)
		return;
	
	switch (game.map[game.selectx][game.selecty])
	{

	case WHITE_PAWN:
		if (game.selecty == 0)
		{
			game.pieceMoveMap[game.selectx - 1][game.selecty] = 1;
			game.pieceMoveMap[game.selectx - 1][game.selecty + 1] = 1;
		}
		else if (game.selecty == 7)
		{
			game.pieceMoveMap[game.selectx - 1][game.selecty - 1] = 1;
			game.pieceMoveMap[game.selectx - 1][game.selecty] = 1;
		}
		else
		{
			game.pieceMoveMap[game.selectx - 1][game.selecty - 1] = 1;
			game.pieceMoveMap[game.selectx - 1][game.selecty] = 1;
			game.pieceMoveMap[game.selectx - 1][game.selecty + 1] = 1;
		}
		break;
	case BLACK_PAWN:
		if (game.selecty == 0)
		{
			game.pieceMoveMap[game.selectx + 1][game.selecty] = 1;
			game.pieceMoveMap[game.selectx + 1][game.selecty + 1] = 1;
		}
		else if (game.selecty == 7)
		{
			game.pieceMoveMap[game.selectx + 1][game.selecty - 1] = 1;
			game.pieceMoveMap[game.selectx + 1][game.selecty] = 1;
		}
		else
		{
			game.pieceMoveMap[game.selectx + 1][game.selecty - 1] = 1;
			game.pieceMoveMap[game.selectx + 1][game.selecty] = 1;
			game.pieceMoveMap[game.selectx + 1][game.selecty + 1] = 1;
		}
		break;
	}
}

void deselectPiece(){
	game.selectx = -1;
	game.selecty = -1;
}

int selectPiece()
{
	if(game.cursorx == game.selectx && game.selecty == game.cursory)
	{
		deselectPiece();
		LoadselectedPieceMap();
		return 1;
	}
	if (game.map[game.cursorx][game.cursory] != EMPTYSPACE)
	{
		game.selectx = game.cursorx;
		game.selecty = game.cursory;
		LoadselectedPieceMap();
		return 1;
	}
	return 0;
}

int CursorMove(int c)
{

	switch (c)
	{
	case ARROW_DOWN:
		moveCursorLocation(game.cursorx + 1, game.cursory);
		return 1;
		break;
	case ARROW_LEFT:
		moveCursorLocation(game.cursorx, game.cursory - 1);
		return 1;
		break;
	case ARROW_RIGHT:
		moveCursorLocation(game.cursorx, game.cursory + 1);
		return 1;
		break;
	case ARROW_UP:
		moveCursorLocation(game.cursorx - 1, game.cursory);
		return 1;
		break;
	}

	return 0;
}

int ReadKey()
{
	int nread;
	char c;
	while( (nread = read(STDIN_FILENO, &c ,1)) != 1)
	{
		if(nread == -1 && errno != EAGAIN)
		die("read");
	}
	if(c == '\x1b')
	{
		char seq[3];
		
		if(read(STDIN_FILENO, &seq[0], 1) != 1) 
		return '\x1b';
		if(read(STDIN_FILENO, &seq[1], 1) != 1) 
		return '\x1b';
		
		if(seq[0] == '[')
		{
			if(seq[1] >= '0' && seq[1] <= '9')
			{
				if(read(STDIN_FILENO, &seq[2], 1) != 1) 
				return '\x1b';
				if(seq[2] == '~')
				{
					switch(seq[1])
					{
						case '1': return HOME_KEY;
						case '3': return DEL_KEY;
						case '4': return END_KEY;
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;	
						case '7': return HOME_KEY;
						case '8': return END_KEY;
						
					}
				}
			}
			else
			{
				switch(seq[1])
				{
					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
					case 'H': return HOME_KEY;
					case 'F': return END_KEY;
					
				}
			}
		}
		else
		if(seq[0] == 'O')
		{
			switch(seq[1])
			{
				case 'H': return HOME_KEY;
				case 'F': return END_KEY;
				
			}
		}
		
		return '\x1b';	
	}
	else
	{
		return c;
	}
	
}

void ProcessKeypress()
{
    int c = ReadKey();

    switch (c)
    {

    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;

	case ' ':
		selectPiece();
		break;

	case '\x1b':
		deselectPiece();
		break;

	case 'a':
		CursorMove(ARROW_LEFT);
		break;
	case 's':
		CursorMove(ARROW_DOWN);
		break;
	case 'd':
		CursorMove(ARROW_RIGHT);
		break;
	case 'w':
		CursorMove(ARROW_UP);
		break;

	case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
        CursorMove(c);
        break;

    default:
        break;
    }
}
  
	/***render buffer ***/

struct buff{
		char *b;
		int len;
	};

#define BUF_INIT {NULL,0}

void buffAppend( struct buff *ab, const char *s, int len)
	{
		char *new = realloc(ab->b, ab -> len +len);

		if(new == NULL)
		return;
		memcpy(&new[ab->len],s,len);
		ab->b = new;
		ab->len += len;

	}
	
void buffFree(struct buff *ab){
		free(ab->b);
	}

	/***output ***/

//void drawControlLine();

void drawTitle(struct buff *ab){

	char title[80];
	int titlelen = snprintf(title, sizeof(title),"Terminal Chess");
	if(titlelen > game.screencols) titlelen = game.screencols;
	int padding = (game.screencols - titlelen) / 2;
	if (padding)
	{
		buffAppend(ab, "=", 1);
		padding--;
	}
	while (padding--)
		buffAppend(ab, "=", 1);
	buffAppend(ab, title, titlelen);
	padding = (game.screencols - titlelen) / 2;
	while (padding--)
		buffAppend(ab, "=", 1);
	buffAppend(ab, "\r\n", 2);
}

void buffAppendColor(struct buff *ab,int ground ,color input)
{ 
	//ground = 3 -- foreground
	//ground = 4 -- background

	char buffer[64]; 
	int len = snprintf(buffer, sizeof(buffer), "\033[%d8;2;%d;%d;%dm",ground, input.r, input.g, input.b);
	buffAppend(ab, buffer, len);
}

void drawMap(struct buff *ab)
{
	buffAppend(ab, "\033[0m    a  b  c  d  e  f  g  h \n\r", 34);

	for (int i = 0; i < X_SIZE; i++)
	{
		buffAppend(ab, "\033[0m", 5); // default foreground and background

		char c[4];
		c[0] = ' ';
		c[1] = i != 0 ? i + '0' : ' ';
		c[2] = ' ';
		c[3] = '\0';
		buffAppend(ab, c, 4);

		// draw the chess row numbers

		buffAppend(ab,"\033[30m",6); //black text for the pieces and brackets

		for (int j = 0; j < Y_SIZE; j++)
		{

			//	COLOR LOGIC

			// is this the selected piece?
			if (i == game.selectx && j == game.selecty)  
			{
				buffAppend(ab, "\033[22m\033[5m", 10); // not dim + blinking
				if ((i + j) % 2 == 1){
					buffAppendColor(ab, 4, game.theme.darkSelectedSquare);
				}
				else{
					buffAppendColor(ab, 4, game.theme.lightSelectedSquare);
				}

			}
			// is this a valid location for the piece to move?
			else if (game.selectx != -1 && game.selecty != -1 && game.pieceMoveMap[i][j] == 1)
			{
				buffAppend(ab, "\033[2m\033[25m", 10); // dim + no blinking
				if ((i + j) % 2 == 1)
					buffAppendColor(ab, 4, game.theme.darkAvailableSquare);
				else
					buffAppendColor(ab, 4, game.theme.lightAvailableSquare);
			}
			// standard board
			else
			{
				buffAppend(ab, "\033[22m\033[25m", 11); // no dim + no blinking

				if ((i + j) % 2 == 1)
					buffAppendColor(ab, 4, game.theme.darkSquare);
				else
					buffAppendColor(ab, 4, game.theme.lightSquare);
			}	

			// draw logic

			char *symbol = render[game.map[i][j]];

			if (i == game.cursorx && j == game.cursory)
			{
				// CURSOR: Draw Brackets + Symbol
				buffAppend(ab, "[", 1);
				buffAppend(ab, symbol, strlen(symbol));
				buffAppend(ab, "]", 1);
			}
			else
			{
				// NORMAL: Draw Space + Symbol + Space
				buffAppend(ab, " ", 1);
				buffAppend(ab, symbol, strlen(symbol));
				buffAppend(ab, " ", 1);
			}
			// chess pieces and empty spaces
		}
		buffAppend(ab, "\n\r", 3);
	}
	buffAppend(ab, "\n\r", 3);
	buffAppend(ab, "\033[0m", 5); // default foreground and background
}

void refreshGameWindow(){

	struct buff ab = BUF_INIT;

	buffAppend(&ab, "\033[2J\033[H", 8);
	buffAppend(&ab, "\x1b[?25l",6);

	drawTitle(&ab);
	drawMap(&ab);

	write(STDOUT_FILENO, ab.b, ab.len);
	buffFree(&ab);
}

/***init ***/

void initGameTheme(){

	game.theme.darkAvailableSquare = RGBtoColor(22, 115, 108);
	game.theme.lightAvailableSquare = RGBtoColor(36, 191, 180);

	game.theme.darkSelectedSquare = RGBtoColor(22, 115, 108);
	game.theme.lightSelectedSquare = RGBtoColor(36, 191, 180);

	game.theme.darkSquare = RGBtoColor(22,115,85);
	game.theme.lightSquare = RGBtoColor(36,191,141);
}

void initGame(){

	int startState[8][8] = {
		{BLACK_ROOK, BLACK_KNIGHT, BLACK_BISHOP, BLACK_QUEEN, BLACK_KING, BLACK_BISHOP, BLACK_KNIGHT, BLACK_ROOK},
		{BLACK_PAWN, BLACK_PAWN, BLACK_PAWN, BLACK_PAWN, BLACK_PAWN, BLACK_PAWN, BLACK_PAWN, BLACK_PAWN},

		{EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE},
		{EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE},
		{EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE},
		{EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE, EMPTYSPACE},

		{WHITE_PAWN, WHITE_PAWN, WHITE_PAWN, WHITE_PAWN, WHITE_PAWN, WHITE_PAWN, WHITE_PAWN, WHITE_PAWN},
		{WHITE_ROOK, WHITE_KNIGHT, WHITE_BISHOP, WHITE_QUEEN, WHITE_KING, WHITE_BISHOP, WHITE_KNIGHT, WHITE_ROOK}};

	memcpy(game.map, startState, sizeof(game.map));

	if (getWindowSize (&game.screenrows, &game.screencols) == -1)
		die("getWindowSize");
	game.screenrows -= 2;

	game.cursorx = 0;
	game.cursory = 0;

	game.selectx = -1;
	game.selecty = -1;

	initGameTheme();
}

int main(){

	enableRawMode();
	initGame();

	while(1){
		refreshGameWindow();
		ProcessKeypress();
	}
}