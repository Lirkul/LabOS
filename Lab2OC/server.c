#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8080
#define MAX_CONNECTIONS 5
#define BUFFER_SIZE 1024

volatile sig_atomic_t wasSigHup = 0; 

void sigHupHandler(int sig) {
    wasSigHup = 1; 
}

int main() {
    int server_fd, client_fd = -1;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    fd_set fds;
    int max_fd;
    char buffer[BUFFER_SIZE];
    sigset_t blockedMask, origMask;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    struct sigaction sa;
    sa.sa_handler = sigHupHandler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        perror("sigaction");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    if (sigprocmask(SIG_BLOCK, &blockedMask, &origMask) < 0) {
        perror("sigprocmask");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        FD_ZERO(&fds);
        FD_SET(server_fd, &fds);
        max_fd = server_fd;

        if (client_fd != -1) {
            FD_SET(client_fd, &fds);
            if (client_fd > max_fd) max_fd = client_fd;
        }

        int res = pselect(max_fd + 1, &fds, NULL, NULL, NULL, &origMask);
        if (res < 0) {
            if (errno == EINTR) continue; 
            perror("pselect");
            break;
        }

       
        if (FD_ISSET(server_fd, &fds)) {
            int new_client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
            if (new_client_fd < 0) {
                perror("accept");
                continue;
            }

            printf("New connection accepted\n");
            if (client_fd == -1) {
                client_fd = new_client_fd; 
            } else {
                printf("Closing additional connection\n");
                close(new_client_fd); 
            }
        }

    
        if (client_fd != -1 && FD_ISSET(client_fd, &fds)) {
            int bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);
            if (bytes_read <= 0) {
                if (bytes_read < 0) perror("recv");
                close(client_fd);
                client_fd = -1;
            } else {
                printf("Received %d bytes\n", bytes_read);
            }
        }

        if (wasSigHup) {
            printf("SIGHUP received\n");
            wasSigHup = 0; 
        }
    }

    close(server_fd);
    if (client_fd != -1) close(client_fd);
    return 0;
}
