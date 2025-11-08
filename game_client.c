#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <Game Type> <Server Name> <Port Number>\n", argv[0]);
        exit(1);
    }

    char *server_name = argv[2];
    int port = atoi(argv[3]);

    int sock;
    struct sockaddr_in server;
    char server_reply[BUF_SIZE];
    char message[BUF_SIZE];
    char formatted_message[BUF_SIZE];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("Could not create socket\n");
        return 1;
    }

    server.sin_addr.s_addr = inet_addr(server_name);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Connect failed");
        return 1;
    }

    while (1) {
        memset(server_reply, 0, BUF_SIZE);
        if (recv(sock, server_reply, BUF_SIZE, 0) > 0) {
            if (strncmp(server_reply, "TEXT", 4) == 0) {
                printf("%s\n", server_reply + 5);
            } else {
                printf("%s\n", server_reply);
            }

            if (strcmp(server_reply, "GO\n") == 0) {
                printf("It's your turn!\n");
                while (1) {
                    printf("Enter your move (number 1-9) or type 'QUIT' to exit: ");
                    fgets(message, BUF_SIZE, stdin);
                    message[strcspn(message, "\n")] = '\0';

                    if (strcmp(message, "quit") == 0) {
                        strcpy(formatted_message, "QUIT");
                    } else {
                        snprintf(formatted_message, BUF_SIZE, "MOVE %s", message);
                    }

                    if (send(sock, formatted_message, strlen(formatted_message), 0) < 0) {
                        printf("Send failed\n");
                        return 1;
                    }

                    if (strcmp(message, "quit") == 0) {
                        printf("You have left the game.\n");
                        break;
                    }

                    memset(server_reply, 0, BUF_SIZE);
                    if (recv(sock, server_reply, BUF_SIZE, 0) > 0) {
                        if (strncmp(server_reply, "TEXT", 4) == 0) {
                            printf("%s\n", server_reply + 5);
                        } else {
                            printf("%s\n", server_reply);
                        }

                        if (strstr(server_reply, "ERROR") != NULL) {
                            printf("There was an error. Please try again.\n");
                        } else {
                            break;
                        }
                    }
                }
            }
        } else {
            printf("recv failed\n");
            break;
        }

        if (strstr(server_reply, "You win!") != NULL || strstr(server_reply, "END") != NULL) {
            printf("Game over! %s\n", server_reply);
            break;
        }
    }

    close(sock);
    return 0;
}
