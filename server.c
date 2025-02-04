/**
 * One thread per connection approach:
 *  One thread per connection will be bad if there are many threads - each thread takes memory and many will induce thrashing.
 *  Most time spent on switching between threads/creating threads than performing useful work.
 *  Instead, reuse threads w thread pool.
 * 
 * Thread pool:
 *  Bound number of threads running on server.
 *  Save time and memory while maintaining concurrency.
 *  Requires Q - shared memory. Need synchronization primitive (lock) to protect accesses.
 *  PROBLEM: worker threads HOG CPU - spinning until job arrives. Steals resources from other jobs running on machine.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <stdbool.h> 
#include <limits.h> /* PATH_MAX */
#include <pthread.h>
#include "queue.h"

#define SERVERPORT 8080 
#define BUFSIZE 4096 
#define SERVER_BACKLOG 100  // queues up to 100 incoming connections before failing.
#define THREAD_POOL_SIZE 20  // depends on available memory on machine, server workload - experimental

pthread_t tpool[THREAD_POOL_SIZE];  // shared mem
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

typedef struct sockaddr_in SA_IN; 
typedef struct sockaddr SA;

void* thread_job(void * arg);
void* handle_connection(void * client_socket_ptr); 
int check(int exp, const char * msg); 

// follows most TCP server socket flows
int main(int argc, char** argv) {
  int server_sock, client_sock, addr_size; 
  SA_IN server_addr, client_addr; 

  for (int i = 0; i < THREAD_POOL_SIZE; i++) {
    pthread_create(&tpool[i], NULL, thread_job, NULL);
  }

  check((server_sock=socket(AF_INET,SOCK_STREAM,0)), "Socket creation failed"); // CREATE SOCKET. SOCK_STREAM <==> TCP
  /**
   * TCP almost always uses SOCK_STREAM and UDP uses SOCK_DGRAM.
   * TCP (SOCK_STREAM) is a connection-based protocol. The connection is established and the two parties 
   * have a conversation until the connection is terminated by one of the parties or by a network error.
   * 
   * UDP (SOCK_DGRAM) is a datagram-based protocol. You send one datagram and get one reply and then the connection terminates.
   *  If you send multiple packets, TCP promises to deliver them in order. UDP does not, so the receiver needs to check them, 
   *    if the order matters.
   *  If a TCP packet is lost, the sender can tell. Not so for UDP.
   *  UDP datagrams are limited in size. TCP can send much bigger lumps than that.
   *  TCP is a bit more robust and makes more checks. UDP is a shade lighter weight (less computer and network stress).
  */

  server_addr.sin_family = AF_INET;  // socket supports ADRESS FAMILY IPv4 - most commonly used
  server_addr.sin_addr.s_addr = INADDR_ANY;  // accept connections that come to any IP address on this host
  server_addr.sin_port = htons(SERVERPORT);  // port that the server is listening on

  check(bind(server_sock, (SA*) &server_addr, sizeof(server_addr)), "Socket bind failed");  // BIND SOCKET
  check(listen(server_sock, 1), "Listen failed");                                     // SERVER LISTENS ON PORT

  while (true) {
    // MULTITHREADING: each connection gets its own thread
    printf("Waiting for connections\n");
    addr_size = sizeof(SA_IN);
    check(client_sock=accept(server_sock, (SA*) &client_addr, (socklen_t*) &addr_size), "Failed to accept client connection");  // accept connections
    printf("connected\n");

    // enqueue pending connections for free worker threads.
    int* tclient = malloc(sizeof(int));
    *tclient=client_sock;

    pthread_mutex_lock(&mtx);
    enqueue(tclient);
    pthread_mutex_unlock(&mtx);

    /* old code, using 1 thread/connection method.
    // int* tclient = malloc(sizeof(int));
    // *tclient=client_sock;
    // pthread_t t;
    // pthread_create(&t, NULL, handle_connection, tclient);

    // if handle_connection called without pthread, all times will be roughly similar
    // on client.rb with reads to 6.c bloating average time (reads to 1-5.c need to wait for 6.c to finish before starting).
    // handle_connection(tclient);
    */
  }

  return 0;
}

int check(int r, const char * msg) {
  if (r < 0) {
    perror(msg);
    exit(1);
  }
  return r;
}

// client sends file name, server reads requested file and sends contents back.
// similar to web server, except HTTP parsing. 
// SECURITY PROBLEM: allows client to read any file from hard drive. solution: containerize w/ docker.

// accepts connection, handles connection. accepts new connection, handles new connection. repeat <= 1 conn at a time.
void* handle_connection(void* client_socket_ptr) {
  int client_socket = *(int*)client_socket_ptr;
  free(client_socket_ptr);

  char buf[BUFSIZE];
  size_t bytes_read;
  int msgsize = 0;
  char actualpath[PATH_MAX];

  while ((bytes_read=read(client_socket, buf+msgsize, sizeof(buf)-msgsize-1)) > 0) {
    msgsize+=bytes_read;
    if (msgsize > BUFSIZE-1 || buf[msgsize-1]=='\n') break;
  }
  check(bytes_read, "recv error");
  buf[msgsize-1]=0;  // remove '\n'

  printf("REQUEST: %s\n", buf);
  fflush(stdout);

  if (realpath(buf, actualpath) == NULL) {  // resolves file path, saves in actualpath
    printf("bad path: %s\n", buf);
    close(client_socket);
    return NULL;
  }

  FILE* fp = fopen(actualpath, "r");
  if (fp == NULL) {
    printf("open error: %s\n", buf);
    close(client_socket);
    return NULL;
  }

  // read file and send contents to client.
  while ((bytes_read = fread(buf, 1, BUFSIZE, fp))>0) {
    printf("sending %zu bytes\n", bytes_read);
    write(client_socket, buf, bytes_read);
  }
  close (client_socket);
  fclose(fp);
  printf("closing connection\n");
  return NULL;
}

void * thread_job(void * arg) {
  while (true) {  // threads never die
    pthread_mutex_lock(&mtx);
    int * tclient = dequeue();
    pthread_mutex_unlock(&mtx);
    if (tclient != NULL) {
      // we have a connection - current thread handles it
      handle_connection(tclient);
    }
  }
}

/* TESTING with client.rb:
 - time ruby client.rb 1 >/dev/null # pipe avoids print to stdout - saves time
 - time ./manyconnections.sh
*/
