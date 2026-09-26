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

    /*-------- ncurses initialization and configurations :) -----------*/

    initscr(); 
    noecho();
    cbreak(); 

    int i;
    int rows, cols; 
    getmaxyx(stdscr,rows,cols);

    /* client-side communication buffers */
    
    char message[1000];
    char request[1024];

    /* ---------------------------------------------------------------- */

    /* ----------- socket initialization & configurations --------------*/

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
        mvprintw(rows/2,cols/2 - 10,"Connection Successful\n");
    
    else 
    {
        mvprintw(rows/2,cols/2 - 15,"[CLIENT] Failed to connect to broker\n");
        refresh();
        napms(1500);
        if (client_fd >= 0)
            close(client_fd);
        free(address);
        endwin();
        return 1;
    }

    mvprintw(rows/2 + 2,cols/2 - 10,"Loading Interface ...\n");

    if (send_request(client_fd, "CLIENT") < 0)
        mvprintw(rows/2 + 3,cols/2 - 10,"Handshake Failed... ◉_◉");

    /*---------------------------------------------------------------------*/

    /* ------------------- simulate loading -------------------------------*/
    
    refresh();
    napms(2000);  

    /* -------------------- thread creation -------------------------------*/
    
    struct NotifQueue* notif_queue = (struct NotifQueue*) calloc(1,sizeof(struct NotifQueue));
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

    /* --------------------------------------------------------------------*/
    
    /* ------------------------ main loop ---------------------------------*/

    while(1)
    {
        /* clearing window after each loop */
        
        erase();
        refresh();

        getmaxyx(stdscr,rows,cols);
        
        int height = rows * 0.8 ;
        int width = cols * 0.4;
        int offsety = rows / 10;
        int offsetx = cols / 10;
        int cmenuy = 0 + offsety; 
        int cmenux = 0 + offsetx;
        int notify = 0 + offsety;
        int notifx = cmenux + width + offsetx;
        

        WINDOW* notifications = newwin(height,width,notify,notifx);
        WINDOW* client_menu = newwin(height,width,cmenuy,cmenux);

        box(notifications,0,0);
        box(client_menu,0,0);

        /* -------------------- notifications rendering ------------------------*/

        mvwprintw(notifications,1,1,"Notification Window\n");
        
        for (i = 0 ; i < width - 2; i++)
            mvwprintw(notifications,2,1 + i,"_");

        pthread_mutex_lock(&notif_mutex);

        
        for (i = 0 ; i < height - 4 && i < notif_queue->count; i++) {
            int idx = (notif_queue->tail - 1 - i + 200) % 200; 
            mvwprintw(notifications, 3 + i, 2, "%s", notif_queue->notif[idx]);
        }
        
        pthread_mutex_unlock(&notif_mutex);
        
        wrefresh(notifications);

        /*--------------------- client menu rendering ---------------------------*/
        
        mvwprintw(client_menu,1,1,"Client Menu Window\n");
        for (i = 0 ; i < width - 2 ; i++)
            mvwprintw(client_menu,2,1 + i,"_");

        mvwprintw(client_menu,3,1,"Options\n");
        mvwprintw(client_menu,4,1,"1. Submit Job\n");
        mvwprintw(client_menu,5,1,"2. Get Job Status\n");
        mvwprintw(client_menu,6,1,"3. Quit application\n");

        wtimeout(client_menu,0);
        wmove(client_menu,7,2);

        int ch = 0;
        ch = wgetch(client_menu);

        if (ch == '1')
        {
            wtimeout(client_menu,-1);
            echo();

            mvwprintw(client_menu,8,1,"Enter job payload: ");
            wmove(client_menu, 8, 20); // moving the cursor to the end of the prompt
            wrefresh(client_menu);
            wgetnstr(client_menu,message, sizeof(message) - 1);

            message[strcspn(message, "\n")] = 0;    // strip newline
            snprintf(request, sizeof(request), "SUBMIT %s\n", message);
            
            noecho();
            wtimeout(client_menu,0);
        
            if (send_request(client_fd, request) == -1)
                notif_queue->disconnected = 1;
            memset(request, 0, sizeof(request));
            memset(message, 0, sizeof(message));

            mvwprintw(client_menu,9,1,"Request sent!");
            wrefresh(client_menu);
            napms(1500);
        }

        else if (ch == '2')
        {
            echo();
            wtimeout(client_menu,-1);

            mvwprintw(client_menu,8,1,"Enter job id: ");
            wmove(client_menu,8,20);
            wrefresh(client_menu);
            wgetnstr(client_menu,message, sizeof(message) - 1);

            message[strcspn(message, "\n")] = 0;    // strip newline            
            snprintf(request, sizeof(request), "STATUS %s\n", message);

            noecho();
            wtimeout(client_menu,0);
      
            if (send_request(client_fd, request) == -1)
                notif_queue->disconnected = 1;
            memset(request, 0, sizeof(request));
            memset(message, 0, sizeof(message));
      
            mvwprintw(client_menu,9,1,"Request sent!");
            wrefresh(client_menu);
            napms(1000);
        }

        else if (ch == '3')
        {
            napms(750);
            erase();
            mvprintw(rows/2,cols/2-10,"Closing client-side connection...\n");
            refresh();
            napms(2000);
            delwin(notifications);
            delwin(client_menu);
            break;
        }

        else if (ch != ERR)
        {
            mvwprintw(client_menu,8,1,"Invalid Entry\n");
            refresh();
            napms(1500);
        }

        /* -------------------------------------------------------------------------------------*/


        /* -------------------------------- free memory here -----------------------------------*/

        delwin(notifications);
        delwin(client_menu);

        napms(250); // save CPU cycles
    }
    
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
                             sizeof(notif_queue->notif[notif_queue->tail]), "%s", line);
                    notif_queue->tail = (notif_queue->tail + 1) % 200;
                    if (notif_queue->count < 200)
                        notif_queue->count++;
                    pthread_mutex_unlock(&notif_mutex);
                }
                line_length = 0;
                line_too_long = 0;
            } else if (!line_too_long) {
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
