#include <immintrin.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// represents the state of a board
typedef struct {
	// the board is always 15x15. If m and n are smaller, extra cells are kept blank

	// each cell is 0 for a blank board, 1 for us, 2 for them
	uint8_t board[15][16];
} board_t;

// one dimensional position in a board
typedef int_fast32_t bloc_t;
// player type
typedef uint_fast8_t player_t;

#define PLAYER_NONE ((player_t)0)
#define PLAYER_US ((player_t)1)
#define PLAYER_THEM ((player_t)2)
#define PLAYER_TIE ((player_t)4)

void printBoard(board_t *b);
extern bloc_t M,N,K;
int basicSolve(board_t *b, bloc_t *x, bloc_t *y);
int backUpMove(board_t *b, bloc_t *x, bloc_t *y);
int higestScoredMove(board_t *b, bloc_t *x, bloc_t *y);
int minimaxMove(board_t *b, bloc_t *x, bloc_t *y, int depth);
int countEmpty(board_t *b);
// highest score possible by evaluation function
#define EVAL_MAX (7230)
#define EVAL_MIN (-7230)

#define EVAL_INF (10000)
#define EVAL_N_INF (-10000)
