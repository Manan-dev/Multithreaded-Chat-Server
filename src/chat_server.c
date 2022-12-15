/*
  Manan Patel
  CS360 -- Laba: main.c
  12/2/2022

  A chat server using pthreads that allows clients to chat with each other using
  nc (or jtelnet).
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dllist.h"
#include "jrb.h"
#include "sockettome.h"

typedef struct server {
  pthread_mutex_t sLock;
  pthread_cond_t sCond;
  char *name;
  Dllist messages;
  Dllist users;
} *Server;

typedef struct user {
  char *name;
  FILE *read, *write;
  Server server;
  int fileDescriptor;
} *User;

JRB serverTree;

/*
  @name: writeToStream
  @brief: writes a message to a stream
  @param[in]: message - message to write
  @param[in]: stream - stream to write to
  @param[in]: fd - file descriptor of socket
  @return: int - 0 on success, -1 on failure
*/
int writeToStream(char *message, FILE *stream, int *fd) {
  if (fputs(message, stream) == EOF) {
    close(*fd);
    pthread_exit(NULL);
  }
  return 0;
}

/*
  @name: readFromStream
  @brief: reads a message to a stream
  @param[in]: message - message to read
  @param[in]: stream - stream to read from
  @param[in]: fd - file descriptor of socket
  @return: int - 0 on success, -1 on failure
*/
int readFromStream(char message[1000], FILE *stream, int *fd) {
  if (fgets(message, 1000, stream) == NULL) {
    close(*fd);
    pthread_exit(NULL);
  }
  return 0;
}

/*
  @name: flushStream
  @brief: flushes a stream
  @param[in]: stream - stream to flush
  @param[in]: fd - file descriptor of socket
  @return: int - 0 on success, -1 on failure
*/
int flushStream(FILE *stream, int *fd) {
  if (fflush(stream) == EOF) {
    close(*fd);
    pthread_exit(NULL);
  }
  return 0;
}

/*
  @name: userWork
  @brief: thread function that handles user input and output
  @param[in]: fd - file descriptor of socket
  @return: void *
*/
void *userWork(void *fd) {
  int *fdptr;
  JRB tmp, serverNode;
  Server server;
  User user;
  Dllist activeUsers, del;
  FILE *in, *out;
  char messages[1000];
  char *output, *name, *serverName;
  char newUser[1000];
  char leftUser[1000];

  /* open files for reading and writing */
  fdptr = (int *)fd;
  in = fdopen(*fdptr, "r");
  if (in == NULL) {
    perror("file open for reading failed");
    exit(1);
  }
  out = fdopen(*fdptr, "w");
  if (out == NULL) {
    perror("file open for writing failed");
    exit(1);
  }

  writeToStream("Chat Rooms:\n\n", out, fdptr);

  /* print all rooms and users */
  jrb_traverse(tmp, serverTree) {
    writeToStream(tmp->key.s, out, fdptr);
    writeToStream(":", out, fdptr);
    dll_traverse(activeUsers, ((Server)tmp->val.v)->users) {
      writeToStream(" ", out, fdptr);
      writeToStream(((User)activeUsers->val.v)->name, out, fdptr);
    }
    writeToStream("\n", out, fdptr);
  }

  /* get new user's name */
  writeToStream("\nEnter your chat name (no spaces):\n", out, fdptr);
  flushStream(out, fdptr);
  readFromStream(messages, in, fdptr);
  name = malloc(strlen(messages));
  memcpy(name, messages, strlen(messages));
  name[strlen(messages) - 1] = '\0';

  /* get new user's room */
  writeToStream("Enter chat room:\n", out, fdptr);
  flushStream(out, fdptr);
  readFromStream(messages, in, fdptr);
  serverName = malloc(strlen(messages));
  memcpy(serverName, messages, strlen(messages));
  serverName[strlen(messages) - 1] = '\0';

  /* find user in tree */
  serverNode = jrb_find_str(serverTree, serverName);
  free(serverName);
  if (serverNode == NULL) {
    close(*fdptr);
    pthread_exit(NULL);
  }

  /* if user exists, add to room */
  server = serverNode->val.v;
  user = malloc(sizeof(struct user));
  user->name = name;
  user->read = in;
  user->write = out;
  user->fileDescriptor = *fdptr;
  user->server = server;

  pthread_mutex_lock(&server->sLock);
  dll_append(server->users, new_jval_v(user));
  del = server->users->blink;
  sprintf(newUser, "%s has joined\n", user->name);
  dll_append(server->messages, new_jval_s(strdup(newUser)));
  pthread_cond_signal(&server->sCond);
  pthread_mutex_unlock(&server->sLock);

  /* check for user input */
  while (1) {
    /* if user enters text */
    if (fgets(messages, 1000, in) != NULL) {
      output = malloc(strlen(name) + 3 + strlen(messages));
      strcpy(output, name);
      strcat(output, ": ");
      strcat(output, messages);
      pthread_mutex_lock(&server->sLock);
      dll_append(server->messages, new_jval_s(strdup(output)));
      pthread_cond_signal(&server->sCond);
      pthread_mutex_unlock(&server->sLock);
    } else {
      /* if user leaves chat */
      sprintf(leftUser, "%s has left\n", user->name);
      pthread_mutex_lock(&server->sLock);
      dll_append(server->messages, new_jval_s(strdup(leftUser)));
      close(*fdptr);
      dll_delete_node(del);
      pthread_cond_signal(&server->sCond);
      pthread_mutex_unlock(&server->sLock);
      pthread_exit(NULL);
    }
  }
}

/*
  @name: serverWork
  @brief: server thread that handles all the messages from the users
  @param[in]: s - server
  @return: void *
*/
void *serverWork(void *s) {
  Server server;
  Dllist tmp;

  server = (Server)s;

  /* wait for messages from users */
  while (1) {
    while (!dll_empty(server->messages)) {
      /* print all messages */
      for (tmp = server->users->flink; tmp != server->users;) {
        /* print message to all users */
        if (fputs(server->messages->flink->val.s, ((User)tmp->val.v)->write) ==
            EOF) {
          fclose(((User)tmp->val.v)->write);
          tmp = tmp->flink;
          dll_delete_node(tmp->blink);
        } else if (fflush(((User)(tmp->val.v))->write) == EOF) {
          fclose(((User)(tmp->val.v))->write);
          tmp = tmp->flink;
          dll_delete_node(tmp->blink);
        } else {
          tmp = tmp->flink;
        }
      }
      dll_delete_node(server->messages->flink);
    }
    pthread_cond_wait(&server->sCond, &server->sLock);
  }
}

/* Main Function */
int main(int argc, char *argv[]) {
  /* Error check cli arguments */
  if (argc <= 2) {
    fprintf(stderr, "Incorrect number of arguments\n");
    return 1;
  }

  pthread_t *thread;
  Server server;
  serverTree = make_jrb();

  /* spawn threads for each room */
  for (int i = 2; i < argc; i++) {
    server = malloc(sizeof(struct server));
    server->name = argv[i];
    server->messages = new_dllist();
    server->users = new_dllist();
    pthread_mutex_init(&server->sLock, NULL);
    pthread_cond_init(&server->sCond, NULL);
    thread = malloc(sizeof(pthread_t));
    pthread_create(thread, NULL, serverWork, server);
    jrb_insert_str(serverTree, argv[i], new_jval_v(server));
  }

  int fd, *fdptr;
  int sock = serve_socket(atoi(argv[1]));

  /* wait for users to join then make a thread */
  while (1) {
    fd = accept_connection(sock);
    fdptr = malloc(sizeof(int));
    *fdptr = fd;
    thread = malloc(sizeof(pthread_t));
    pthread_create(thread, NULL, userWork, fdptr);
  }
  return 0;
}