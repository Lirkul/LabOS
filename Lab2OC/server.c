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
sigset_t origMask;

void sigHupHandler(int sig) {
    wasSigHup = 1;
}

int setup_server_socket(int port) {
    int server_fd;
    struct sockaddr_in server_addr;
    int opt = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    return server_fd;
}

void setup_signal_handler() {
    struct sigaction sa;
    sigset_t blockedMask;

    sa.sa_handler = sigHupHandler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);

    if (sigprocmask(SIG_BLOCK, &blockedMask, &origMask) < 0) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }
}

int accept_connection(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);

    if (client_fd < 0) {
        perror("accept");
        return -1;
    }

    printf("New connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    return client_fd;
}

ssize_t read_from_client(int client_fd, char *buffer, size_t size) {
    ssize_t bytes_read = recv(client_fd, buffer, size, 0);
    if (bytes_read <= 0) {
        if (bytes_read < 0)
            perror("recv");
        printf("Client disconnected\n");
    } else {
        printf("Received %zd bytes\n", bytes_read);
    }
    return bytes_read;
}

void handle_sighup() {
    if (wasSigHup) {
        printf("SIGHUP received\n");
        wasSigHup = 0;
    }
}

void cleanup(int server_fd, int client_fd) {
    close(server_fd);
    if (client_fd != -1)
        close(client_fd);
}

int wait_for_events(int server_fd, int client_fd, sigset_t *original_mask, fd_set *read_fds, int *max_fd) {
    FD_ZERO(read_fds);
    FD_SET(server_fd, read_fds);
    *max_fd = server_fd;

    if (client_fd != -1) {
        FD_SET(client_fd, read_fds);
        if (client_fd > *max_fd)
            *max_fd = client_fd;
    }

    return pselect(*max_fd + 1, read_fds, NULL, NULL, NULL, original_mask);
}

int main() {
    int server_fd, client_fd = -1;
    fd_set read_fds;
    int max_fd;
    char buffer[BUFFER_SIZE];

    server_fd = setup_server_socket(PORT);
    setup_signal_handler();

    while (1) {
        int res = wait_for_events(server_fd, client_fd, &origMask, &read_fds, &max_fd);
        if (res < 0) {
            if (errno == EINTR)
                handle_sighup();
                continue;
            perror("pselect");
            break;
        }

        if (FD_ISSET(server_fd, &read_fds)) {
            if (client_fd == -1) {
                client_fd = accept_connection(server_fd);
            } else {
                int new_client_fd = accept(server_fd, NULL, NULL);
                if (new_client_fd < 0) {
                    perror("accept");
                } else {
                    printf("Closing additional connection\n");
                    close(new_client_fd);
                }
            }
        }

        if (client_fd != -1 && FD_ISSET(client_fd, &read_fds)) {
            ssize_t bytes_read = read_from_client(client_fd, buffer, BUFFER_SIZE);
            if (bytes_read <= 0) {
                close(client_fd);
                client_fd = -1;
            }
        }

        
    }

    cleanup(server_fd, client_fd);
    return 0;
}
