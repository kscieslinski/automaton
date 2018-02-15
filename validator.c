/* VALIDATOR
The validator is a server accepting, or rejecting, words. A word is accepted if 
it is accepted by specified automaton. The program validator begins by reading 
from the standard input the description of the automaton and then in an infinite 
loop waits for the words to verify. When a word is received he runs the program 
run which validates the word and vaidator waits for an another word or response 
from the program run. Afer receiving a message from one of the run processes 
validator forwords the message to the adequate tester.

When a tester send an unique stop word ! the server stops , i.e. he does not 
accept new words, collects the responses from the run processes, forwards the 
answers, writes a report on stdout, and, finally, terminates.

The validator report consist in three lines describing the numbers of received 
queries, sent answers and accepted words:
Rcd: x\n
Snt: y\n
Acc: z\n

where x,y,z respectively are the numbers of received queries, sent answers and 
accepted words; and a sequence of summaries of the interactions with the 
programs tester from which validator received at least one query. A summary for 
a tester with PID pid consists in:

[PID: pid\n
Rcd: y\n
Acc: z\n]
wher pid,y,z respectively are: the process' pid, the number of messages received 
from this process and the number of acceped words sent by this process. */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>       
#include <unistd.h>    
#include <sys/stat.h>   
#include <sys/types.h>
#include <signal.h>
#include <mqueue.h>
#include <errno.h>
#include "helper.h"


Automaton automaton;
Tester testers;
int actRunProcesses = 0;
pid_t validatorPid;

/* buffers */
char mess[MSGSIZE + 1];
char buf[MSGSIZE + 1];
char res[RES_SIZE + 1];

/* messages */
mqd_t orders;
unsigned int msgPrio;

/* states */
bool acceptWordsToCheck = true;

/* report */
int rcd = 0;
int snt = 0;
int acc = 0;


void printReport() {
  printf("Rcd: %d\n", rcd);
  printf("Snt: %d\n", snt);
  printf("Acc: %d\n", acc);

  Tester* ptr = testers.next;
  while (ptr) {
    if (ptr->rcd != 0) {
      printf("Pid: %d\n", ptr->pid);
      printf("Rcd: %d\n", ptr->rcd);
      printf("Acc: %d\n", ptr->acc);
    }
    ptr = ptr->next;
  }
}

void informTesters(unsigned int prio) {
  sprintf(mess, EMPTY_WORD);
  Tester* ptr = testers.next;
  while (ptr) {
    mq_send(ptr->desc, mess, strlen(mess), prio);
    ptr = ptr->next;
  }
}

void closeSingleTester(Tester* ptr) {
  sprintf(mess, EMPTY_WORD);
  mq_send(ptr->desc, mess, strlen(mess), END_WORK_PRIORITY);
  mq_close(ptr->desc);
  ptr->closed = true;
}

void closeTesters() {
  Tester* ptr = testers.next;
  Tester* tmp;
  while (ptr) {
    if (!ptr->closed) {
      closeSingleTester(ptr);
    }
    tmp = ptr; 
    ptr = ptr->next;
    free(tmp);
  }
}

void clean() {
  closeTesters();
  mq_close(orders); mq_unlink(ORDERS);
}

void shutDownProgram() {
  fprintf(stderr, "VALIDATOR(%d): ERROR %s\n", getpid(), strerror(errno));
  clean();
  exit(1);
}

void createOrdersQueue() {
  struct mq_attr mqA; mqA.mq_maxmsg = MAXMSG; mqA.mq_msgsize = MSGSIZE; 
  orders = mq_open(ORDERS, O_RDWR | O_CREAT | O_CLOEXEC, S_IRUSR | S_IWUSR, &mqA);
  if (orders == (mqd_t ) ERROR) {
    shutDownProgram();
  }
}

void defineSignals() {
  sigset_t blockMask;
  sigemptyset(&blockMask); 
  sigaddset(&blockMask, RUN_PROCESS_FAILED);
  struct sigaction action;
  action.sa_handler = shutDownProgram;
  action.sa_mask = blockMask;
  action.sa_flags = 0;
  sigaction(RUN_PROCESS_FAILED, &action, NULL);
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

void init() {
  validatorPid = getpid();
  testers.next = NULL;

  createOrdersQueue();
  defineSignals();
  redefineSigchild();
  loadAutomaton(&automaton);
}

Tester* findTester(pid_t testerPid) {
  Tester* ptr = testers.next;
  while (ptr->pid != testerPid) {
    ptr = ptr->next;
  }
  return ptr;
}


void runRunProcess() {
  if (!acceptWordsToCheck) {
    return;
  }
  
  pid_t testerPid;
  sscanf(mess, "%d", &testerPid);
  Tester* src = findTester(testerPid);
  src->rcd += 1; rcd++;

  int p[RDWR_SIZE];  pipe(p);
  char pReadDscAdress[DSC_ADRESS_SIZE];
  char validatorPidStr[MAX_PID_SIZE + 1];
  pid_t runProcess;
  switch (runProcess = fork()) {
    case ERROR:
      shutDownProgram();

    case 0:
      close(p[WRITE]);
      sprintf(pReadDscAdress,  "%d", p[READ]);
      sprintf(validatorPidStr, "%d", validatorPid);
      execl("./run", "run", mess, pReadDscAdress, validatorPidStr, NULL);
      shutDownProgram();

    default:
      write(p[WRITE], (const char *) &automaton, sizeof(Automaton));
      close(p[WRITE]); close(p[READ]);
      break;
  }
  actRunProcesses++;
}

void addNewTester() {
  pid_t testerPid = atoi(mess);
  Tester* new = newTester(testerPid);

  sprintf(mess, "/TQ-%d", testerPid);
  new->desc = mq_open(mess, O_WRONLY | O_CLOEXEC);
  if (new->desc == (mqd_t) ERROR) {
    kill(testerPid, SIGKILL); mq_unlink(mess);
  }

  if (!acceptWordsToCheck) {
    if (mq_send(new->desc, mess, strlen(mess), END_WORK_PRIORITY) == ERROR) {
      kill(testerPid, SIGKILL); mq_unlink(mess);
    }
    mq_close(new->desc);
    free(new);
  } else {
    new->next = testers.next;
    testers.next = new;
  }
}

void passResultToTester() {
  actRunProcesses--;

  pid_t testerPid;
  sscanf(mess, "%d %s %s", &testerPid, buf, res);
  sprintf(mess, "%s %s", buf, res);
  Tester* dst = findTester(testerPid);
  if (!strcmp(res, ACCEPTED)) {
    dst->acc += 1; acc++;
  }
  snt++; dst->snt++;
  mq_send(dst->desc, mess, strlen(mess), CHECKED_WORD_PRIORITY);
  if (!acceptWordsToCheck && dst->rcd == dst->snt) {
    closeSingleTester(dst);
  } 
}

void stopRecivingNewWords() {
  if (!acceptWordsToCheck) {
    return;
  }
  informTesters(NO_NEW_WORDS_PIORITY);
  acceptWordsToCheck = false;
}

int receiveMessage() {
  int messLen = mq_receive(orders, mess, MSGSIZE, &msgPrio);
  if (messLen < 0) {
    shutDownProgram();
  }
  fixStringEncoding(mess, MSGSIZE, messLen);

  switch (msgPrio) {
    case ORDER_FROM_TESTER:
      sscanf(mess, "%*d %s", buf);
      if (!strcmp(buf, NO_NEW_WORDS)) {
        stopRecivingNewWords();
      } else {
        runRunProcess();
      }
      break;

    case NEW_TESTER_PRIORITY:
      addNewTester();
      break;        

    case RESULT_FROM_RUN:
      passResultToTester();
      break;

    default:
      shutDownProgram();
  }  
  return 0;
}


int main() {
  init();

  while (acceptWordsToCheck || actRunProcesses > 0) {
    receiveMessage();
  }

  printReport();

  clean();
  return 0;
}