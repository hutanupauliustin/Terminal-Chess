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

enum{
    No = 0,
    NoWe,
    We,
    SoWe,
    So,
    SoEa,
    Ea,
    NoEa
};

typedef struct
{
    int inputMode;  // 0--cursor with keys 1--commands with moves 2--cursor with mouse 
    int outputMode; // 0--normal 
                    //1##--attackMap #(piece) #(colour) 

    bitboard pieces[6][2];
    int turn;
    bitboard occupancy[3];
    bitboard between[64][64];
    bitboard mask[64];
    bitboard rays[8][64];

    bitboard attackMap[6][2][64];
    bitboard moveMap[6][2][64];
    bitboard checkMap[2];
    bitboard enPassantPosition;

    int cursor_position;

    int selected_position;

}board;

int outOfBounds(int x, int y)
{
    return !((x >= 0) && (x <= 7) && (y >= 0) && (y <= 7));
}

void initMasks(board *game){
    for(int i  = 0; i < 64; i++)
        game->mask[i] = 1ULL << i;


    int f1,f2,r1,r2;

    for(int i = 0; i < 64; i++){
        r1 = i / 8;
        f1 = i % 8;

        for( int j = 0; j < 64; j++){
            if( i == j)
                continue;
            
            f2 = j%8;
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

        game->attackMap[P][BLACK][i] = 0ULL;
        game->attackMap[P][WHITE][i] = 0ULL;
        game->attackMap[K][BLACK][i] = 0ULL;
        game->attackMap[K][WHITE][i] = 0ULL;
        game->attackMap[N][BLACK][i] = 0ULL;
        game->attackMap[N][WHITE][i] = 0ULL;
        game->attackMap[R][BLACK][i] = 0ULL;
        game->attackMap[R][WHITE][i] = 0ULL;
        game->attackMap[B][BLACK][i] = 0ULL;
        game->attackMap[B][WHITE][i] = 0ULL;
        game->attackMap[Q][BLACK][i] = 0ULL;
        game->attackMap[Q][WHITE][i] = 0ULL;

        game->moveMap[P][BLACK][i] = 0ULL;
        game->moveMap[P][WHITE][i] = 0ULL;
        game->moveMap[K][BLACK][i] = 0ULL;
        game->moveMap[K][WHITE][i] = 0ULL;
        game->moveMap[N][BLACK][i] = 0ULL;
        game->moveMap[N][WHITE][i] = 0ULL;
        game->moveMap[R][BLACK][i] = 0ULL;
        game->moveMap[R][WHITE][i] = 0ULL;
        game->moveMap[B][BLACK][i] = 0ULL;
        game->moveMap[B][WHITE][i] = 0ULL;
        game->moveMap[Q][BLACK][i] = 0ULL;
        game->moveMap[Q][WHITE][i] = 0ULL;
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
    game->cursor_position = 0;
    game->selected_position = -1;
}

void initPawnMoveAttackMap(board *game){

for(int i = 0; i < 64; i++){
    
    game->attackMap[P][WHITE][i] = 0ULL;
    
    if(i < 56)  //overflow protection
    {

        game->moveMap[P][WHITE][i] |= 1ULL << (i + 8);

        if(i/8 == 1)
            game->moveMap[P][WHITE][i] |= 1ULL << (i + 16);


        if( i % 8 == 0) // on file a
        game->attackMap[P][WHITE][i] |= 1ULL << (i + 1 + 8);
        else if( i % 8 == 7) // on file h
        game->attackMap[P][WHITE][i] |= 1ULL << (i - 1 + 8);
        else
        game->attackMap[P][WHITE][i] |= 5ULL << (i - 1 + 8);
        
    }

    game->attackMap[P][BLACK][i] |= 0ULL;
    
    if(i> 7)  //overflow protection
    {
        game->moveMap[P][BLACK][i] |= 1ULL << (i - 8);
        if(i/8 == 6)
            game->moveMap[P][BLACK][i] |= 1ULL << (i - 16);
 
        if (i % 8 == 0) // on file a
        game->attackMap[P][BLACK][i] |= 1ULL << (i + 1 - 8);
        else if (i % 8 == 7) // on file h
        game->attackMap[P][BLACK][i] |= 1ULL << (i - 1 - 8);
        else
        game->attackMap[P][BLACK][i] |= 5ULL << (i - 1 - 8);
    }
}
}

void initKnightMoveAttackMap(board *game){

    int dx[8] = {2, 1, -1, -2, -2, -1, 1, 2};
    int dy[8] = {1, 2, 2, 1, -1, -2, -2, -1};

    for (int i = 0; i < 64; i++)
    {

        game->attackMap[N][WHITE][i] = 0ULL;
        game->attackMap[N][BLACK][i] = 0ULL;

        for(int j = 0; j < 8; j++){
            if(!outOfBounds(i % 8 + dx[j], i/8 + dy[j])){
                game->attackMap[N][WHITE][i] |= game->mask[i + dy[j]*8 + dx[j]];
                game->moveMap[N][WHITE][i] |= game->mask[i + dy[j] * 8 + dx[j]];

                game->attackMap[N][BLACK][i] |= game->attackMap[N][WHITE][i];
                game->moveMap[N][BLACK][i] |= game->moveMap[N][WHITE][i];
            }
        }
    
    }
    
}

void initKingMoveAttackMap(board *game){
        
    int dx[8] = {1, 1, -1, -1, -1, -1, 1, 1};
    int dy[8] = {1, 1, 1, 1, -1, -1, -1, -1};

    for (int i = 0; i < 64; i++)
    {

        game->attackMap[K][WHITE][i] = 0ULL;
        game->attackMap[K][BLACK][i] = 0ULL;

        for (int j = 0; j < 8; j++)
        {
            if (!outOfBounds(i % 8 + dx[j], i / 8 + dy[j]))
            {
            game->attackMap[K][WHITE][i] |= game->mask[i + dy[j] * 8 + dx[j]];
            game->moveMap[K][WHITE][i] |= game->mask[i + dy[j] * 8 + dx[j]];

            game->attackMap[K][BLACK][i] |= game->attackMap[K][WHITE][i];
            game->moveMap[K][BLACK][i] |= game->moveMap[K][WHITE][i];
            }
        }  
    }

}

void initBoard(board *game)
{
    initMasks(game);
    initPieces(game);
    initCursor(game);
    initPawnMoveAttackMap(game);
    initKnightMoveAttackMap(game);
    initKingMoveAttackMap(game);

    game->turn = WHITE;
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

/*** game ***/

int getPieceColour( board *game, int pos){
    
    for(int k = P; k <= K; k++){
        if (game->pieces[k][0] & game->mask[pos])
            return 0;
        if(game->pieces[k][1] & game->mask[pos])
            return 1;
    }
    return EMPTY;
}

int getPieceType( board *game, int pos){
    
    for (int k = P; k <= K; k++)
    {
        if ((game->pieces[k][0] | game->pieces[k][1]) & game->mask[pos])
            return k;
    }
    return EMPTY;
}

void createBishopMoveAttackMap(board *game,int pos)
{
    int dr[4] = {1, -1, -1, 1};
    int df[4] = {-1, -1, 1, 1};

    int f = pos % 8;
    int r = pos / 8;

    int colour = getPieceColour(game, pos);
    int oppcolor = (colour == BLACK ) ? WHITE : BLACK;
    
    if (colour == EMPTY) 
        return;

    game->checkMap[oppcolor] ^= game->attackMap[Q][colour][pos];
    game->attackMap[B][colour][pos] = 0ULL;
    game->moveMap[B][colour][pos] = 0ULL;    

    for (int j = 0; j < 4; j++)
    {
        int fnew = f;
        int rnew = r;

        for (int k = 1; k <= 8; k++)
        {
            fnew += df[j];
            rnew += dr[j];

            if (outOfBounds(fnew, rnew))
                break;

            if ((game->occupancy[BOTH] & game->mask[rnew*8 +fnew]) == game->mask[rnew*8 +fnew])
            {    
                game->attackMap[B][colour][pos] |= game->mask[rnew * 8 + fnew];
                game->checkMap[oppcolor] |= game->mask[rnew * 8 + fnew];
                break;
            }

            game->attackMap[B][colour][pos] |= game->mask[rnew * 8 + fnew];
            game->checkMap[oppcolor] |= game->mask[rnew * 8 + fnew];
            game->moveMap[B][colour][pos] |= game->mask[rnew * 8 + fnew];
        }
    }
}

void createRookMovAttackMap(board *game, int pos)
{
    int dr[4] = {1, 0,-1, 0};
    int df[4] = {0,-1, 0, 1};

    int f = pos % 8;
    int r = pos / 8;


    int color = getPieceColour(game, pos);
    int oppcolor = (color == BLACK ) ? WHITE : BLACK;
    if (color == EMPTY)
        return;

    game->checkMap[oppcolor] ^= game->attackMap[Q][color][pos];
    game->attackMap[R][color][pos] = 0ULL;
    game->moveMap[R][color][pos] = 0ULL;    


    for (int j = 0; j < 4; j++)
    {
        int fnew = f;
        int rnew = r;

        for (int k = 1; k <= 8; k++)
        {
            fnew += df[j];
            rnew += dr[j];

            if (outOfBounds(fnew, rnew))
                break;

            if ((game->occupancy[BOTH] & game->mask[rnew * 8 + fnew]) == game->mask[rnew * 8 + fnew])
            {
                game->attackMap[R][color][pos] |= game->mask[rnew * 8 + fnew];
                game->checkMap[oppcolor] |= game->mask[rnew * 8 + fnew];
                break;
            }

            game->attackMap[R][color][pos] |= game->mask[rnew * 8 + fnew];
            game->checkMap[oppcolor] |= game->mask[rnew * 8 + fnew];
            game->moveMap[R][color][pos] |= game->mask[rnew * 8 + fnew];
            }
    }
}

void createQueenMoveAttackMap(board *game, int pos){
    int dr[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    int df[8] = {0, -1, -1, -1, 0, 1, 1, 1};

    int f = pos % 8;
    int r = pos / 8; 
 

    int color = getPieceColour(game,pos);
    int oppcolor = (color == BLACK ) ? WHITE : BLACK;
    if (color == EMPTY)
        return;

    game->checkMap[oppcolor] ^= game->attackMap[Q][color][pos];
    game->attackMap[Q][color][pos] = 0ULL;
    game->moveMap[Q][color][pos] = 0ULL;

    for (int j = 0; j < 8; j++)
    {
        int fnew = f;
        int rnew = r;

        for (int k = 1; k <= 8; k++)
        {
            fnew += df[j];
            rnew += dr[j];

            if (outOfBounds(fnew, rnew))
                break;

            if ((game->occupancy[BOTH] & game->mask[rnew * 8 + fnew]) == game->mask[rnew * 8 + fnew])
            {
                game->attackMap[Q][color][pos] |= game->mask[rnew * 8 + fnew];
                game->checkMap[oppcolor] |= game->mask[rnew * 8 + fnew];
                break;
            }

            game->attackMap[Q][color][pos] |= game->mask[rnew * 8 + fnew];
            game->checkMap[oppcolor] |= game->mask[rnew * 8 + fnew];
            game->moveMap[Q][color][pos] |= game->mask[rnew * 8 + fnew];
        }
    }
}

void createPieceMoveAttackMap(board *game, int pos)
{
    int piece = getPieceType(game,pos);
    int color = getPieceColour(game,pos);

    switch(piece){
        case P:
            break;
        case N:
            break;
        case B:
            createBishopMoveAttackMap(game,pos);
            break;
        case R:
            createRookMovAttackMap(game,pos);
            break;
        case Q:
            createQueenMoveAttackMap(game,pos);
            break;
        case K:
            break;
    }
}

void updateOccupancy(board *game) {
    game->occupancy[WHITE] = 0ULL;
    game->occupancy[BLACK] = 0ULL;
    game->occupancy[BOTH]  = 0ULL;

    for (int p = P; p <= K; p++) {
        game->occupancy[WHITE] |= game->pieces[p][WHITE];
        game->occupancy[BLACK] |= game->pieces[p][BLACK];
    }

    game->occupancy[BOTH] = game->occupancy[WHITE] | game->occupancy[BLACK];
}

/*void updateMoveAttackMap(board *game){
    for(int i = 0; i < 64; i++){
        createPieceMoveAttackMap(game,i);
    
        game->checkMap[]
}*/

int move(board *game, int from, int where)
{ // 0-failed 1-moved 2-capture 3-en passant capture 4-castle 5-promotion
    
    int piece = getPieceType(game,from);
    int colour = getPieceColour(game,from);
    int oppositeColour = (colour == BLACK) ? WHITE : BLACK;

    if(game->turn != colour || piece == EMPTY)
        return 0;

    bitboard moveMask = game->mask[from] | game->mask[where];

    if(((game->moveMap[piece][colour][from] & game->mask[where]) != 0ULL) && ((game->occupancy[BOTH] & game->mask[where]) == 0ULL)){
       
        game->checkMap[oppositeColour] ^= game->attackMap[piece][colour][from];
        game->pieces[piece][colour] ^= moveMask;
        game->checkMap[oppositeColour] ^= game->attackMap[getPiecetype(game,where)][getPieceColour(game,where)][from];

        updateOccupancy(game);

        game->turn = oppositeColour;
        return 1;
    }

    if(((game->attackMap[piece][colour][from] & game->mask[where]) != 0ULL ) && ((game->occupancy[oppositeColour] & game->mask[where] )!= 0ULL)){
        int capturedPiece;
        for( capturedPiece = P; capturedPiece  <= K; capturedPiece++){
            if((game->pieces[capturedPiece][oppositeColour] & game->mask[where]) != 0ULL)
                game->pieces[capturedPiece][oppositeColour] ^= game->mask[where];
                break;
        }
       
        game->pieces[piece][colour] ^= moveMask;

        updateOccupancy(game);
        game->turn = oppositeColour;
        return 2;
    }
    
    return 0;
}

/*** input ***/

enum{
    up = 0,
    down,
    left,
    right
};

void deselectPiece(board *game)
{
    if (game->selected_position == -1 )
        return;

    int pos = game->selected_position;
    int piece = getPieceType(game, pos);
    int colour = getPieceColour(game, pos);

    if(piece == R || piece == Q || piece == B){
        game->attackMap[piece][colour][pos] = 0ULL;
        game->moveMap[piece][colour][pos] = 0ULL;
    }

    game->selected_position = -1;
}

void selectPiece(board *game){


    if (getPieceType(game,game->cursor_position) == EMPTY && getPieceColour(game,game->cursor_position) == game->turn){
        deselectPiece(game);
        return;
    }

    game->selected_position = game->cursor_position;

    createPieceMoveAttackMap(game,game->selected_position);
}

int moveCursor(board *game, int dir){
    int dx[4] = {0,0,-1,1};
    int dy[4] = {1,-1,0,0};
    if(!outOfBounds(game->cursor_position % 8 + dx[dir],game->cursor_position/8 + dy[dir])){
        game->cursor_position += dx[dir] + dy[dir]*8;
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
            selectPiece(game);
            return 1;
        }
        else if (ev.key == TB_KEY_BACKSPACE || ev.key == TB_KEY_BACKSPACE2)
        {
            removeChar(s);
            return 1;
        }
        else if(ev.key == TB_KEY_ESC)
        {
            deselectPiece(game);
            return 1;

        }
        else if((char)ev.ch == ' ')
        {
            if(game->selected_position != -1){
                int result = move(game, game->selected_position, game->cursor_position);
                if(result > 0){
                    deselectPiece(game);  
                }else{
                    selectPiece(game);
                }
            }else
                selectPiece(game);
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

struct  rendered{int squareColour, pieceType, pieceColour, selected, hovered;};

void renderBoard(board *game, struct rendered renderedBoard[8][8] ){
    
    for(int rank = 0; rank < 8; rank++){
        for(int file = 0; file < 8; file++){
            int position = rank*8 + file;

            renderedBoard[rank][file].pieceType = EMPTY;
            renderedBoard[rank][file].pieceColour = -1;
            renderedBoard[rank][file].squareColour = (rank + file) %2 == 0 ? 0 : 1;


            for( int colour = WHITE; colour <=BLACK; colour++){
                for(int piece = P; piece <= K; piece++){
                    if(game->pieces[piece][colour] & game->mask[position]){
                        renderedBoard[rank][file].pieceType = piece;
                        renderedBoard[rank][file].pieceColour = colour;
                    }
                }
            }
            
            

            if(file + rank*8 == game->cursor_position)
                renderedBoard[rank][file].hovered = 1;
            else 
                renderedBoard[rank][file].hovered = 0;


            if(file + rank*8 == game->selected_position)
                renderedBoard[rank][file].selected = 1;
            else 
                renderedBoard[rank][file].selected = 0;    
     
            
            
        }
    }


}

char *renderPiece[2][7] = {{" ♙ ", " ♘ ", " ♗ ", " ♖ ", " ♕ ", " ♔ ", "   "},{" ♟ ", " ♞ ", " ♝ ", " ♜ ", " ♛ ", " ♚ ", "   "}};
int rendercolour[2] = {TB_WHITE,TB_BLACK};

void drawBoard(board *game){
    // Use a local buffer for the view
    struct rendered boardView[8][8];
    
    // Populate data
    renderBoard(game, boardView);
    
    int x_ofset = (tb_width() - 24 )/2;
    int y_ofset = 5;
    
    for(int rank = 0; rank < 8; rank++) {    
        for(int file = 0; file < 8; file++) { 
        
            int position = rank * 8 + file; 
            int draw_x = x_ofset + (file * 3);
            int draw_y = y_ofset + (7 - rank);
            
            // Extract data for current square
            int pType = boardView[rank][file].pieceType;
            int pColor = boardView[rank][file].pieceColour;
            int sqColor = boardView[rank][file].squareColour;
            char *symbol = (pColor !=  -1 ) ? renderPiece[pColor][pType] : "   " ; 
            
            // Base Colors
            int bg_colour = sqColor ? TB_BLUE : TB_CYAN; 
            //int fr_colour = (pColor == WHITE) ? (TB_WHITE | TB_BOLD) : (TB_BLACK );
            int fr_colour = TB_WHITE;

            // Selection Highlight
            if(boardView[rank][file].selected) {
                bg_colour = TB_GREEN;
            }

            // Move & Capture Highlights
            if(game->selected_position != -1) {
                
                // Get details of the SELECTED piece 
                int selRank = game->selected_position / 8;
                int selFile = game->selected_position % 8;
                int selType = boardView[selRank][selFile].pieceType;
                int selColor = boardView[selRank][selFile].pieceColour;

                if (selType != EMPTY && selColor != -1) {
                    int selOppColor = (selColor == WHITE) ? BLACK : WHITE;

                    // Check if Selected piece can move to Current Square (position)
                    if (game->moveMap[selType][selColor][game->selected_position] & game->mask[position]) {
                        bg_colour = sqColor ? (TB_BLUE | TB_BRIGHT) : (TB_CYAN | TB_BRIGHT );
                    }
                    // Check if Selected piece can capture at Current Square
                    else if ((game->attackMap[selType][selColor][game->selected_position] & game->mask[position]) 
                          && (game->occupancy[selOppColor] & game->mask[position])) {
                        bg_colour = TB_RED | TB_BRIGHT;
                        tb_printf(draw_x, draw_y, TB_WHITE, bg_colour, "[");
                        tb_printf(draw_x + 2, draw_y, TB_WHITE, bg_colour, "]");
                    }
                }
            }

            // Render
            if (game->outputMode == 0) {
                tb_printf(draw_x, draw_y, fr_colour, bg_colour, "%s", symbol);
            }

            // Cursor Brackets
            if(boardView[rank][file].hovered) {
                tb_printf(draw_x, draw_y, TB_GREEN | TB_BRIGHT , bg_colour, "[");
                tb_printf(draw_x + 2, draw_y, TB_GREEN |TB_BRIGHT , bg_colour, "]");
            }
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
    game.outputMode = 0;      // 0--normal
                                // 1##--attackMap #(piece) #(colour)

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