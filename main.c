#include <json.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include "board.h"
#include <unistd.h>

int json_board_callback(void *board, int type, const char *data, uint32_t length)
{
  // 0 for m, 1 for n, 2 for k, 3 for board
	static int lastParam = -1;
  static int x = 0;
  static int y = 0;

	switch (type) {
	case JSON_ARRAY_END:
		y = 0;
    x++;
		break;
	case JSON_KEY:
    if(!strncmp(data, "m", length)) lastParam = 0;
    else if(!strncmp(data, "n", length)) lastParam = 1;
    else if(!strncmp(data, "k", length)) lastParam = 2;
    else if(!strncmp(data, "board", length)) lastParam = 3;
    else lastParam = -1;
    break;
	case JSON_INT:
    if(lastParam == 0) M = strtol(data, NULL, 10);
    if(lastParam == 1) N = strtol(data, NULL, 10);
    if(lastParam == 2) K = strtol(data, NULL, 10);
    if(lastParam == 3) {
      int cell = strtol(data, NULL, 10);
      if(cell == -1) ((board_t*)board)->board[x][y] = 0;
      if(cell == 0) ((board_t*)board)->board[x][y] = 1;
      if(cell == 1) ((board_t*)board)->board[x][y] = 2;
      if(x >= M || y >= N) fprintf(stderr, "board data exceeded M and N\n");
      y++;
    }
    break;
  case JSON_OBJECT_END:
    lastParam = -1;
    x = 0;
    y = 0;
    break;
	case JSON_NULL:
		((board_t*)board)->board[0][0] = 8;
  case JSON_ARRAY_BEGIN:
  case JSON_OBJECT_BEGIN:
    break;
	default:
    fprintf(stderr, "Unexpected JSON atom type\n");
	}

  return 0;
}

/**
 * Handle loading data into memory with libcurl
 */
struct MemoryStruct {
  char *memory;
  size_t size;
};
 
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if(ptr == NULL) {
    /* out of memory! */ 
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
 
  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}

/**
 * Load a board from the api, set M, N, and K
 * Returns nonzero on failure, 0 on success */
int loadBoard(board_t *board, const char * url, const char * key) {
  char * finalUrl = malloc(strlen(url) + strlen(key) + 16);
  sprintf(finalUrl, "%s/api/board?key=%s", url, key);
  /* make http request */
  CURL *curl;
  curl = curl_easy_init();

  struct MemoryStruct chunk;
  chunk.memory = malloc(1);
  chunk.size = 0;

  curl_easy_setopt(curl, CURLOPT_URL, finalUrl);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

  int res = curl_easy_perform(curl);
  if(res != CURLE_OK) {
    curl_easy_cleanup(curl);
    free(finalUrl);
    fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    free(chunk.memory);
    return 1;
  }
  curl_easy_cleanup(curl);
  free(finalUrl);
  if(!strncmp(chunk.memory, "null", 4)) {
    free(chunk.memory);
    return 1;
  }

  json_parser parser;
  if(json_parser_init(&parser, NULL, &json_board_callback, board)) {
    fprintf(stderr, "Failed to initialize JSON parser\n");
    return 1;
  }
  memset(board, 0, sizeof(board_t));
  int ret;
  if((ret = json_parser_string(&parser, chunk.memory, chunk.size, NULL))) {
    fprintf(stderr, "Failed to parse JSON data %i\n", ret);
    json_parser_free(&parser);
    free(chunk.memory);
    return 1;
  }
  free(chunk.memory);
  json_parser_free(&parser);
  // parser is indicating a null board was returned
  if(board->board[0][0] == 8) {
    memset(board, 0, sizeof(board_t));
    return 1;
  }
  return 0;
}

/**
 * Set the ai's name */
void setName(char * name, char * url, char * key) {
  printf("Setting Name: %s --- ", name);
  char * finalUrl = malloc(strlen(url) + 14);
  char * options = malloc(strlen(name) + strlen(key) + 11);
  sprintf(options, "key=%s&name=%s", key, name);
  sprintf(finalUrl, "%s/api/set_name", url);

  CURL *curl;
  curl = curl_easy_init();

  curl_easy_setopt(curl, CURLOPT_URL, finalUrl);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, 1);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, options);

  int res = curl_easy_perform(curl);

  if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

  curl_easy_cleanup(curl);
  free(finalUrl);
  free(options);
  printf("\n");
}

/**
 * send a move to the server
 * If board is not null, it will be printed with the move indicated */
void postMove(bloc_t x, bloc_t y, char *url, char *key, board_t *board) {
  if(board != NULL) {
    board_t scratch;
    memcpy(&scratch, board, sizeof(board_t));
    scratch.board[x][y] = PLAYER_TIE;
    printBoard(&scratch);
  }
  printf("Sending Move: (%li, %li) --- ", x, y);
  char * finalUrl = malloc(strlen(url) + 10);
  char * options = malloc(strlen(key) + 15);
  sprintf(options, "key=%s&x=%li&y=%li", key, x, y);
  sprintf(finalUrl, "%s/api/move", url);

  CURL *curl;
  curl = curl_easy_init();

  curl_easy_setopt(curl, CURLOPT_URL, finalUrl);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_HTTPPOST, 1);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, options);

  int res = curl_easy_perform(curl);

  if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

  curl_easy_cleanup(curl);
  free(finalUrl);
  free(options);
  printf("\n");
}

int calculateDepth(int openNodes, int maxNodesSearched) {
  int searched = openNodes;
  for(int i = 1; i < 20; i++) {
    if(openNodes == 0) return i - 1;
    if(searched >= maxNodesSearched) return i - 1;
    searched *= --openNodes;
  }
  return 20;
}

#define MAX_MINIMAX_SEARCH_NODES 800000

int main(int argc, char ** argv) {
  if(argc < 3) {
    fprintf(stderr, "Usage: mnk url key\n");
    return 1;
  }
  board_t b;
  memset(&b, 0, sizeof(board_t));
  bloc_t x, y;

  setName("Wawrzynek HeuristicMinimax", argv[1], argv[2]);

  while(1) {
    if(!loadBoard(&b, argv[1], argv[2])) {
      printf("Solving Board:\n");
      printBoard(&b);
      if(basicSolve(&b, &x, &y)) {
        printf("BasicSolve Found Move\n");
        postMove(x, y, argv[1], argv[2], &b);
      } else {
        printf("BasicSolve Didn't Find Move\n");
        // calculate depth for minimax
        int depth = calculateDepth(countEmpty(&b), MAX_MINIMAX_SEARCH_NODES);
        printf("Doing minimax with depth=%i\n", depth);
        if(minimaxMove(&b, &x, &y, depth)) {
          printf("Minimax Found Move\n");
          postMove(x, y, argv[1], argv[2], &b);
        } else {
          printf("Minimax Didn't find move\n");
          if(higestScoredMove(&b, &x, &y)) {
            printf("HigestScore Found Move\n");
            postMove(x, y, argv[1], argv[2], &b);
          } else {
            printf("HigestScore Didn't Find Move\n");
            if(backUpMove(&b, &x, &y)) {
              printf("BackUp Found Move\n");
              postMove(x, y, argv[1], argv[2], &b);
            } else printf("BackUp Didn't Find Move. Giving Up\n");
          }
        }
      }
    } else {
      printf("No Board to Solve\n");
    }
    sleep(1);
  }
}