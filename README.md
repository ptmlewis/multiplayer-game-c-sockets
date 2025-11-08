# 🎮 Multiplayer Turn-Based Game (C Sockets)

## Overview
Multi-client networked game using TCP sockets and threads. Players take turns performing moves with timeouts and validations.

## Features
- Server manages shared game state and turn logic
- Timeout and invalid-move disqualification
- Real-time feedback via clients

## Run
```bash
gcc game_server.c -lpthread -o server
gcc game_client.c -o client
./server 8080 game 3
./client game 127.0.0.1 8080
```

## Skills
C · Networking · Multithreading · Synchronization
