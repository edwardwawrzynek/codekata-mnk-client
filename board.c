#include "board.h"

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

// M, N, and K for current board to solve
bloc_t M, N, K;

/**
 * prints out a board, with us as green and them as red */
void printBoard(board_t *b) {
	for(int y = 0; y < N; y++) {
		for(int x = 0; x < M; x++) {
			switch(b->board[x][y]) {
				case PLAYER_US:
					printf("\x1b[1;32m #\x1b[m");
					break;
				case PLAYER_THEM:
					printf("\x1b[1;31m #\x1b[m");
					break;
				case PLAYER_NONE:
					printf(" .");
					break;
				case PLAYER_TIE:
					printf("\x1b[1;34m #\x1b[m");
					break;
				default:
					printf(" ?");
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
	for(bloc_t x = 0; x < M; x++) {
		bloc_t count1 = 0;
		bloc_t count2 = 0;
		for(bloc_t y = 0; y < N; y++) {
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
int basicSolve(board_t *b, bloc_t *x, bloc_t *y) {
	// scratch boards
	board_t s0, s1, s2;
	bloc_t sx, sy, sx2, sy2;
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
	// fork if we can (incl. third level forks)
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
	// block fork if we can TODO: if multiple forks, block the one (if any) that forces them to defend
	*x = -1;
	while(nextPosition(b, x, y)) {
		copyWithMove(&s0, b, PLAYER_THEM, *x, *y);
		sx = -1;
		bloc_t winCount = 0;
		while(nextPosition(&s0, &sx, &sy)) {
			copyWithMove(&s1, &s0, PLAYER_THEM, sx, sy);
			if(checkWin(&s1) == PLAYER_THEM) winCount++;
		}
		/* if we found only one direct win, check for third level fork */
		if(winCount == 1) {
			sx = -1;
			while(nextPosition(&s0, &sx, &sy)) {
				copyWithMove(&s1, &s0, PLAYER_US, sx, sy);
				sx2 = -1;
				bloc_t winCount2 = 0;
				while(nextPosition(&s1, &sx2, &sy2)) {
					copyWithMove(&s2, &s1, PLAYER_US, sx2, sy2);
					if(checkWin(&s2) == PLAYER_US) winCount2++;
				}
				if(winCount2 >= 2) winCount++;
			}
		}
		if(winCount >= 2) return 1;
	}
	// block fork if we can (and third level fork)
	*x = -1;
	while(nextPosition(b, x, y)) {
		copyWithMove(&s0, b, PLAYER_THEM, *x, *y);
		sx = -1;
		bloc_t winCount = 0;
		while(nextPosition(&s0, &sx, &sy)) {
			copyWithMove(&s1, &s0, PLAYER_THEM, sx, sy);
			if(checkWin(&s1) == PLAYER_THEM) winCount++;
		}
		/* if we found only one direct win, check for third level fork */
		if(winCount == 1) {
			sx = -1;
			while(nextPosition(&s0, &sx, &sy)) {
				copyWithMove(&s1, &s0, PLAYER_THEM, sx, sy);
				sx2 = -1;
				bloc_t winCount2 = 0;
				while(nextPosition(&s1, &sx2, &sy2)) {
					copyWithMove(&s2, &s1, PLAYER_THEM, sx2, sy2);
					if(checkWin(&s2) == PLAYER_THEM) winCount2++;
				}
				if(winCount2 >= 2) winCount++;
			}
		}
		if(winCount >= 2) return 1;
	}
	
	return 0;
}

/**
 * Board evaluation function for minimax.
 * Pieces in the center of the board (TODO: how to define) are worth +2, edges +1
 * For each row, column, or diagonal uninterrupted by the enemy's stones for at least K long, each piece adds +2, and is increased by 1 after each piece
 * 
 * So one piece in an empty row of K is +2, two pieces is +5 (2+3), three is +9 (2+3+4), etc
 * 
 * Basically
 * - Having a potential winning row/col/diag is good, and is given a better score the closer it is to winning
 * - Being in the center is also good
 */

// Preprocessor abuse
#define EVAL_COUNTERS_DECL() 			\
/* Player 1 counters */						\
/* how long the run has gone */		\
bloc_t runLen1 = 0;								\
/* how many stones in the run */	\
bloc_t pieces1 = 0;								\
/* current score of run */				\
bloc_t runScore1 = 0;							\
bloc_t runLen2 = 0;								\
bloc_t pieces2 = 0;								\
bloc_t runScore2 = 0;

#define EVAL_END_RUN() 										\
if(runLen2 >= K) finalScore -= runScore2; \
if(runLen1 >= K) finalScore += runScore1;

#define EVAL_RUN_BODY() 											\
if(b->board[x][y] & PLAYER_US) {							\
	pieces1++;																	\
	runScore1 += pieces1 + 1;										\
	runLen1++;																	\
	/* this ends a PLAYER_THEM run */						\
	if(runLen2 >= K) finalScore -= runScore2;		\
	runLen2 = 0;																\
	runScore2 = 0;															\
	pieces2 = 0;																\
} else if(b->board[x][y] & PLAYER_THEM) {			\
	pieces2++;																	\
	runScore2 += pieces2 + 1;										\
	runLen2++;																	\
	/* this ends a PLAYER_US run */							\
	if(runLen1 >= K) finalScore += runScore1;		\
	runLen1 = 0;																\
	runScore1 = 0;															\
	pieces1 = 0;																\
} else {																			\
	runLen1++;																	\
	runLen2++;																	\
}

static int evaluateBoard(board_t *b) {
	int finalScore = 0;
	/** score piece location **/
	/** center is defined as M/3 < x < 2*M/3, N/3 < y < 2*N/3 **/
	for(bloc_t x = 0; x < M; x++) {
		for(bloc_t y = 0; y < N; y++) {
			int value = (x >= (M/3) && x < (M - (M/3)) && y >= (N/3) && y < (N - (N/3))) ? 2 : 1;
			if(b->board[x][y] & PLAYER_US) finalScore += value;
			else if(b->board[x][y] & PLAYER_THEM) finalScore -= value;
		}
	}

	/** score pieces in relation to each other **/

	/** --- SCORE ROWS --- **/
	for(bloc_t y = 0; y < N; y++) {
		EVAL_COUNTERS_DECL();
		for(bloc_t x = 0; x < M; x++) {
			EVAL_RUN_BODY();
		}
		EVAL_END_RUN();
	}

	/** --- SCORE COLUMNS --- **/
	for(bloc_t x = 0; x < M; x++) {
		EVAL_COUNTERS_DECL();
		for(bloc_t y = 0; y < N; y++) {
			EVAL_RUN_BODY();
		}
		EVAL_END_RUN();
	}

	/** --- SCORE DIAGONALS --- **/
	// check down and left diagonals
	for(bloc_t i = 0; i < (M + N - 1); i++) {
		bloc_t j = (i < M) ? 0 : (i - M + 1);
		bloc_t z = (i < N) ? (i + 1) : N;

		EVAL_COUNTERS_DECL();
		for(bloc_t y = j; y < z; y++) {
			bloc_t x = i - y;

			EVAL_RUN_BODY();
		}
		EVAL_END_RUN();
	}
	// score down and right diagonals
	for(bloc_t i = 0; i < (M + N - 1); i++) {
		bloc_t j = (i < N) ? (N - i - 1) : 0;
		bloc_t z = (i < M) ? N : (M + N - i - 1);
		EVAL_COUNTERS_DECL();
		for(bloc_t y = j; y < z; y++) {
			bloc_t x = i + y - N + 1;
			EVAL_RUN_BODY();
		}
		EVAL_END_RUN();
	}

	return finalScore;
}

/**
 * Pick the first legal move as a back up in case other methods fail */
int backUpMove(board_t *b, bloc_t *x, bloc_t *y) {
	*x = -1;
	return nextPosition(b, x, y);
}

/**
 * Pick the higest scored move */
int higestScoredMove(board_t *b, bloc_t *x, bloc_t *y) {
	int maxScore = EVAL_N_INF;
	bloc_t max_x = -1, max_y = -1;
	board_t scratch;

	*x = -1;
	while(nextPosition(b, x, y)) {
		copyWithMove(&scratch, b, PLAYER_US, *x, *y);
		int score = evaluateBoard(&scratch);
		if(score > maxScore) {
			maxScore = score;
			max_x = *x;
			max_y = *y;
		}
	}

	*x = max_x;
	*y = max_y;
	return max_x != -1;
}

/**
 * Count the number of empty cells in a board
 */
int countEmpty(board_t *b) {
	int res = 0;
	for(int x = 0; x < M; x++) {
		for(int y = 0; y< N; y++) {
			if(b->board[x][y] == PLAYER_NONE) res++;
		}
	}
	return res;
}

static int min(int a, int b) {
	if(a < b) return a;
	else return b;
}
static int max(int a, int b) {
	if(a > b) return a;
	else return b;
}

/**
 * Minimax search. Search at most depth levels down. Runs alpha-beta pruning
 * In cases where all paths lead to loss, prefer losses further in the future 
 * 
 * b is the board to solve 
 * x and y are clobbered. They are the move that is made by the player
 * depth is the max levels down to visit
 * alpha and beta are used for pruning -- they should start at -infinity and +infinity
 * isMaximizePlayer is true if current move should be maximized. Maximizing player is us, minimizing them. Should be true if solving for us
 * 
 * returns score of branch */
static int minimax(board_t *b, bloc_t *x, bloc_t *y, int depth, int alpha, int beta, int isMaximizePlayer) {
	board_t child;
	// scratch x and y
	bloc_t sx, sy;
	// If node is terminal (win, loss, or tie) return its score
	player_t winner = checkWin(b);
	if(winner == PLAYER_US) return EVAL_INF;
	else if(winner == PLAYER_THEM) return EVAL_N_INF;
	else if(winner == PLAYER_TIE) return 0;

	// if depth == 0, score the node by the evaluation function
	if(depth == 0) return evaluateBoard(b);

	// visit each child branch, and choose the max or min (depending on which player makes this move)
	if(isMaximizePlayer) {
		int value = EVAL_N_INF;
		bloc_t max_x = -1, max_y = -1;
		// go through each child node
		*x = -1;
		while(nextPosition(b, x, y)) {
			copyWithMove(&child, b, PLAYER_US, *x, *y);
			// evaluate child node
			int nodeValue = minimax(&child, &sx, &sy, depth - 1, alpha, beta, 0);
			// store child move and value if it is max
			if(nodeValue > value) {
				value = nodeValue;
				max_x = *x;
				max_y = *y;
			}
			alpha = max(alpha, value);
			if(alpha >= beta) break;
		}
		*x = max_x;
		*y = max_y;
		// make losses better with age
		if(value < EVAL_MIN) value++;
		return value;
	} else {
		int value = EVAL_INF;
		bloc_t min_x = -1, min_y = -1;
		// go through each child node
		*x = -1;
		while(nextPosition(b, x, y)) {
			copyWithMove(&child, b, PLAYER_THEM, *x, *y);
			// evaluate child node
			int nodeValue = minimax(&child, &sx, &sy, depth - 1, alpha, beta, 1);
			// store child move and value if it is min
			if(nodeValue < value) {
				value = nodeValue;
				min_x = *x;
				min_y = *y;
			}
			alpha = min(alpha, value);
			if(alpha >= beta) break;
		}
		*x = min_x;
		*y = min_y;
		// make losses better with age
		if(value < EVAL_MIN) value++;
		return value;
	}
}

/**
 * Run the minimax algorithm
 */
int minimaxMove(board_t *b, bloc_t *x, bloc_t *y, int depth) {
	printf("Minimax Score: %i\n", minimax(b, x, y, depth, EVAL_N_INF, EVAL_INF, 1));
	return *x != -1 && *y != -1;
}