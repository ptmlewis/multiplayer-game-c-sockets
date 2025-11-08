#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/select.h>

#define MAX_PLAYERS 10
#define BUF_SIZE 1024
#define MAX_ERRORS 5
#define TIMEOUT 20

typedef struct {
    int sock;
    int player_id;
    int active;
    int error_count;
} Player;

Player players[MAX_PLAYERS];
int num_connected = 0;
int game_total = 25;
int current_player = 0;
pthread_mutex_t lock;

int count_active_players() {
    int count = 0;
    for (int i = 0; i < num_connected; i++) {
        if (players[i].active) {
            count++;
        }
    }
    return count;
}

int find_last_active_player() {
    for (int i = 0; i < num_connected; i++) {
        if (players[i].active) {
            return i;
        }
    }
    return -1;
}

void *handle_player(void *arg) {
    Player *player = (Player *)arg;
    char buffer[BUF_SIZE];
    int read_size;

    sprintf(buffer, "TEXT Welcome to the game, Player %d\n", player->player_id);
    send(player->sock, buffer, strlen(buffer), 0);

    while (1) {
        memset(buffer, 0, BUF_SIZE);

        fd_set read_fds;
        struct timeval tv;
        FD_ZERO(&read_fds);
        FD_SET(player->sock, &read_fds);

        tv.tv_sec = TIMEOUT;
        tv.tv_usec = 0;

        int activity = select(player->sock + 1, &read_fds, NULL, NULL, &tv);
        if (activity == 0) {
            sprintf(buffer, "END You have been removed from the game due to inactivity.\n");
            send(player->sock, buffer, strlen(buffer), 0);
            player->active = 0;
            close(player->sock);

            int active_players = count_active_players();
            if (active_players == 1) {
                int last_player = find_last_active_player();
                sprintf(buffer, "TEXT You are the last player remaining. You win!\nEND\n");
                send(players[last_player].sock, buffer, strlen(buffer), 0);
                close(players[last_player].sock);
                pthread_mutex_unlock(&lock);
                exit(0);
            }

            pthread_mutex_unlock(&lock);
            return NULL;
        } else if (activity < 0) {
            perror("select error");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(player->sock, &read_fds)) {
            read_size = recv(player->sock, buffer, BUF_SIZE, 0);
            if (read_size <= 0) {
                player->active = 0;
                close(player->sock);
                pthread_mutex_unlock(&lock);
                return NULL;
            }

            buffer[read_size] = '\0';
            pthread_mutex_lock(&lock);

            if (player->player_id == current_player + 1) {
                if (strncmp(buffer, "MOVE", 4) == 0) {
                    int move = atoi(buffer + 5);

                    if (move >= 1 && move <= 9) {
                        player->error_count = 0;
                        game_total -= move;

                        sprintf(buffer, "TEXT Move accepted. Current total: %d\n", game_total);
                        send(player->sock, buffer, strlen(buffer), 0);

                        if (game_total <= 0) {
                            sprintf(buffer, "TEXT You win!\nEND\n");
                            send(player->sock, buffer, strlen(buffer), 0);

                            for (int i = 0; i < num_connected; i++) {
                                if (i != current_player && players[i].active) {
                                    sprintf(buffer, "TEXT You lost! Game over.\nEND\n");
                                    send(players[i].sock, buffer, strlen(buffer), 0);
                                }
                            }

                            for (int i = 0; i < num_connected; i++) {
                                close(players[i].sock);
                            }

                            player->active = 0;
                            pthread_mutex_unlock(&lock);
                            exit(0);
                        }

                        current_player = (current_player + 1) % num_connected;

                        sprintf(buffer, "GO\n");
                        send(players[current_player].sock, buffer, strlen(buffer), 0);
                    } else {
                        player->error_count++;
                        if (player->error_count >= MAX_ERRORS) {
                            sprintf(buffer, "END You have been removed from the game due to repeated invalid moves.\n");
                            send(player->sock, buffer, strlen(buffer), 0);
                            player->active = 0;
                            close(player->sock);

                            int active_players = count_active_players();
                            if (active_players == 1) {
                                int last_player = find_last_active_player();
                                sprintf(buffer, "TEXT You are the last player remaining. You win!\nEND\n");
                                send(players[last_player].sock, buffer, strlen(buffer), 0);
                                close(players[last_player].sock);
                                pthread_mutex_unlock(&lock);
                                exit(0);
                            }

                            pthread_mutex_unlock(&lock);
                            return NULL;
                        } else {
                            sprintf(buffer, "TEXT ERROR Invalid move. Enter a number between 1 and 9. (%d/%d errors)\nGO\n", player->error_count, MAX_ERRORS);
                            send(player->sock, buffer, strlen(buffer), 0);
                        }
                    }
                } else if (strncmp(buffer, "QUIT", 4) == 0) {
                    sprintf(buffer, "END You have quit the game.\n");
                    send(player->sock, buffer, strlen(buffer), 0);
                    player->active = 0;
                    close(player->sock);

                    int active_players = count_active_players();
                    if (active_players == 1) {
                        int last_player = find_last_active_player();
                        sprintf(buffer, "TEXT You are the last player remaining. You win!\nEND\n");
                        send(players[last_player].sock, buffer, strlen(buffer), 0);
                        close(players[last_player].sock);
                        pthread_mutex_unlock(&lock);
                        exit(0);
                    }

                    pthread_mutex_unlock(&lock);
                    return NULL;
                }
            } else {
                sprintf(buffer, "ERROR It's not your turn.\n");
                send(player->sock, buffer, strlen(buffer), 0);
            }

            pthread_mutex_unlock(&lock);
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <Port Number> <Game Type> <Max Players>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    int max_players = atoi(argv[3]);
    pthread_t threads[MAX_PLAYERS];

    int server_sock, new_sock;
    struct sockaddr_in server, client;
    socklen_t client_size = sizeof(client);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Could not create socket");
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Bind failed");
        return 1;
    }

    listen(server_sock, max_players);

    while (num_connected < max_players) {
        new_sock = accept(server_sock, (struct sockaddr *)&client, &client_size);
        if (new_sock < 0) {
            perror("Accept failed");
            return 1;
        }

        players[num_connected].sock = new_sock;
        players[num_connected].player_id = num_connected + 1;
        players[num_connected].active = 1;
        players[num_connected].error_count = 0;

        pthread_create(&threads[num_connected], NULL, handle_player, &players[num_connected]);
        num_connected++;

        if (num_connected == max_players) {
            send(players[0].sock, "GO\n", 3, 0);
        }
    }

    for (int i = 0; i < max_players; i++) {
        pthread_join(threads[i], NULL);
    }

    close(server_sock);
    return 0;
}
