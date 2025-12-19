#define TB_IMPL
#include "termbox2_win.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>


/*** data ***/

typedef uint64_t U64;
typedef U64 bitboard;

char *renderPiece[7] = {" ♟ ", " ♞ ", " ♝ ", " ♜ ", " ♛ ", " ♚ ", "   "};
int renderColor[2] = {TB_WHITE,TB_BLACK};

enum{
    a1 = 0, b1, c1, d1, e1, f1, g1, h1,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a8, b8, c8, d8, e8, f8, g8, h8,   
};

enum{
    P = 0,
    N,
    B,
    R,
    Q,
    K,
    EMPTY
};

enum{
    WHITE = 0,
    BLACK,
    BOTH
};

typedef struct
{
    int inputMode; // 0--cursor with keys 1--commands with moves 2--cursor with mouse 

    bitboard pieces[6][2];
    int turn;
    bitboard occupancy[3];
    bitboard between[64][64];
    bitboard mask[64];

    int cursor_rank;
    int cursor_file;

    int selected_rank;
    int selected_file;

}board;

void initMasks(board *game){
    for(int i  = 0; i < 64; i++)
        game->mask[i] = 1ULL << i;


    int f1,f2,r1,r2;
    for(int i = 0; i < 64; i++)
        for( int j = 0; j < 64; j++){
            if( i == j)
                continue;
            
 
            f1 = i%8;
            f2 = j%8;
            r1 = i/8;
            r2 = j/8;
            
            if(f1 == f2 || r1 == r2 || abs(f1 - f2) == abs(r1 - r2)){ //if ther are on the same line
            
                int dx = (f2 > f1) ? 1: ((f2 < f1) ? -1 : 0);
                int dy = (r2 > r1) ? 1 : ((r2 < r1) ? -1 : 0);

                int step = (dy * 8) + dx;

                int current = i + step;
                while(current  != j){
                    game->between[i][j] |= (1ULL <<current);
                    current += step;
                }
            }
        }
        
    }

    void initPieces(board *game)
    {
        // RANK 2 (White Pawns): Bits 8-15 -> 0xFF00
        // RANK 7 (Black Pawns): Bits 48-55 -> 0x00FF000000000000

        game->pieces[P][WHITE] = 0xFF00ULL;
        game->pieces[P][BLACK] = 0x00FF000000000000ULL;

        // KNIGHTS (B1, G1 | B8, G8)
        game->pieces[N][WHITE] = 0x42ULL;
        game->pieces[N][BLACK] = 0x4200000000000000ULL;

        // ROOKS (A1, H1 | A8, H8)
        game->pieces[R][WHITE] = 0x81ULL;
        game->pieces[R][BLACK] = 0x8100000000000000ULL;

        // BISHOPS (C1, F1 | C8, F8)
        game->pieces[B][WHITE] = 0x24ULL;
        game->pieces[B][BLACK] = 0x2400000000000000ULL;

        // QUEENS (D1 | D8)
        game->pieces[Q][WHITE] = 0x08ULL;
        game->pieces[Q][BLACK] = 0x0800000000000000ULL;

        // KINGS (E1 | E8)
        game->pieces[K][WHITE] = 0x10ULL;
        game->pieces[K][BLACK] = 0x1000000000000000ULL;

        // Update Occupancies
        game->occupancy[WHITE] = 0xFFFFULL;
        game->occupancy[BLACK] = 0xFFFF000000000000ULL;
        game->occupancy[BOTH] = game->occupancy[WHITE] | game->occupancy[BLACK];
    }

void initCursor(board *game){
    game->cursor_file = 0;
    game->cursor_rank = 0;
    game->selected_file = -1;
    game->selected_rank = 0;
}

void initBoard(board *game){
    initMasks(game);
    initPieces(game);
    initCursor(game);
}

typedef struct
{
    char *chars;
    int len;

} string;

void initString(string *s){

    s->len = 0;
    s->chars = NULL;
}

void appendString(string *s, char *apnd, int len){

    char *buff;
    buff = realloc(s->chars, s->len +len);

    if( buff == NULL)
        return;

    memcpy(&buff[s->len],apnd,len);
    s->chars = buff;
    s->len +=len;

    s->chars[s->len] = '\0';

}

void appendChar(string *s, char apnd)
{

    char *buff;
    buff = realloc(s->chars, s->len + 1 + 1);
    if (buff == NULL)
        return;

    s->chars = buff;
    s->chars[s->len] = apnd;
    (s->len)++;
    (s->chars)[s->len] = '\0';

}

void removeChar(string *s)
{
    if(s->len > 0){
        s->len--;
        (s->chars)[s->len] = '\0';
    }
}

void freeString(string *s){
    if(s->chars != NULL)
        free(s->chars);
}

/*** input ***/

enum{
    up = 0,
    down,
    left,
    right
};

int outOfBounds(int x, int y)
{
    return !((x >= 0) && (x <= 7) && (y >= 0) && (y <= 7));
}

int moveCursor(board *game, int dir){
    int dx[4] = {0,0,-1,1};
    int dy[4] = {1,-1,0,0};
    if(!outOfBounds(game->cursor_file + dx[dir],game->cursor_rank + dy[dir])){
        game->cursor_file += dx[dir];
        game->cursor_rank += dy[dir];
    }
    return 0;
}

int handleInput(board * game, string * s)
{
    struct tb_event ev;

    // Wait for input
    tb_poll_event(&ev);

    if (ev.type == TB_EVENT_KEY)
    {

        // 1. Handle Special Keys (Non-printable)
        if (ev.key == TB_KEY_ARROW_UP)
        {
            moveCursor(game,up);
            return 1;
        }
        if (ev.key == TB_KEY_ARROW_DOWN)
        {
            moveCursor(game, down);
            return 1;
        }
        if (ev.key == TB_KEY_ARROW_LEFT)
        {
            moveCursor(game, left);
            return 1;
        }
        if (ev.key == TB_KEY_ARROW_RIGHT)
        {
            moveCursor(game, right);
            return 1;
        }
        else if (ev.key == TB_KEY_ENTER)
        {
            // confirm_selection();
            return 1;
        }
        else if (ev.key == TB_KEY_BACKSPACE || ev.key == TB_KEY_BACKSPACE2)
        {
            removeChar(s);
            return 1;
        }

        // 2. Handle Ctrl Combinations
        else if (ev.key == TB_KEY_CTRL_Q)
        {
            return 0;
        }

        // 3. Handle Regular Typing (a, b, c, 1, 2, 3)
        else if (ev.ch != 0)
        {
            appendChar(s,(char)ev.ch);
            return 1;
        }

        return 1;
    }

    // 4. Handle Resize (Important for responsiveness)
    else if (ev.type == TB_EVENT_RESIZE)
    {
        // re_calculate_layout(ev.w, ev.h);
        return 1;
    }

    return 1;
}


/*** output ***/

void drawBoard(board *game){
    int x_ofset = (tb_width() - 24 )/2;
    int y_ofset = 5;
    int i,j,k;

    int file,rank,draw_x,draw_y;
    
    for ( i = 0; i < 64; i++)
    {   
        file = i%8; // (a-h)
        rank = i/8; // (1-8)

        draw_x = x_ofset + (file*3);
        draw_y = y_ofset + (7-rank);

        int bg_colour = ((file+rank)%2 == 0) ? TB_CYAN : TB_BLUE;
        int fr_colour = TB_DEFAULT;
        int flag = 0;
        char *symbol = renderPiece[EMPTY];

        for( k = WHITE; k<= BLACK; k++)
            for( j = P; j <= K; j++)
                if(game->pieces[j][k] & game->mask[i]){
                    fr_colour = (k == WHITE) ? TB_WHITE | TB_BOLD : TB_BLACK | TB_BOLD;
                    symbol = renderPiece[j];
                }

        if(file == game->selected_file && rank == game->selected_rank){
            bg_colour = TB_GREEN;
        }

        tb_printf(draw_x, draw_y, fr_colour, bg_colour, "%s", symbol);

        if(file == game->cursor_file && rank == game->cursor_rank){
            tb_printf(draw_x , draw_y, TB_GREEN, bg_colour, "[");
            tb_printf(draw_x + 2, draw_y, TB_GREEN, bg_colour, "]");
        }

    }
}

int main(){

    struct tb_event *ev;

    int toContinue = 1;

    string s;
    initString(&s);
  

    board game;

    tb_init();
  
    initBoard(&game);

    while(toContinue){
        
        tb_clear();
        if(s.chars != NULL)
            tb_printf(4, tb_height()- 4, TB_WHITE | TB_REVERSE, TB_BLACK, "%s", s.chars);
        drawBoard(&game);
        tb_present();
        toContinue = handleInput(&game,&s);
    }

    tb_shutdown();
    freeString(&s);
    return 0;

}