#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ncurses.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include "../util/socket.h"

pthread_mutex_t job_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t notif_mutex = PTHREAD_MUTEX_INITIALIZER;

struct NotifQueue {
    char notif[200][2048];
    int tail;
    int count;
    int client_fd;
    volatile sig_atomic_t disconnected;
};

int known_jobs[100];
int num_jobs = 0;

void* notif_listener(void* arg);

static int send_request(int fd, const char* request)
{
    size_t sent = 0;
    size_t length = strlen(request);

    while (sent < length) {
        ssize_t result = send(fd, request + sent, length - sent, MSG_NOSIGNAL);

        if (result < 0 && errno == EINTR)
            continue;

        if (result <= 0)
            return -1;

        sent += (size_t)result;
    }

    return 0;
}

int main(int argc, char** argv)
{
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(0);

    int i;
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    char message[1000];
    char request[1024];

    char ip[20] = "127.0.0.1";
    int port_num = 2000;

    if (argc >= 3) {
        snprintf(ip, sizeof(ip), "%s", argv[1]);
        port_num = atoi(argv[2]);
    }

    int client_fd = createTCPIpv4Socket();
    struct sockaddr* address = createTCPIpv4SocketAddress(ip, port_num);

    if (client_fd >= 0 && address != NULL &&
        connect(client_fd, address, sizeof(*address)) == 0)
        mvprintw(rows / 2, cols / 2 - 10, "Connection Successful");

    else
    {
        mvprintw(rows / 2, cols / 2 - 15, "[CLIENT] Failed to connect to broker");
        refresh();
        napms(1500);

        if (client_fd >= 0)
            close(client_fd);

        free(address);
        endwin();
        return 1;
    }

    mvprintw(rows / 2 + 2, cols / 2 - 10, "Loading Interface ...");

    if (send_request(client_fd, "CLIENT") < 0)
        mvprintw(rows / 2 + 3, cols / 2 - 10, "Handshake Failed... (connection lost)");

    refresh();
    napms(2000);

    struct NotifQueue* notif_queue =
        (struct NotifQueue*) calloc(1, sizeof(struct NotifQueue));

    notif_queue->tail = 0;
    notif_queue->count = 0;
    notif_queue->client_fd = client_fd;
    notif_queue->disconnected = 0;

    pthread_t notif_handler;

    if (pthread_create(&notif_handler, NULL, notif_listener, notif_queue) != 0) {
        free(notif_queue);
        close(client_fd);
        free(address);
        endwin();
        return 1;
    }

    WINDOW* notifications = NULL;
    WINDOW* client_menu = NULL;

    int window_rows = 0;
    int window_cols = 0;

    while (1)
    {
        getmaxyx(stdscr, rows, cols);

        if (rows != window_rows || cols != window_cols) {
            if (notifications != NULL)
                delwin(notifications);

            if (client_menu != NULL)
                delwin(client_menu);

            notifications = NULL;
            client_menu = NULL;

            window_rows = rows;
            window_cols = cols;

            if (rows >= 14 && cols >= 60) {
                int height = rows - 2;
                int width = (cols - 3) / 2;
                int y = 1;
                int menu_x = 1;
                int notifications_x = menu_x + width + 1;

                client_menu = newwin(height, width, y, menu_x);
                notifications = newwin(height, width, y, notifications_x);

                if (client_menu == NULL || notifications == NULL) {
                    if (client_menu != NULL)
                        delwin(client_menu);

                    if (notifications != NULL)
                        delwin(notifications);

                    client_menu = NULL;
                    notifications = NULL;
                } else {
                    keypad(client_menu, TRUE);
                }
            }
        }

        if (client_menu == NULL || notifications == NULL) {
            werase(stdscr);
            mvwprintw(stdscr, 1, 1,
                      "Resize terminal to at least 60 columns x 14 rows.");
            wnoutrefresh(stdscr);
            doupdate();
            napms(250);
            continue;
        }

        int height, width;
        getmaxyx(client_menu, height, width);

        werase(stdscr);
        werase(notifications);
        werase(client_menu);

        box(notifications, 0, 0);
        box(client_menu, 0, 0);

        mvwprintw(notifications, 1, 1, "Notification Window");
        mvwhline(notifications, 2, 1, ACS_HLINE, width - 2);

        pthread_mutex_lock(&notif_mutex);

        for (i = 0; i < height - 4 && i < notif_queue->count; i++) {
            int idx = (notif_queue->tail - 1 - i + 200) % 200;

            mvwprintw(notifications, 3 + i, 2, "%.*s",
                      width - 4, notif_queue->notif[idx]);
        }

        pthread_mutex_unlock(&notif_mutex);

        mvwprintw(client_menu, 1, 1, "Client Menu Window");
        mvwhline(client_menu, 2, 1, ACS_HLINE, width - 2);

        mvwprintw(client_menu, 3, 1, "Options");
        mvwprintw(client_menu, 4, 1, "1. Submit Job");
        mvwprintw(client_menu, 5, 1, "2. Get Job Status");
        mvwprintw(client_menu, 6, 1, "3. Quit application");

        wtimeout(client_menu, 0);
        wmove(client_menu, 7, 2);

        wnoutrefresh(stdscr);
        wnoutrefresh(notifications);
        wnoutrefresh(client_menu);
        doupdate();

        int ch = 0;
        ch = wgetch(client_menu);

        if (ch == '1')
        {
            wtimeout(client_menu, -1);
            echo();
            curs_set(1);

            mvwprintw(client_menu, 8, 1, "Enter job payload: ");
            wmove(client_menu, 8, 20);
            wrefresh(client_menu);

            int input_limit = width - 22;

            if (input_limit > (int)sizeof(message) - 1)
                input_limit = (int)sizeof(message) - 1;

            if (input_limit < 1)
                input_limit = 1;

            wgetnstr(client_menu, message, input_limit);

            message[strcspn(message, "\n")] = 0;
            snprintf(request, sizeof(request), "SUBMIT %s\n", message);

            noecho();
            curs_set(0);
            wtimeout(client_menu, 0);

            if (send_request(client_fd, request) == -1)
                notif_queue->disconnected = 1;

            memset(request, 0, sizeof(request));
            memset(message, 0, sizeof(message));

            mvwprintw(client_menu, 9, 1, "Request sent!");
            wnoutrefresh(client_menu);
            doupdate();
            napms(1500);
        }

        else if (ch == '2')
        {
            echo();
            wtimeout(client_menu, -1);
            curs_set(1);

            mvwprintw(client_menu, 8, 1, "Enter job id: ");
            wmove(client_menu, 8, 20);
            wrefresh(client_menu);

            int input_limit = width - 22;

            if (input_limit > (int)sizeof(message) - 1)
                input_limit = (int)sizeof(message) - 1;

            if (input_limit < 1)
                input_limit = 1;

            wgetnstr(client_menu, message, input_limit);

            message[strcspn(message, "\n")] = 0;
            snprintf(request, sizeof(request), "STATUS %s\n", message);

            noecho();
            curs_set(0);
            wtimeout(client_menu, 0);

            if (send_request(client_fd, request) == -1)
                notif_queue->disconnected = 1;

            memset(request, 0, sizeof(request));
            memset(message, 0, sizeof(message));

            mvwprintw(client_menu, 9, 1, "Request sent!");
            wnoutrefresh(client_menu);
            doupdate();
            napms(1000);
        }

        else if (ch == '3')
        {
            mvwprintw(client_menu, 8, 1, "Closing connection...");
            wnoutrefresh(client_menu);
            doupdate();
            napms(500);
            break;
        }

        else if (ch != ERR)
        {
            mvwprintw(client_menu, 8, 1, "Invalid Entry");
            wnoutrefresh(client_menu);
            doupdate();
            napms(1500);
        }

        napms(250);
    }

    if (notifications != NULL)
        delwin(notifications);

    if (client_menu != NULL)
        delwin(client_menu);

    free(address);

    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);

    pthread_join(notif_handler, NULL);

    free(notif_queue);

    endwin();

    return 0;
}

void* notif_listener(void* arg)
{
    struct NotifQueue* notif_queue = (struct NotifQueue*) arg;
    int fd = notif_queue->client_fd;

    char input[1024];
    char line[2048];
    size_t line_length = 0;
    int line_too_long = 0;

    for (;;) {
        ssize_t bytes_read = read(fd, input, sizeof(input));

        if (bytes_read < 0 && errno == EINTR)
            continue;

        if (bytes_read <= 0)
            break;

        for (ssize_t i = 0; i < bytes_read; i++) {
            char ch = input[i];

            if (ch == '\n') {
                if (!line_too_long) {
                    if (line_length > 0 && line[line_length - 1] == '\r')
                        line_length--;

                    line[line_length] = '\0';

                    pthread_mutex_lock(&notif_mutex);

                    snprintf(notif_queue->notif[notif_queue->tail],
                             sizeof(notif_queue->notif[notif_queue->tail]),
                             "%s",
                             line);

                    notif_queue->tail = (notif_queue->tail + 1) % 200;

                    if (notif_queue->count < 200)
                        notif_queue->count++;

                    pthread_mutex_unlock(&notif_mutex);
                }

                line_length = 0;
                line_too_long = 0;
            }

            else if (!line_too_long) {
                if (line_length + 1 < sizeof(line))
                    line[line_length++] = ch;
                else
                    line_too_long = 1;
            }
        }
    }

    notif_queue->disconnected = 1;

    return NULL;
}

