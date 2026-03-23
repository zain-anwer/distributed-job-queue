#include "dashboard_handler.h"

void* dashboard(void* arg) {

    initscr();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);    
    start_color();
    use_default_colors();

    init_pair(1, COLOR_GREEN,  -1);  // idle workers / completed
    init_pair(2, COLOR_YELLOW, -1);  // busy workers
    init_pair(3, COLOR_RED,    -1);  // offline workers / failed jobs
    init_pair(4, COLOR_CYAN,   -1);  // headers

    while (1) {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);

        int content_width = cols / 2;
        int cx = (cols - content_width) / 2;

        clear();
        int r = rows/4;

        // ================= RECENT LOGS (TOP) =================
        #define MAX_VISIBLE_LOGS 5

        attron(COLOR_PAIR(4) | A_BOLD);
        mvhline(r, cx, ACS_HLINE, content_width);
        mvaddch(r, cx, ACS_ULCORNER);
        mvaddch(r, cx + content_width - 1, ACS_URCORNER);
        mvprintw(r, cx + 2, " RECENT LOGS ");
        attroff(COLOR_PAIR(4) | A_BOLD);

        sem_wait(&log_mutex);
        int log_count = log_queue->count;
        int head      = log_queue->head;
        int visible   = (log_count < MAX_VISIBLE_LOGS) ? log_count : MAX_VISIBLE_LOGS;
        int view_start = (head - visible + 200) % 200;

        for (int i = 1; i <= MAX_VISIBLE_LOGS; i++)
            mvaddch(r + i, cx, ACS_VLINE),
            mvaddch(r + i, cx + content_width - 1, ACS_VLINE);

        for (int i = 0; i < visible; i++) {
            int idx = (view_start + i) % 200;
            mvprintw(r + 1 + i, cx + 1, "%-.*s",
                     content_width - 2, log_queue->log_messages[idx]);
        }
        sem_post(&log_mutex);

        int bottom_border = r + MAX_VISIBLE_LOGS + 1;
        mvhline(bottom_border, cx, ACS_HLINE, content_width);
        mvaddch(bottom_border, cx, ACS_LLCORNER);
        mvaddch(bottom_border, cx + content_width - 1, ACS_LRCORNER);

        r = bottom_border + 2;

        // ================= HEADER =================
        attron(COLOR_PAIR(4) | A_BOLD);
        mvprintw(r++, cx, "Job Queue Broker");
        attroff(COLOR_PAIR(4) | A_BOLD);
        r++;

        // ================= WORKERS =================
        attron(A_BOLD);
        mvprintw(r++, cx, "WORKERS:");
        attroff(A_BOLD);
        mvprintw(r++, cx, "%-5s %-10s %-10s %-12s",
                 "ID", "STATUS", "JOBS_DONE", "JOBS_FAILED");

        sem_wait(&worker_mutex);
        for (int i = 0; i < worker_pool.num_workers; i++) {
            int pair;
            const char* status_str;

            if      (worker_pool.workers[i].status == WORKER_IDLE) { pair = 1; status_str = "IDLE";    }
            else if (worker_pool.workers[i].status == WORKER_BUSY) { pair = 2; status_str = "BUSY";    }
            else                                                    { pair = 3; status_str = "OFFLINE"; }

            attron(COLOR_PAIR(pair));
            mvprintw(r++, cx, "%-5d %-10s %-10d %-12d",
                     worker_pool.workers[i].worker_id, status_str,
                     worker_pool.workers[i].jobs_completed,
                     worker_pool.workers[i].jobs_failed);
            attroff(COLOR_PAIR(pair));
        }
        sem_post(&worker_mutex);
        r++;

        // ================= JOB QUEUE STATS =================
        sem_wait(&registry_mutex);
        int pending = 0, in_progress = 0, completed = 0, failed = 0;
        for (int i = 0; i < jobs_registered; i++) {
            if      (registry[i]->status == JOB_PENDING)     pending++;
            else if (registry[i]->status == JOB_IN_PROGRESS) in_progress++;
            else if (registry[i]->status == JOB_COMPLETED)   completed++;
            else if (registry[i]->status == JOB_FAILED)      failed++;
        }
        sem_post(&registry_mutex);

        attron(A_BOLD);
        mvprintw(r++, cx, "JOB QUEUE:");
        attroff(A_BOLD);

        attron(COLOR_PAIR(1));
        mvprintw(r++, cx, "  Pending:     %d", pending);
        mvprintw(r++, cx, "  In Progress: %d", in_progress);
        mvprintw(r++, cx, "  Completed:   %d", completed);
        attroff(COLOR_PAIR(1));

        attron(COLOR_PAIR(3));
        mvprintw(r++, cx, "  Failed:      %d", failed);
        attroff(COLOR_PAIR(3));

        refresh();
        napms(500);
    }

    endwin();
    return NULL;
}