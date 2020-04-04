#include <immintrin.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/**
 * Edward Wawrzynek
 * A solver for an arbitrary m,n,k game
 * An m,n,k is a game played on a M x N board, where players alternate placing stones, and the first to place K in a row wins
 *
 * The algorithm used is a minimax algorithm (negamax, which relies on the fact that a two player game is zero sum).
 * 
 * Ideally, we could search the entire game tree and look at all outcomes. This is impossible for all but the smallest boards.
 * Instead, we use a bounded minimax -- we search as far down the tree as we reasonably can, than evaluate boards with an evaluation function.
 * 
 * ---- Evaluation Function ----
 * Given a board, the evaluation function applies heuristics and gives it a score for how good it is for the player. The evaluation function must match the zero sum nature of the game--the score for a board must be the inverse of the score for the inverse board.
 * The evaluation function does not search the game tree--it is only called once the depth of the search tree is exhausted.
 * A winning board is +infinity
 * A lossing board is -infinity
 * TODO (other heuristics)
 * 
 * ---- Minimax Algorithm ----
 * The minimax algorithm searches the game tree depth-first. For each non leaf node, it looks at all of its branches. For each one, it scores it according to the worst possible outcome, and then picks the branch with the best score.
 * 
 * If the search depth is exceeded, nodes are treated as leaf nodes an given a score by the evaluation function.
 * 
 * Basically, it picks the choice that has the best worst-case outcome. It minimizes the maximum possible loss (therefor, minimax).
 * 
 * ---- Notes ----
 * 
 * Minimax assumes a perfect opponent -- one who will always pick the worst for us (so we force them to give us the best worst outcome).
 * 
 * The opponents certainly aren't perfect, but assuming a non perfect opponent for a move is unsafe -- an opponent may make a perfect move. We avoid blunders instead of taking risks.
 * 
 * There may be cases where a perfect opponent would always beat us (ie all branches are -infinity), but a non perfect opponent will miss it. This is probably decently likely (especially in endgame). The best move is probably the one that delays the loss the furthest (ie more chances for the opponent to screw up). We need to therefore pass this information with the score.
 * 
 * Or, we can normalize evaluation function to some range(-1 to 1), than pick large values (-1000 and 1000) for wins and loses. Each time we pass a max/min up, if it is < -1, we add one and pass it up. Thus, losses further in the future are seen as better.
 * 
 * We could do the same with values > 1 (subtract one when we pass them up) to make wins further in the future less desirable than wins closer in the future. (This isn't, however required -- if a branch is +1000, we can win it no matter what. May end games quicker + less moves for us the screw up (timeout on, etc)). NOTE: we can't use negamax with this scheme. Use regular minimax. Alpha-beta pruning will still work, this may hurt the effectiveness of alpha beta prunning.
 * 
 * For mostly blank boars, we may be able to explore only part of them. For a board with 225 cells empty (15x15), we can only explore two moves ahead without running out of time.
 * One solution is to always run a precursory two level deep search -- first for wins, then for losses, then for fork opportunities, then for blocking forks. That should take < 1s, which still gives a decent amount of time for minimax search.
*/

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

// M, N, and K for current board to solve
bloc_t M, N, K;

/**
 * prints out a board, with us as U and them as T */
void printBoard(board_t *b) {
	for(int y = 0; y < N; y++) {
		for(int x = 0; x < M; x++) {
			switch(b->board[x][y]) {
				case PLAYER_US:
					printf(" U");
					break;
				case PLAYER_THEM:
					printf(" T");
					break;
				default:
				case PLAYER_NONE:
					printf("  ");
					break;
			}
		}
		printf("\n");
	}
	for(int i = 0; i < M; i++) printf("--");
	printf("\n");
}

/**
 * Checks a board for a number of win conditions
 * Returns 1 if player1 won, 2 if player2 won, 0 if nobody won, and 4 if the board is tied */
static player_t checkWin(board_t *b) {
	// TODO: investigate using SIMD

	// TODO: check if bounding loops on M and N or running all loops to 15 and allowing full unrolling has better performance
	// check rows
	for(bloc_t y = 0; y < N; y++) {
		bloc_t count1 = 0;
		bloc_t count2 = 0;
		for(bloc_t x = 0; x < M; x++) {
			if(b->board[x][y] & PLAYER_US) count1++;
			else count1 = 0;

			if(b->board[x][y] & PLAYER_THEM) count2++;
			else count2 = 0;

			if(count1 >= K) return PLAYER_US;
			if(count2 >= K) return PLAYER_THEM;
		}
	}

	// check columns 
	for(bloc_t x = 0; x < N; x++) {
		bloc_t count1 = 0;
		bloc_t count2 = 0;
		for(bloc_t y = 0; y < M; y++) {
			if(b->board[x][y] & PLAYER_US) count1++;
			else count1 = 0;

			if(b->board[x][y] & PLAYER_THEM) count2++;
			else count2 = 0;

			if(count1 >= K) return PLAYER_US;
			if(count2 >= K) return PLAYER_THEM;
		}
	}

	// check diagonals
	// check down and left diagonals
	for(bloc_t i = 0; i < (M + N - 1); i++) {
		bloc_t numCont1 = 0;
		bloc_t numCont2 = 0;
		bloc_t j = (i < M) ? 0 : (i - M + 1);
		bloc_t z = (i < N) ? (i + 1) : N;
		for(bloc_t y = j; y < z; y++) {
			bloc_t x = i - y;

			if(b->board[x][y] & PLAYER_US) numCont1++;
			else numCont1 = 0;
			if(b->board[x][y] & PLAYER_THEM) numCont2++;
			else numCont2 = 0;

			if(numCont1 >= K) return PLAYER_US;
			if(numCont2 >= K) return PLAYER_THEM;
		}
	}
	// check down and right diagonals
	// also checks for a tie (ie -- full boards)
	bloc_t hasEmpty = 0;
	for(bloc_t i = 0; i < (M + N - 1); i++) {
		bloc_t numCont1 = 0;
		bloc_t numCont2 = 0;
		bloc_t j = (i < N) ? (N - i - 1) : 0;
		bloc_t z = (i < M) ? N : (M + N - i - 1);
		for(bloc_t y = j; y < z; y++) {
			bloc_t x = i + y - N + 1;

			if(!b->board[x][y]) hasEmpty = 1;

			if(b->board[x][y] & PLAYER_US) numCont1++;
			else numCont1 = 0;
			if(b->board[x][y] & PLAYER_THEM) numCont2++;
			else numCont2 = 0;

			if(numCont1 >= K) return PLAYER_US;
			if(numCont2 >= K) return PLAYER_THEM;
		}
	}

	return hasEmpty ? PLAYER_NONE : PLAYER_TIE;
}

/**
 * Given the position currently being examined, get the next position to examine
 * If *x is -1, start at first position
 * 
 * x and y are set to the next position
 * 
 * return 1 if there is a next position, 0 otherwise */
static int nextPosition(board_t *b, bloc_t *x, bloc_t *y) {
	bloc_t newx = *x;
	bloc_t newy = *y;

	if(*x == -1) {
		newx = 0;
		newy = 0;
	} else {
		newx++;
		if(newx >= M) {
			newx = 0;
			newy++;
			if(newy >= N) return 0;
		}
	}
	do {
		if(b->board[newx][newy] == PLAYER_NONE) {
			*x = newx;
			*y = newy;
			return 1;
		}
		newx++;
		if(newx >= M) {
			newx = 0;
			newy++;
		}
	} while(newy < N);

	return 0;
}

/**
 * Copy the board from src to dst and place player's stone at x, y
 */
static void copyWithMove(board_t *dst, board_t *src, player_t player, bloc_t x, bloc_t y) {
	memcpy(dst, src, sizeof(board_t));
	dst->board[x][y] = player;
}

/**
 * Check for obvious moves we should make
 * Run pre-minimax
 * Checks for wins, blocking wins, forking, blocking forks
 * Returns 1 and sets x and y if a move was found, 0 otherwise
 * 
 * x and y are clobbered no matter the result
 * 
 * While minimax would detect all of these, it may miss forks if it can only run two levels deep, which it has to do on mostly empty large boards.
 */
static int basicSolve(board_t *b, bloc_t *x, bloc_t *y) {
	// scratch boards
	board_t s0, s1;
	bloc_t sx, sy;
	// win if we can
	*x = -1;
	while(nextPosition(b, x, y)) {
		copyWithMove(&s0, b, PLAYER_US, *x, *y);
		if(checkWin(&s0) == PLAYER_US) return 1;
	}
	// block win
	*x = -1;
	while(nextPosition(b, x, y)) {
		copyWithMove(&s0, b, PLAYER_THEM, *x, *y);
		if(checkWin(&s0) == PLAYER_THEM) return 1;
	}
	// fork if we can
	*x = -1;
	while(nextPosition(b, x, y)) {
		copyWithMove(&s0, b, PLAYER_US, *x, *y);
		sx = -1;
		bloc_t winCount = 0;
		while(nextPosition(&s0, &sx, &sy)) {
			copyWithMove(&s1, &s0, PLAYER_US, sx, sy);
			if(checkWin(&s1) == PLAYER_US) winCount++;
		}
		if(winCount >= 2) return 1;
	}
	// block fork if we can
	*x = -1;
	while(nextPosition(b, x, y)) {
		copyWithMove(&s0, b, PLAYER_THEM, *x, *y);
		sx = -1;
		bloc_t winCount = 0;
		while(nextPosition(&s0, &sx, &sy)) {
			copyWithMove(&s1, &s0, PLAYER_THEM, sx, sy);
			if(checkWin(&s1) == PLAYER_THEM) winCount++;
		}
		if(winCount >= 2) return 1;
	}
	
	return 0;
}

int main() {
	M = 15;
	N = 15;
	K = 15;
	board_t b;
	memset(&b, 0, sizeof(board_t));
	b.board[0][0] = 1;


	bloc_t x = -2, y;

	printf("found move: %u\n", basicSolve(&b, &x, &y));
	printf("x: %i, y: %i\n", x, y);
}