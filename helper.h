#ifndef _HELPER_
#define _HELPER_

#include "err.h"
#include <sys/types.h> 
#include <mqueue.h>
#include <stdbool.h>


#define ALPHABET_SIZE 26
#define MAX_STATES 100
#define RES_SIZE 1
#define RDWR_SIZE 2
#define READ 0
#define WRITE 1
#define DSC_ADRESS_SIZE 10
#define STARTING_POSITION 0
#define PARALLELISM_EFFICIENT_FACTOR 20
#define GUARD -1
#define ERROR -1
#define INIT_PROCESS_PID 1

#define ORDERS "/orders"
#define RESULTS "/results"
#define MSGSIZE 1020
#define MAXMSG 10
#define MAX_PID_SIZE 5

/* testers result mq priorities */
#define END_WORK_PRIORITY 1
#define CHECKED_WORD_PRIORITY 2
#define NO_NEW_WORDS_PIORITY 3

/* validator orders mq priorities */
#define ORDER_FROM_TESTER 1
#define RESULT_FROM_RUN 2
#define NEW_TESTER_PRIORITY 3

#define ACCEPTED "A"
#define DECLINED "N"
#define NO_NEW_WORDS "!"
#define EMPTY_WORD "E"
#define EMPTY_CHAR 'E'
#define EOL '\n'

#define END_SENDERS_WORK (SIGRTMIN + 1)
#define RUN_PROCESS_FAILED (SIGRTMIN + 2)


typedef struct Tester {
  pid_t pid;
  mqd_t desc;
  int rcd;
  int snt;
  int acc;
  bool closed;
  struct Tester* next;
} Tester;

typedef struct Automaton {
  int A; // alphabet size
  int Q; // number of states
  int U; // number of universal states
  int F; // number of accepting states
  int q; // starting state 
  int G[MAX_STATES]; // accepting states
  int T[MAX_STATES][ALPHABET_SIZE][MAX_STATES + 1]; // T[i][j][0] = number of transitions
} Automaton;

extern Tester* newTester(int);

extern int loadAutomaton(Automaton*);

extern void fixStringEncoding(char*, int, int);

extern int loadWordStdin(char*);


#endif /* HELPER_H */