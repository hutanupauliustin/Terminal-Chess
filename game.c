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

/***prototypes ***/
int outOfBounds(int x, int y);
int isAttacked(int x, int y, int enemyColor);

	enum chessPieces {
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

enum piecesColor
{
	WHITE = 0,
	BLACK
};

int pieceColor(int piece)
{
	
	if (piece >= WHITE_PAWN && piece <= WHITE_KING)
	return WHITE;
	
	if (piece >= BLACK_PAWN && piece <= BLACK_KING)
	return BLACK;
	
	return -1;
}

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
	
	typedef struct color
	{
		int r;
		int g;
		int b;
	} color;
	
	color RGBtoColor(int r, int g, int b)
	{
		color buff;
		buff.r = r;
		buff.g = g;
		buff.b = b;
		
		return buff;
	}
	
	typedef struct configColors
	{
		color darkSquare;
		color lightSquare;
		color darkSelectedSquare;
		color lightSelectedSquare;
		color darkAvailableSquare;
		color lightAvailableSquare;
		color darkTakeablePieceSquare;
		color lightTakeablePieceSquare;
		
	} configColors;
	
	typedef struct renderRow
	{
		char *chars;
	} renderRow;
	
	typedef struct config
	{
		
		int board[X_SIZE][Y_SIZE];
		int pieceMoveboard[X_SIZE][Y_SIZE];
		int isCheckBoad[X_SIZE][Y_SIZE];

		int whoseTurn;
		renderRow *row;
		int screenrows;
		int screencols;
		int cursorx;
		int cursory;
		int selectx;
		int selecty;
		configColors theme;
		
		struct termios orig_termios;
	} config;
	
	config game;
	
	/*** terminal ***/
	
	void die(const char *s)
	{		
		perror(s);
		exit(1);
	}
	
	void disableRawMode()
	{
		write(STDOUT_FILENO, "\033[?1049l", 9);
		
		if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &game.orig_termios) == -1)
		die("tcsetattr");
	}
	
	void enableRawMode()
	{
		if (tcgetattr(STDIN_FILENO, &game.orig_termios) == -1)
		die("tcgetattr");
		atexit(disableRawMode);
		
		struct termios raw = game.orig_termios;
		
		raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
		raw.c_oflag &= ~(OPOST);
		raw.c_cflag |= (CS8);
		raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
		raw.c_cc[VMIN] = 0;
		raw.c_cc[VTIME] = 1;
		
		if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
		die("tcsetattr");
		
		write(STDOUT_FILENO, "\033[?1049h", 9);
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
	
	/***game ***/
	
	int touchingKing(int targetX, int targetY)
	{
		for (int i = -1; i <= 1; i++)
		{
			for (int j = -1; j <= 1; j++)
			{
				if (i == 0 && j == 0)
				continue; 
				
				int checkX = targetX + i;
				int checkY = targetY + j;
				
				if (!outOfBounds(checkX, checkY))
				{
					int piece = game.board[checkX][checkY];
					
					if (piece == WHITE_KING || piece == BLACK_KING)
					{
						if (checkX == game.selectx && checkY == game.selecty)
						{
							continue;
						}
						return 1; // Found an enemy king
					}
				}
			}
		}
		return 0;
	}
	
	void LoadselectedPieceMoveBoard()
	{
		for (int i = 0; i < X_SIZE; i++)
		for (int j = 0; j < Y_SIZE; j++)
		game.pieceMoveboard[i][j] = 0;
		
		if (game.selectx == -1 || game.selecty == -1)
		return;
		
		switch (game.board[game.selectx][game.selecty])
		{
			case EMPTYSPACE:
			{
				break;
			}
			case WHITE_PAWN:
			{
				if (game.selecty == 0)
				{
					game.pieceMoveboard[game.selectx - 1][game.selecty] = pieceColor(game.board[game.selectx - 1][game.selecty]) == WHITE ? 0 : (game.board[game.selectx - 1][game.selecty] == EMPTYSPACE ? 1 : 0);
					game.pieceMoveboard[game.selectx - 1][game.selecty + 1] = pieceColor(game.board[game.selectx-1][game.selecty+1]) == WHITE ? 0 :(game.board[game.selectx - 1][game.selecty + 1] == EMPTYSPACE ? 0 : 2);
				}
				else if (game.selecty == 7)
				{
					game.pieceMoveboard[game.selectx - 1][game.selecty - 1] = pieceColor(game.board[game.selectx-1][game.selecty-1]) == WHITE ? 0 :(game.board[game.selectx - 1][game.selecty - 1] == EMPTYSPACE ? 0 : 2);
					game.pieceMoveboard[game.selectx - 1][game.selecty] = pieceColor(game.board[game.selectx - 1][game.selecty]) == WHITE ? 0 : (game.board[game.selectx - 1][game.selecty] == EMPTYSPACE ? 1 : 0);
				}
				else
				{
					game.pieceMoveboard[game.selectx - 1][game.selecty - 1] = pieceColor(game.board[game.selectx-1][game.selecty-1]) == WHITE ? 0 :(game.board[game.selectx - 1][game.selecty - 1] == EMPTYSPACE ? 0 : 2);
					game.pieceMoveboard[game.selectx - 1][game.selecty] = pieceColor(game.board[game.selectx - 1][game.selecty]) == WHITE ? 0 : (game.board[game.selectx - 1][game.selecty] == EMPTYSPACE ? 1 : 0);
					game.pieceMoveboard[game.selectx - 1][game.selecty + 1] = pieceColor(game.board[game.selectx-1][game.selecty+1]) == WHITE ? 0 :(game.board[game.selectx - 1][game.selecty + 1] == EMPTYSPACE ? 0 : 2);
				}
				break;
			}
			case BLACK_PAWN:
			{
				if (game.selecty == 0)
				{
					game.pieceMoveboard[game.selectx + 1][game.selecty] = pieceColor(game.board[game.selectx + 1][game.selecty]) == BLACK ? 0 : (game.board[game.selectx + 1][game.selecty] == EMPTYSPACE ? 1 : 0);
					game.pieceMoveboard[game.selectx + 1][game.selecty + 1] = pieceColor(game.board[game.selectx + 1][game.selecty + 1]) == BLACK ? 0 : (game.board[game.selectx + 1][game.selecty + 1] == EMPTYSPACE ? 0 : 2);
				}
				else if (game.selecty == 7)
				{
					game.pieceMoveboard[game.selectx + 1][game.selecty - 1] = pieceColor(game.board[game.selectx + 1][game.selecty - 1]) == BLACK ? 0 : (game.board[game.selectx + 1][game.selecty - 1] == EMPTYSPACE ? 0 : 2);
					game.pieceMoveboard[game.selectx + 1][game.selecty] = pieceColor(game.board[game.selectx + 1][game.selecty]) == BLACK ? 0 : (game.board[game.selectx + 1][game.selecty] == EMPTYSPACE ? 1 : 0);
				}
				else
				{
					game.pieceMoveboard[game.selectx + 1][game.selecty - 1] = pieceColor(game.board[game.selectx + 1][game.selecty - 1]) == BLACK ? 0 : (game.board[game.selectx + 1][game.selecty - 1] == EMPTYSPACE ? 0 : 2);
					game.pieceMoveboard[game.selectx + 1][game.selecty] = pieceColor(game.board[game.selectx + 1][game.selecty]) == BLACK ? 0 : (game.board[game.selectx + 1][game.selecty] == EMPTYSPACE ? 1 : 0);
					game.pieceMoveboard[game.selectx + 1][game.selecty + 1] = pieceColor(game.board[game.selectx + 1][game.selecty + 1]) == BLACK ? 0 : (game.board[game.selectx + 1][game.selecty + 1] == EMPTYSPACE ? 0 : 2);
				}
				break;
			}
			case WHITE_KNIGHT:
			{
				int x = game.selectx;
				int y = game.selecty;
				
				if (!outOfBounds(x - 1, y + 2))
				game.pieceMoveboard[x - 1][y + 2] = (pieceColor(game.board[x - 1][y + 2]) == WHITE) ? 0 : pieceColor(game.board[x - 1][y + 2]) == BLACK ? 2
				: 1;
				if (!outOfBounds(x - 2, y + 1))
				game.pieceMoveboard[x - 2][y + 1] = (pieceColor(game.board[x - 2][y + 1]) == WHITE) ? 0 : pieceColor(game.board[x - 2][y + 1]) == BLACK ? 2
				: 1;
				if (!outOfBounds(x - 2, y - 1))
				game.pieceMoveboard[x - 2][y - 1] = (pieceColor(game.board[x - 2][y - 1]) == WHITE) ? 0 : pieceColor(game.board[x - 2][y - 1]) == BLACK ? 2
				: 1;
				if (!outOfBounds(x - 1, y - 2))
				game.pieceMoveboard[x - 1][y - 2] = (pieceColor(game.board[x - 1][y - 2]) == WHITE) ? 0 : pieceColor(game.board[x - 1][y - 2]) == BLACK ? 2
				: 1;
				if (!outOfBounds(x + 1, y - 2))
				game.pieceMoveboard[x + 1][y - 2] = (pieceColor(game.board[x + 1][y - 2]) == WHITE) ? 0 : pieceColor(game.board[x + 1][y - 2]) == BLACK ? 2
				: 1;
				if (!outOfBounds(x + 2, y - 1))
				game.pieceMoveboard[x + 2][y - 1] = (pieceColor(game.board[x + 2][y - 1]) == WHITE) ? 0 : pieceColor(game.board[x + 2][y - 1]) == BLACK ? 2
				: 1;
				if (!outOfBounds(x + 2, y + 1))
				game.pieceMoveboard[x + 2][y + 1] = (pieceColor(game.board[x + 2][y + 1]) == WHITE) ? 0 : pieceColor(game.board[x + 2][y + 1]) == BLACK ? 2
				: 1;
				if (!outOfBounds(x + 1, y + 2))
				game.pieceMoveboard[x + 1][y + 2] = (pieceColor(game.board[x + 1][y + 2]) == WHITE) ? 0 : pieceColor(game.board[x + 1][y + 2]) == BLACK ? 2
				: 1;
				break;
			}
			case BLACK_KNIGHT:
			{
				int x = game.selectx;
				int y = game.selecty;
				
				if (!outOfBounds(x - 1, y + 2))
				game.pieceMoveboard[x - 1][y + 2] = (pieceColor(game.board[x - 1][y + 2]) == BLACK) ? 0 : pieceColor(game.board[x - 1][y + 2]) == WHITE ? 2
				: 1;
				if (!outOfBounds(x - 2, y + 1))
				game.pieceMoveboard[x - 2][y + 1] = (pieceColor(game.board[x - 2][y + 1]) == BLACK) ? 0 : pieceColor(game.board[x - 2][y + 1]) == WHITE ? 2
				: 1;
				if (!outOfBounds(x - 2, y - 1))
				game.pieceMoveboard[x - 2][y - 1] = (pieceColor(game.board[x - 2][y - 1]) == BLACK) ? 0 : pieceColor(game.board[x - 2][y - 1]) == WHITE ? 2
				: 1;
				if (!outOfBounds(x - 1, y - 2))
				game.pieceMoveboard[x - 1][y - 2] = (pieceColor(game.board[x - 1][y - 2]) == BLACK) ? 0 : pieceColor(game.board[x - 1][y - 2]) == WHITE ? 2
				: 1;
				if (!outOfBounds(x + 1, y - 2))
				game.pieceMoveboard[x + 1][y - 2] = (pieceColor(game.board[x + 1][y - 2]) == BLACK) ? 0 : pieceColor(game.board[x + 1][y - 2]) == WHITE ? 2
				: 1;
				if (!outOfBounds(x + 2, y - 1))
				game.pieceMoveboard[x + 2][y - 1] = (pieceColor(game.board[x + 2][y - 1]) == BLACK) ? 0 : pieceColor(game.board[x + 2][y - 1]) == WHITE ? 2
				: 1;
				if (!outOfBounds(x + 2, y + 1))
				game.pieceMoveboard[x + 2][y + 1] = (pieceColor(game.board[x + 2][y + 1]) == BLACK) ? 0 : pieceColor(game.board[x + 2][y + 1]) == WHITE ? 2
				: 1;
				if (!outOfBounds(x + 1, y + 2))
				game.pieceMoveboard[x + 1][y + 2] = (pieceColor(game.board[x + 1][y + 2]) == BLACK) ? 0 : pieceColor(game.board[x + 1][y + 2]) == WHITE ? 2
				: 1;
				break;
			}
			case WHITE_BISHOP:
			{
				int i, j;
				for (i = game.selectx, j = game.selecty;; i--, j++)
				{
					if (outOfBounds(i, j))
					break;
					else if (pieceColor(game.board[i][j]) == BLACK)
					{
						game.pieceMoveboard[i][j] = 2;
						break;
					}
					else if (pieceColor(game.board[i][j]) == WHITE && i != game.selectx && j != game.selecty)
					break;
					else
					game.pieceMoveboard[i][j] = 1;
				}
				
				for (i = game.selectx, j = game.selecty;; i--, j--)
				{
					if (outOfBounds(i, j))
					break;
					else if (pieceColor(game.board[i][j]) == BLACK)
					{
						game.pieceMoveboard[i][j] = 2;
						break;
					}
					else if (pieceColor(game.board[i][j]) == WHITE && i != game.selectx && j != game.selecty)
					break;
					else
					game.pieceMoveboard[i][j] = 1;
				}
				
				for (i = game.selectx, j = game.selecty;; i++, j--)
				{
					if (outOfBounds(j, j))
					break;
					else if (pieceColor(game.board[i][j]) == BLACK)
					{
						game.pieceMoveboard[i][j] = 2;
						break;
					}
					else if (pieceColor(game.board[i][j]) == WHITE && i != game.selectx && j != game.selecty)
					break;
					else
					game.pieceMoveboard[i][j] = 1;
				}
				
				for (i = game.selectx, j = game.selecty;; i++, j++)
				{
					if (outOfBounds(j, j))
					break;
					else if (pieceColor(game.board[i][j]) == BLACK)
					{
						game.pieceMoveboard[i][j] = 2;
						break;
					}
					else if (pieceColor(game.board[i][j]) == WHITE && i != game.selectx && j != game.selecty)
					break;
					else
					game.pieceMoveboard[i][j] = 1;
				}
				break;
			}
			case BLACK_BISHOP:
			{
				int i, j;
				for (i = game.selectx, j = game.selecty;; i--, j++)
				{
					if (outOfBounds(i, j))
					break;
					else if (pieceColor(game.board[i][j]) == WHITE)
					{
						game.pieceMoveboard[i][j] = 2;
						break;
					}
					else if (pieceColor(game.board[i][j]) == BLACK && i != game.selectx && j != game.selecty)
					break;
					else
					game.pieceMoveboard[i][j] = 1;
				}
				
				for (i = game.selectx, j = game.selecty;; i--, j--)
				{
					if (outOfBounds(i, j))
					break;
					else if (pieceColor(game.board[i][j]) == WHITE)
					{
						game.pieceMoveboard[i][j] = 2;
						break;
					}
					else if (pieceColor(game.board[i][j]) == BLACK && i != game.selectx && j != game.selecty)
					break;
					else
					game.pieceMoveboard[i][j] = 1;
				}
				
				for (i = game.selectx, j = game.selecty;; i++, j--)
				{
					if (outOfBounds(j, j))
					break;
					else if (pieceColor(game.board[i][j]) == WHITE)
					{
						game.pieceMoveboard[i][j] = 2;
						break;
					}
					else if (pieceColor(game.board[i][j]) == BLACK && i != game.selectx && j != game.selecty)
					break;
					else
					game.pieceMoveboard[i][j] = 1;
				}
				
				for (i = game.selectx, j = game.selecty;; i++, j++)
				
				{
					if (outOfBounds(j, j))
					break;
					else if (pieceColor(game.board[i][j]) == WHITE)
					{
						game.pieceMoveboard[i][j] = 2;
						break;
					}
					else if (pieceColor(game.board[i][j]) == BLACK && i != game.selectx && j != game.selecty)
					break;
					else
					game.pieceMoveboard[i][j] = 1;
				}
				break;
			}
			case WHITE_ROOK:
			{
				for(int i = game.selectx; ;i++){
					if(outOfBounds(i,game.selecty))
					break;
					else if(pieceColor(game.board[i][game.selecty]) == BLACK ){
						game.pieceMoveboard[i][game.selecty] = 2;
						break;
					}
					else if (pieceColor(game.board[i][game.selecty]) == WHITE && i != game.selectx)
					break;
					else
					game.pieceMoveboard[i][game.selecty] = 1;
				}
				
				for (int i = game.selectx;; i--)
				{
					if (outOfBounds(i, game.selecty))
					break;
					else if (pieceColor(game.board[i][game.selecty]) == BLACK)
					{
						game.pieceMoveboard[i][game.selecty] = 2;
						break;
					}
					else if (pieceColor(game.board[i][game.selecty]) == WHITE && i != game.selectx)
					break;
					else
					game.pieceMoveboard[i][game.selecty] = 1;
				}
				
				for (int j = game.selecty;; j--)
				{
					if (outOfBounds(j, game.selecty))
					break;
					else if (pieceColor(game.board[game.selectx][j]) == BLACK)
					{
						game.pieceMoveboard[game.selectx][j] = 2;
						break;
					}
					else if (pieceColor(game.board[game.selectx][j]) == WHITE && j != game.selecty)
					break;
					else
					game.pieceMoveboard[game.selectx][j] = 1;
				}
				
				for (int j = game.selecty;; j++)
				{
					if (outOfBounds(j, game.selecty))
					break;
					else if (pieceColor(game.board[game.selectx][j]) == BLACK)
					{
						game.pieceMoveboard[game.selectx][j] = 2;
						break;
					}
					else if (pieceColor(game.board[game.selectx][j]) == WHITE && j != game.selecty)
					break;
					else 
					game.pieceMoveboard[game.selectx][j] = 1;
				}
				break;
			}
			case BLACK_ROOK:
			{
				for (int i = game.selectx;; i++)
				{
					if (outOfBounds(i, game.selecty))
					break;
					else if (pieceColor(game.board[i][game.selecty]) == WHITE)
					{
						game.pieceMoveboard[i][game.selecty] = 2;
						break;
					}
					else if (pieceColor(game.board[i][game.selecty]) == BLACK && i != game.selectx)
					break;
					else
					game.pieceMoveboard[i][game.selecty] = 1;
				}
				
				for (int i = game.selectx;; i--)
				{
					if (outOfBounds(i, game.selecty))
					break;
					else if (pieceColor(game.board[i][game.selecty]) == WHITE)
					{
						game.pieceMoveboard[i][game.selecty] = 2;
						break;
					}
					else if (pieceColor(game.board[i][game.selecty]) == BLACK && i != game.selectx)
					break;
					else
					game.pieceMoveboard[i][game.selecty] = 1;
				}
				
				for (int j = game.selecty;; j--)
				{
					if (outOfBounds(j, game.selecty))
					break;
					else if (pieceColor(game.board[game.selectx][j]) == WHITE)
					{
						game.pieceMoveboard[game.selectx][j] = 2;
						break;
					}
					else if (pieceColor(game.board[game.selectx][j]) == BLACK && j != game.selecty)
					break;
					else
					game.pieceMoveboard[game.selectx][j] = 1;
				}
				
				for (int j = game.selecty;; j++)
				{
					if (outOfBounds(j, game.selecty))
					break;
					else if (pieceColor(game.board[game.selectx][j]) == WHITE)
					{
						game.pieceMoveboard[game.selectx][j] = 2;
						break;
					}
					else if (pieceColor(game.board[game.selectx][j]) == BLACK && j != game.selecty)
					break;
					else
					game.pieceMoveboard[game.selectx][j] = 1;
				}
				break;
			}
			case WHITE_QUEEN:
			{
				for (int i = game.selectx;; i++)
				{
					if (outOfBounds(i, game.selecty))
					break;
					else if (pieceColor(game.board[i][game.selecty]) == BLACK)
					{
						game.pieceMoveboard[i][game.selecty] = 2;
						break;
					}
					else if (pieceColor(game.board[i][game.selecty]) == WHITE && i != game.selectx)
					break;
					else
					game.pieceMoveboard[i][game.selecty] = 1;
				}
				
				for (int i = game.selectx;; i--)
				{
					if (outOfBounds(i, game.selecty))
					break;
					else if (pieceColor(game.board[i][game.selecty]) == BLACK)
					{
						game.pieceMoveboard[i][game.selecty] = 2;
						break;
					}
					else if (pieceColor(game.board[i][game.selecty]) == WHITE && i != game.selectx)
					break;
					else
					game.pieceMoveboard[i][game.selecty] = 1;
				}
				
				for (int j = game.selecty;; j--)
				{
					if (outOfBounds(j, game.selecty))
					break;
					else if (pieceColor(game.board[game.selectx][j]) == BLACK)
					{
						game.pieceMoveboard[game.selectx][j] = 2;
						break;
					}
					else if (pieceColor(game.board[game.selectx][j]) == WHITE && j != game.selecty)
					break;
					else
					game.pieceMoveboard[game.selectx][j] = 1;
				}
				
				for (int j = game.selecty;; j++)
				{
					if (outOfBounds(j, game.selecty))
					break;
					else if (pieceColor(game.board[game.selectx][j]) == BLACK)
					{
						game.pieceMoveboard[game.selectx][j] = 2;
						break;
					}
					else if (pieceColor(game.board[game.selectx][j]) == WHITE && j != game.selecty)
					break;
					else
					game.pieceMoveboard[game.selectx][j] = 1;
				}
				
				int i, j;
				for (i = game.selectx, j = game.selecty;; i--, j++)
				{
					if (outOfBounds(i, j))
					break;
					else if (pieceColor(game.board[i][j]) == BLACK)
					{
						game.pieceMoveboard[i][j] = 2;
						break;
					}
					else if (pieceColor(game.board[i][j]) == WHITE && i != game.selectx && j != game.selecty)
					break;
					else
					game.pieceMoveboard[i][j] = 1;
				}
				
				for (i = game.selectx, j = game.selecty;; i--, j--)
				{
					if (outOfBounds(i, j))
					break;
					else if (pieceColor(game.board[i][j]) == BLACK)
					{
						game.pieceMoveboard[i][j] = 2;
						break;
					}
					else if (pieceColor(game.board[i][j]) == WHITE && i != game.selectx && j != game.selecty)
					break;
					else
					game.pieceMoveboard[i][j] = 1;
				}
				
				for (i = game.selectx, j = game.selecty;; i++, j--)
				{
					if (outOfBounds(j, j))
					break;
					else if (pieceColor(game.board[i][j]) == BLACK)
					{
						game.pieceMoveboard[i][j] = 2;
						break;
					}
					else if (pieceColor(game.board[i][j]) == WHITE && i != game.selectx && j != game.selecty)
					break;
					else
					game.pieceMoveboard[i][j] = 1;
				}
				
				for (i = game.selectx, j = game.selecty;; i++, j++)
				{
					if (outOfBounds(j, j))
					break;
					else if (pieceColor(game.board[i][j]) == BLACK)
					{
						game.pieceMoveboard[i][j] = 2;
						break;
					}
					else if (pieceColor(game.board[i][j]) == WHITE && i != game.selectx && j != game.selecty)
					break;
					else
					game.pieceMoveboard[i][j] = 1;
				}
				break;
			}
			case BLACK_QUEEN:
			{
				for (int i = game.selectx;; i++)
				{
					if (outOfBounds(i, game.selecty))
					break;
					else if (pieceColor(game.board[i][game.selecty]) == WHITE)
					{
						game.pieceMoveboard[i][game.selecty] = 2;
						break;
					}
					else if (pieceColor(game.board[i][game.selecty]) == BLACK && i != game.selectx)
					break;
					else
					game.pieceMoveboard[i][game.selecty] = 1;
				}
				
				for (int i = game.selectx;; i--)
				{
					if (outOfBounds(i, game.selecty))
					break;
					else if (pieceColor(game.board[i][game.selecty]) == WHITE)
					{
						game.pieceMoveboard[i][game.selecty] = 2;
						break;
					}
					else if (pieceColor(game.board[i][game.selecty]) == BLACK && i != game.selectx)
					break;
					else
					game.pieceMoveboard[i][game.selecty] = 1;
				}
				
				for (int j = game.selecty;; j--)
				{
					if (outOfBounds(j, game.selecty))
					break;
					else if (pieceColor(game.board[game.selectx][j]) == WHITE)
					{
						game.pieceMoveboard[game.selectx][j] = 2;
						break;
					}
					else if (pieceColor(game.board[game.selectx][j]) == BLACK && j != game.selecty)
					break;
					else
					game.pieceMoveboard[game.selectx][j] = 1;
				}
				
				for (int j = game.selecty;; j++)
				{
					if (outOfBounds(j, game.selecty))
					break;
					else if (pieceColor(game.board[game.selectx][j]) == WHITE)
					{
						game.pieceMoveboard[game.selectx][j] = 2;
						break;
					}
					else if (pieceColor(game.board[game.selectx][j]) == BLACK && j != game.selecty)
					break;
					else
					game.pieceMoveboard[game.selectx][j] = 1;
				}
				
				int i, j;
				for (i = game.selectx, j = game.selecty;; i--, j++)
				{
					if (outOfBounds(i, j))
					break;
					else if (pieceColor(game.board[i][j]) == WHITE)
					{
						game.pieceMoveboard[i][j] = 2;
						break;
					}
					else if (pieceColor(game.board[i][j]) == BLACK && i != game.selectx && j != game.selecty)
					break;
					else
					game.pieceMoveboard[i][j] = 1;
				}
				
				for (i = game.selectx, j = game.selecty;; i--, j--)
				{
					if (outOfBounds(i, j))
					break;
					else if (pieceColor(game.board[i][j]) == WHITE)
					{
						game.pieceMoveboard[i][j] = 2;
						break;
					}
					else if (pieceColor(game.board[i][j]) == BLACK && i != game.selectx && j != game.selecty)
					break;
					else
					game.pieceMoveboard[i][j] = 1;
				}
				
				for (i = game.selectx, j = game.selecty;; i++, j--)
				{
					if (outOfBounds(j, j))
					break;
					else if (pieceColor(game.board[i][j]) == WHITE)
					{
						game.pieceMoveboard[i][j] = 2;
						break;
					}
					else if (pieceColor(game.board[i][j]) == BLACK && i != game.selectx && j != game.selecty)
					break;
					else
					game.pieceMoveboard[i][j] = 1;
				}
				
				for (i = game.selectx, j = game.selecty;; i++, j++)
				{
					if (outOfBounds(j, j))
					break;
					else if (pieceColor(game.board[i][j]) == WHITE)
					{
						game.pieceMoveboard[i][j] = 2;
						break;
					}
					else if (pieceColor(game.board[i][j]) == BLACK && i != game.selectx && j != game.selecty)
					break;
					else
					game.pieceMoveboard[i][j] = 1;
				}
				break;
			}
			case WHITE_KING:
			{
				int x = game.selectx;
				int y = game.selecty;
				
				if (!outOfBounds(x - 1, y + 1) && !touchingKing(x-1,y+1))
				game.pieceMoveboard[x - 1][y + 1] = (pieceColor(game.board[x - 1][y + 1]) == BLACK) ? 2 : pieceColor(game.board[x - 1][y + 1]) == WHITE ? 0 : 1;
				
				if (!outOfBounds(x - 1, y)&& !touchingKing(x-1,y))
				game.pieceMoveboard[x - 1][y] = (pieceColor(game.board[x - 1][y]) == BLACK) ? 2 : pieceColor(game.board[x - 1][y]) == WHITE ? 0 : 1;
				
				if (!outOfBounds(x - 1, y - 1)&& !touchingKing(x-1,y-1))
				game.pieceMoveboard[x - 1][y - 1] = (pieceColor(game.board[x - 1][y - 1]) == BLACK) ? 2 : pieceColor(game.board[x - 1][y - 1]) == WHITE ? 0 : 1;
				
				if (!outOfBounds(x, y - 1)&& !touchingKing(x,y-1))
				game.pieceMoveboard[x][y - 1] = (pieceColor(game.board[x][y - 1]) == BLACK) ? 2 : pieceColor(game.board[x][y - 1]) == WHITE ? 0 : 1;
				
				if (!outOfBounds(x, y + 1)&& !touchingKing(x,y+1))
				game.pieceMoveboard[x][y + 1] = (pieceColor(game.board[x][y + 1]) == BLACK) ? 2 : pieceColor(game.board[x][y + 1]) == WHITE ? 0 : 1;
				
				if (!outOfBounds(x+1, y + 1) && !touchingKing(x+1,y+1))
				game.pieceMoveboard[x+1][y + 1] = (pieceColor(game.board[x+1][y + 1]) == BLACK) ? 2 : pieceColor(game.board[x+1][y + 1]) == WHITE ? 0 : 1;
				
				if (!outOfBounds(x+1, y)&& !touchingKing(x+1,y))
				game.pieceMoveboard[x+1][y] = (pieceColor(game.board[x+1][y]) == BLACK) ? 2 : pieceColor(game.board[x+1][y]) == WHITE ? 0 : 1;
				
				if (!outOfBounds(x+1, y - 1)&& !touchingKing(x+1,y-1))
				game.pieceMoveboard[x+1][y - 1] = (pieceColor(game.board[x+1][y - 1]) == BLACK) ? 2 : pieceColor(game.board[x+1][y - 1]) == WHITE ? 0 : 1;	
			
				break;
			}
			case BLACK_KING:
			{
				int x = game.selectx;
				int y = game.selecty;

				if (!outOfBounds(x - 1, y + 1) && !touchingKing(x - 1, y + 1))
					game.pieceMoveboard[x - 1][y + 1] = (pieceColor(game.board[x - 1][y + 1]) == BLACK) ? 0 : pieceColor(game.board[x - 1][y + 1]) == WHITE ? 2
																																							: 1;

				if (!outOfBounds(x - 1, y) && !touchingKing(x - 1, y))
					game.pieceMoveboard[x - 1][y] = (pieceColor(game.board[x - 1][y]) == BLACK) ? 0 : pieceColor(game.board[x - 1][y]) == WHITE ? 2
																																				: 1;

				if (!outOfBounds(x - 1, y - 1) && !touchingKing(x - 1, y - 1))
					game.pieceMoveboard[x - 1][y - 1] = (pieceColor(game.board[x - 1][y - 1]) == BLACK) ? 0 : pieceColor(game.board[x - 1][y - 1]) == WHITE ? 2
																																							: 1;

				if (!outOfBounds(x, y - 1) && !touchingKing(x, y - 1))
					game.pieceMoveboard[x][y - 1] = (pieceColor(game.board[x][y - 1]) == BLACK) ? 0 : pieceColor(game.board[x][y - 1]) == WHITE ? 2
																																				: 1;

				if (!outOfBounds(x, y + 1) && !touchingKing(x, y + 1))
					game.pieceMoveboard[x][y + 1] = (pieceColor(game.board[x][y + 1]) == BLACK) ? 0 : pieceColor(game.board[x][y + 1]) == WHITE ? 2
																																				: 1;

				if (!outOfBounds(x + 1, y + 1) && !touchingKing(x + 1, y + 1))
					game.pieceMoveboard[x + 1][y + 1] = (pieceColor(game.board[x + 1][y + 1]) == BLACK) ? 0 : pieceColor(game.board[x + 1][y + 1]) == WHITE ? 2
																																							: 1;

				if (!outOfBounds(x + 1, y) && !touchingKing(x + 1, y))
					game.pieceMoveboard[x + 1][y] = (pieceColor(game.board[x + 1][y]) == BLACK) ? 0 : pieceColor(game.board[x + 1][y]) == WHITE ? 2
																																				: 1;

				if (!outOfBounds(x + 1, y - 1) && !touchingKing(x + 1, y - 1))
					game.pieceMoveboard[x + 1][y - 1] = (pieceColor(game.board[x + 1][y - 1]) == BLACK) ? 0 : pieceColor(game.board[x + 1][y - 1]) == WHITE ? 2: 1;
			
				break;
			}
		}
	}
	
	void deselectPiece()
	{
		game.selectx = -1;
		game.selecty = -1;
	}
	
	int selectPiece()
	{
			game.selectx = game.cursorx;
			game.selecty = game.cursory;
			LoadselectedPieceMoveBoard();
			return 1;
	}

int movePiece(){
	
	if(game.board[game.cursorx][game.cursorx] == WHITE_KING || game.board[game.cursorx][game.cursory] == BLACK_KING){
		if (game.pieceMoveboard[game.cursorx][game.cursory] == 1 && !isAttacked(game.cursorx,game.cursory,!pieceColor(game.board[game.cursorx][game.cursory])))
		{ // can the piece move there, and would it not be check
			game.board[game.cursorx][game.cursory] = game.board[game.selectx][game.selecty];
			game.board[game.selectx][game.selecty] = EMPTYSPACE;
			deselectPiece();
			return 1;
		}
		
		if (game.pieceMoveboard[game.cursorx][game.cursory] == 2 && !isAttacked(game.cursorx, game.cursory, !pieceColor(game.board[game.cursorx][game.cursory])))
		{ // can the piece take what's there?
			game.board[game.cursorx][game.cursory] = game.board[game.selectx][game.selecty];
			game.board[game.selectx][game.selecty] = EMPTYSPACE;
			deselectPiece();
			return 1;
		}
	}
	else 
	{
		if(game.pieceMoveboard[game.cursorx][game.cursory] == 1){ //can the piece move there?
			game.board[game.cursorx][game.cursory] = game.board[game.selectx][game.selecty];
			game.board[game.selectx][game.selecty] = EMPTYSPACE;
			deselectPiece();
			return 1;
		}
		
		if (game.pieceMoveboard[game.cursorx][game.cursory] == 2){ // can the piece take what's there?
			game.board[game.cursorx][game.cursory] = game.board[game.selectx][game.selecty];
			game.board[game.selectx][game.selecty] = EMPTYSPACE;
			deselectPiece();
			return 1;
		}
		return 0;
	}
	return 0;
}

	// Checks if square (x, y) is attacked by a sliding piece (Rook, Bishop, Queen)//made by AI :(
	// to change into an attack map and try to implement the attack map into the pieceMoveboardmap
	// make all squares where a king cannot move appear with a specific colour 
	//give the move command an output when the king cannot move to somewhere because
	//it would result un a check
	//add a message line under the chess board
	//maybe try to clean up the code to stop using so many global variables, "maybe"

	// Returns 1 if square (x, y) is being attacked by 'enemyColor'
	int isAttacked(int x, int y, int enemyColor)
	{
		// ---------------------------------------------
		// 1. CHECK PAWNS (Missing in your code)
		// ---------------------------------------------
		// If the enemy is WHITE, they attack from "below" (x+1).
		// If the enemy is BLACK, they attack from "above" (x-1).
		int pawnDir = (enemyColor == WHITE) ? 1 : -1;

		// Check diagonal left
		if (!outOfBounds(x + pawnDir, y - 1))
		{
			int p = game.board[x + pawnDir][y - 1];
			if (p != EMPTYSPACE && pieceColor(p) == enemyColor)
			{
				if (p == WHITE_PAWN || p == BLACK_PAWN)
					return 1;
			}
		}
		// Check diagonal right
		if (!outOfBounds(x + pawnDir, y + 1))
		{
			int p = game.board[x + pawnDir][y + 1];
			if (p != EMPTYSPACE && pieceColor(p) == enemyColor)
			{
				if (p == WHITE_PAWN || p == BLACK_PAWN)
					return 1;
			}
		}

		// ---------------------------------------------
		// 2. CHECK KNIGHTS (Fixed variable and overwrite bugs)
		// ---------------------------------------------
		int kx[] = {-2, -1, 1, 2, 2, 1, -1, -2};
		int ky[] = {1, 2, 2, 1, -1, -2, -2, -1};

		for (int i = 0; i < 8; i++)
		{
			int tx = x + kx[i]; // Use x, not selectx
			int ty = y + ky[i]; // Use y, not selecty

			if (!outOfBounds(tx, ty))
			{
				int p = game.board[tx][ty];
				if (p != EMPTYSPACE && pieceColor(p) == enemyColor)
				{
					if (p == WHITE_KNIGHT || p == BLACK_KNIGHT)
						return 1; // Return immediately!
				}
			}
		}

		// ---------------------------------------------
		// 3. CHECK SLIDERS (Rook, Bishop, Queen)
		// ---------------------------------------------
		int dx[] = {-1, 1, 0, 0, -1, -1, 1, 1};
		int dy[] = {0, 0, -1, 1, -1, 1, -1, 1};

		for (int dir = 0; dir < 8; dir++)
		{
			for (int dist = 1; dist < 8; dist++)
			{
				int tx = x + (dx[dir] * dist);
				int ty = y + (dy[dir] * dist);

				if (outOfBounds(tx, ty))
					break;

				int piece = game.board[tx][ty];

				if (piece != EMPTYSPACE)
				{
					if (pieceColor(piece) == enemyColor)
					{
						int isRook = (piece == WHITE_ROOK || piece == BLACK_ROOK);
						int isBishop = (piece == WHITE_BISHOP || piece == BLACK_BISHOP);
						int isQueen = (piece == WHITE_QUEEN || piece == BLACK_QUEEN);
						int isKing = (piece == WHITE_KING || piece == BLACK_KING);

						// Orthogonal (0-3): Rook, Queen, or King (1 step)
						if (dir < 4)
						{
							if (isRook || isQueen)
								return 1;
							if (isKing && dist == 1)
								return 1;
						}
						// Diagonal (4-7): Bishop, Queen, or King (1 step)
						else
						{
							if (isBishop || isQueen)
								return 1;
							if (isKing && dist == 1)
								return 1;
						}
					}
					break; // Blocked by ANY piece
				}
			}
		}

		return 0; // Safe
	}
	/*** input ***/
	
	int outOfBounds(int x, int y){
		return x < 0 || y < 0 || x > X_SIZE-1 || y > Y_SIZE - 1;  
	}
	
	int moveCursorLocation ( int xnew, int ynew){
		
		if(!outOfBounds(xnew,ynew)){
			game.cursorx = xnew;
			game.cursory = ynew;
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
			exit(0);
			break;
			
			case ' ':
				if ((pieceColor((game.board[game.cursorx][game.cursory])) == game.whoseTurn) && game.selectx == -1)
					selectPiece();
				else
					deselectPiece();
			break;

			case '\r':
			if(game.selectx != -1){
				if(movePiece())
					game.whoseTurn = !game.whoseTurn;
			}
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
		buffAppend(ab, "\r\n", 2);
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
	
	void drawBoard(struct buff *ab)
	{
		char leftBorder[80];
		int leftBorderLen = (game.screencols - 30)/2;
		for( int i = 0; i < leftBorderLen; i++)
		leftBorder[i]=' ';
		leftBorder[leftBorderLen] = '\0';
		
		buffAppend(ab, leftBorder, leftBorderLen); //draw the border on the left as to center the gameboard
		
		buffAppend(ab, "\033[0m    a  b  c  d  e  f  g  h \n\r", 34);
		
		for (int i = 0; i < X_SIZE; i++)
		{
			buffAppend(ab, "\033[0m", 5); // default foreground and background
			buffAppend(ab, leftBorder,leftBorderLen);
			
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
				else if (game.selectx != -1 && game.selecty != -1 && game.pieceMoveboard[i][j] == 1)
				{
					buffAppend(ab, "\033[2m\033[25m", 10); // dim + no blinking
					if ((i + j) % 2 == 1)
					buffAppendColor(ab, 4, game.theme.darkAvailableSquare);
					else
					buffAppendColor(ab, 4, game.theme.lightAvailableSquare);
				}
				// is this a piece the selected piece can take?
				else if ((game.selectx != -1 && game.selecty != -1 )&& (game.pieceMoveboard[i][j] == 2) && (pieceColor(game.board[game.selectx][game.selecty]) != pieceColor(game.board[i][j])))
				{
					buffAppend(ab, "\033[22m\033[25m", 11); // no dim + no blinking
					if ((i + j) % 2 == 1)
					buffAppendColor(ab, 4, game.theme.darkTakeablePieceSquare);
					else
					buffAppendColor(ab, 4, game.theme.lightTakeablePieceSquare);
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
				
				char *symbol = render[game.board[i][j]];
				
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
		drawBoard(&ab);
		
		write(STDOUT_FILENO, ab.b, ab.len);
		buffFree(&ab);
	}
	
	/***init ***/
	
void initIsCheckBoard(){


}

	void initGameTheme(){
		
		game.theme.darkAvailableSquare = RGBtoColor(22, 115, 108);
		game.theme.lightAvailableSquare = RGBtoColor(36, 191, 180);
		
		game.theme.darkSelectedSquare = RGBtoColor(22, 115, 108);
		game.theme.lightSelectedSquare = RGBtoColor(36, 191, 180);
		
		game.theme.darkSquare = RGBtoColor(22,115,85);
		game.theme.lightSquare = RGBtoColor(36,191,141);
		
		game.theme.darkTakeablePieceSquare = RGBtoColor(157, 107, 57);
		game.theme.lightTakeablePieceSquare = RGBtoColor(157, 107, 57);
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

		memcpy(game.board, startState, sizeof(game.board));

		if (getWindowSize(&game.screenrows, &game.screencols) == -1)
			die("getWindowSize");
			
		game.screenrows -= 2;
			
		game.cursorx = 0;
		game.cursory = 0;
			
		game.selectx = -1;
		game.selecty = -1;
			
		game.whoseTurn = WHITE;
			
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