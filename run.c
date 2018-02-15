/* RUN
Program run receives from validator a word to verify and the description of the 
automaton. Then he begins the verification. When the verification stops, the 
process sends a message to the server and terminates. */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>       
#include <unistd.h>    
#include <sys/stat.h>   
#include <sys/types.h>    
#include <sys/prctl.h>
#include <signal.h>
#include <mqueue.h>
#include <errno.h>
#include "helper.h"


Automaton automaton;
pid_t validatorPid;

/* buffers */
char mess[MSGSIZE + 1];
char word[MSGSIZE + 1];
char res[RES_SIZE + 1];

/* messages */
mqd_t orders;


void clean() {
  mq_close(orders);
}

void shutDownProgram() {
  fprintf(stderr, "RUN(%d): ERROR %s\n", getpid(), strerror(errno));
  kill(validatorPid, RUN_PROCESS_FAILED);
  clean();
  exit(1);
}

void connectToOrdersQueue() {
  orders = mq_open(ORDERS, O_WRONLY);
  if (orders == (mqd_t) ERROR) {
    shutDownProgram();
  }
}

void getAutomaton(char* pReadDscAdress) {
  int readDsc = atoi(pReadDscAdress);
  if (read(readDsc, (char*) &automaton, sizeof(Automaton)) == ERROR) {
    close(readDsc);
    shutDownProgram();
  }
  close(readDsc);
}

void redefineSigchild() {
  sigset_t blockMask;
  sigemptyset(&blockMask); 
  struct sigaction action;
  action.sa_handler = 0;
  action.sa_mask = blockMask;
  action.sa_flags = SA_NOCLDWAIT;
  action.sa_handler = 0;
  sigaction(SIGCHLD, &action, NULL);  
}

int init(char** argv) {
  validatorPid = atoi(argv[3]);
  prctl(PR_SET_PDEATHSIG, SIGTERM);
  if (getppid() == INIT_PROCESS_PID) {
    exit(ERROR);
  }
  redefineSigchild();
  connectToOrdersQueue();
  getAutomaton(argv[2]);
  return 0;
}

bool isAccepting(int state) {
  for (int i = 0; i < automaton.F; i++) {
    if (automaton.G[i] == state) {
      return true;
    }
  }
  return false;
}

bool isUniversal(int state) {
  return automaton.U > state;
}

bool accept(int, int, int);

bool divideWork(int len, int pos, int state, int transitionsNo) {
  int secDim = word[pos] - 'a';
  int p[RDWR_SIZE]; pipe(p);

  bool pathRes;  
  for (int i = 1; i <= transitionsNo; i++) {
    switch (fork()) {
      case ERROR:
        shutDownProgram();

      case 0:
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (getppid() == INIT_PROCESS_PID) {
          exit(ERROR);
        }
        redefineSigchild();
        close(p[READ]);
        pathRes = accept(len, pos + 1, automaton.T[state][secDim][i]);
        write(p[WRITE], &pathRes, sizeof(pathRes));
        close(p[WRITE]);
        exit(0);

      default:
        break;
    }
  }
  
  for (int i = 1; i <= transitionsNo; i++) {
    read(p[0], &pathRes, sizeof(pathRes));
    if (isUniversal(state) && pathRes == false) {
      close(p[READ]); close(p[WRITE]); 
      return false;
    } else if (!isUniversal(state) && pathRes) {
      close(p[READ]); close(p[WRITE]);
      return true;
    }
  }
  return isUniversal(state);
}

bool accept(int len, int pos, int state) {
  if (pos == len) {
    return isAccepting(state);
  }

  int secDim = word[pos] - 'a';
  int transitionsNo = automaton.T[state][secDim][0];

  if (transitionsNo == 0) {
    return isUniversal(state);
  }

  if (transitionsNo > 1 && len - pos > PARALLELISM_EFFICIENT_FACTOR) {
    return divideWork(len, pos, state, transitionsNo);
  }

  for (int i = 1; i <= transitionsNo; i++) {
    bool pathRes = accept(len, pos + 1, automaton.T[state][secDim][i]);
    if (isUniversal(state) && pathRes == false) {
      return false;
    } else if (!isUniversal(state) && pathRes == true) {
      return true;
    }
  }
  return isUniversal(state);
}

void verifyWord(char* argv1) {
  int tmp;
  sscanf(argv1, "%d %s", &tmp, word);
  
  bool acc;
  if (!strcmp(word, EMPTY_WORD)) {
    acc = isAccepting(automaton.q);
  } else {
    acc = accept(strlen(word), STARTING_POSITION, automaton.q);
  }
  if (acc) {
    strncpy(res, ACCEPTED, RES_SIZE);
  } else {
    strncpy(res, DECLINED, RES_SIZE);
  }

  sprintf(mess, "%s %s", argv1, res);
  if (mq_send(orders, mess, strlen(mess), RESULT_FROM_RUN) == ERROR) {
    shutDownProgram();
  }
}

int main(int argc, char** argv) {
  init(argv);

  verifyWord(argv[1]);

  clean();
  return 0;
}