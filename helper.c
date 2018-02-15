#include "helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Tester* newTester(pid_t pid) {
  Tester* new = malloc(sizeof(Tester));
  new->pid = pid;
  new->next = NULL;
  new->rcd = 0;
  new->snt = 0;
  new->acc = 0;
  new->closed = false;
  return new;
}

int loadAutomaton(Automaton* automaton) {
  int transitionsNo; 
  scanf("%d %d %d %d %d %d", &transitionsNo, &(automaton->A), &(automaton->Q), 
                             &(automaton->U), &(automaton->F), &(automaton->q));

  for (int i = 0; i < automaton->F; i++) {
    int tmp; 
    scanf("%d", &tmp);
    automaton->G[i] = tmp;
  }

  for (int i = 0; i < automaton->Q; i++) {
    for (int j = 0; j < ALPHABET_SIZE; j++) {
      automaton->T[i][j][0] = 0;
    }
  }

  transitionsNo -= 2;
  if (automaton->F > 0) {
    transitionsNo--;
  }

  for (int i = 0; i < transitionsNo; i++) {
    int src; char a; int dst; 
    scanf("%d %c", &src, &a);
    while ((dst = getchar()) != '\n' && dst != EOF) {
      if (dst == ' ') {
        continue;
      }
      int nextPos = ++(automaton->T[src][a - 'a'][0]);
      automaton->T[src][a - 'a'][nextPos] = dst - '0';
    }
  }
  return 0; 
} 

void fixStringEncoding(char* buf, int bufLen, int messLen) {
  buf[messLen < bufLen ? messLen : bufLen] = '\0';
}

int loadWordStdin(char* word) {
  char c; int pos = STARTING_POSITION;
  if ((c = getchar()) == EOF) {
    return EOF;
  }
  if (c == EOL) {
    word[pos] = EMPTY_CHAR; pos++;
    word[pos] = '\0';
    return 1;
  }
  word[pos] = c; pos++;
  while ((c = getchar()) != EOF && c != EOL) {
    word[pos] = c; pos++;
  }
  word[pos] = '\0';
  return 1;
}