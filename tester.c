/* TESTER
Queries in a form of words are sent by programs tester. A program tester in an 
infinite loop read words form the standard input stream and forwards them to the 
server. Every word is written in a single line and the line feed symbol \n does 
not belong to any of the words.

When the tester receives an answer from the server, he writes on the standard 
output stream the word he resecived answer for and the decision A if the word 
was accepted and N if not.

Ant teste terminates when the server terminates of when tester receives the 
EOF symbol, i.e. the end of file symbol. When the tester terminates it sends no 
new queries, waits for the remaining answers from the server, and writes on the 
standard outpu a report.

A Report of a tester consist of three lines
Snt: x\n
Rcd: y\n
Acc: z\n
wher x,y,z respectively are the numbers of: queries, received answers, and 
accepted words sent by this process. 


In my implementation, process tester divides into two processes. 
One of them is responsible for sending, and the other one for receiving messages.
They communicate using message queues. When receiver receives stop order 
from validator he informs sender to finish work. In case of any error, processes 
terminate. */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>       
#include <unistd.h>    
#include <sys/stat.h>   
#include <sys/types.h>
#include <sys/wait.h>     
#include <mqueue.h>
#include <signal.h>
#include <errno.h>
#include "helper.h"

/* states */
bool endWork = false;
bool endSending = false;

/* buffers */
char mess[MSGSIZE + 1];
char buf[MSGSIZE+ 1];
char res[RES_SIZE + 1];

/* message queues,
for communication between tester and validator */
mqd_t orders;
mqd_t results;
unsigned int msgPrio;

/* pipes,
for communication between sender and reciver */ 
int p[RDWR_SIZE];
pid_t senderPid;
pid_t parentPid;

/* report,
rcd: total number of received results
snt: total number of sent queries
acc: total number of accepted words */
int rcd = 0;
int snt = 0;
int acc = 0;


void printReport() {
  printf("Snt: %d\n", snt);
  printf("Rcd: %d\n", rcd);
  printf("Acc: %d\n", acc);
}

void clean() {
  mq_close(results); sprintf(buf, "/TQ-%d", getpid()); mq_unlink(buf);
  mq_close(orders);
}

void shutDownProgram() {
  fprintf(stderr, "TESTER(%d): ERROR %s\n", parentPid, strerror(errno));
  clean();
  exit(1);
}

void connectToOrdersQueue() {
  orders = mq_open(ORDERS, O_WRONLY);
  if (orders == (mqd_t) ERROR) {
    shutDownProgram();
  }
}

void createResultsQueue() {
  struct mq_attr mqA; mqA.mq_maxmsg = MAXMSG; mqA.mq_msgsize = MSGSIZE;
  sprintf(mess, "/TQ-%d", getpid());
  
  results = mq_open(mess, O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR, &mqA);
  if (results == (mqd_t) ERROR) {
    shutDownProgram();
  }

  sprintf(mess, "%d", getpid());
  if (mq_send(orders, mess, strlen(mess), NEW_TESTER_PRIORITY) == (mqd_t) ERROR) {
    shutDownProgram();
  }
}

void init() {
  parentPid = getpid();
  connectToOrdersQueue();
  createResultsQueue();
  printf("PID: %d\n", getpid());
}

void receiveCheckedWord() {
  sscanf(mess, "%s %s", buf, res);
  if (!strcmp(res, ACCEPTED)) {
    acc++;
  }
  rcd++;
  if (!strcmp(buf, EMPTY_WORD)) {
    printf(" %s\n", res);
  } else {
    printf("%s\n", mess);
  }
}

int receiveMessage() {
  int messLen = mq_receive(results, mess, MSGSIZE, &msgPrio);
  if (messLen < 0) {
    shutDownProgram();
  }
  fixStringEncoding(mess, MSGSIZE, messLen);

  switch (msgPrio) {
    case CHECKED_WORD_PRIORITY:
      receiveCheckedWord();
      break;

    case NO_NEW_WORDS_PIORITY:
      kill(senderPid, END_SENDERS_WORK);
      read(p[READ], &snt, sizeof(snt));
      endSending = true;
      break;

    case END_WORK_PRIORITY:  
      kill(senderPid, END_SENDERS_WORK);
      endWork = true;
      break;      
  }   
  return 0;
}

void receiverWork() {
  close(p[WRITE]);

  while (!endWork && (!endSending || snt > rcd)) {
    receiveMessage();
  }

  close(p[READ]);
}

void senderEnd() {
  write(p[WRITE], &snt, sizeof(snt));
  close(p[WRITE]);
  exit(0);
}

void sendMessage() {
  sprintf(mess, "%d %s", parentPid, buf);

  if (mq_send(orders, mess, strlen(mess), ORDER_FROM_TESTER) == ERROR) {
    senderEnd();
  }

  if (!strcmp(buf, NO_NEW_WORDS)) {
    endSending = true;
  } else {
    snt++;
  }
}

void senderWork() {
  close(p[READ]);

  sigset_t blockMask;
  sigemptyset(&blockMask);
  struct sigaction action;
  action.sa_handler = senderEnd;
  action.sa_mask = blockMask;
  action.sa_flags = 0;
  sigaction(END_SENDERS_WORK, &action, 0);


  while (!endSending) {
    if (loadWordStdin(buf) == EOF) {
      endSending = true;
      break;
    }
    sendMessage();
  }


  sigaddset(&blockMask, END_SENDERS_WORK);
  sigprocmask(SIG_BLOCK, &blockMask, 0);
  senderEnd();
}

int main() {
  init();

  pipe(p);
  switch (senderPid = fork()) {
    case ERROR:
      shutDownProgram();

    case 0:
      senderWork();
      shutDownProgram();
      break;

    default:
      receiverWork();
      wait(0);
      break;
  }

  printReport();

  clean();
  return 0;
}