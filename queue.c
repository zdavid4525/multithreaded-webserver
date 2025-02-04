#include "queue.h"
#include <stdlib.h>

node_t* head = NULL;
node_t* tail = NULL;

void enqueue(int * client_socket) {
  node_t* nn=malloc(sizeof(node_t));
  nn->client_socket=client_socket;
  nn->next=NULL;
  if (tail==NULL) {
    head=nn;
  } else {
    tail->next=nn;
  }
  tail=nn;
}

int* dequeue() {
  if (head==NULL) return NULL;
  int * cs = head->client_socket;
  node_t* tmp=head;
  head=head->next;
  if (head==NULL) tail=NULL;
  free(tmp);
  return cs;
}