# What is it?

A chat server using pthreads that allows clients to chat with each other using nc (or jtelnet). Written in C programming language.

# How to run?

```sh
git clone git@github.com:Manan-dev/Multithreaded-Chat-Server.git
```

## Command line instructions
```sh
usage: chat-server port Chat-Room-Names ...
```
Ex:
```sh
chat_server 8005 Football Bridge Politics
```

### Connect clients to this server
```sh
nc hydra3.eecs.utk.edu 8005
```

### And have fun communicating with friends!