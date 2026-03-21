#include "dashboard_handler.h"

void* dashboard(void* arg) {
    initscr();
    noecho();
    curs_set(0);
    start_color();

    init_pair(1, COLOR_GREEN,  COLOR_BLACK);  // idle workers
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);  // busy workers
    init_pair(3, COLOR_RED,    COLOR_BLACK);  // offline workers
    init_pair(4, COLOR_CYAN,   COLOR_BLACK);  // headers

    while (1) {
        clear();

        // Header
        attron(COLOR_PAIR(4) | A_BOLD);
        mvprintw(0, 0, "=== NodeTalk Job Queue Broker ===");
        attroff(COLOR_PAIR(4) | A_BOLD);

        // Worker status table
        mvprintw(2, 0, "WORKERS:");
        mvprintw(3, 0, "%-5s %-10s %-8s %-10s %-8s",
                 "ID", "STATUS", "JOBS_DONE", "JOBS_FAILED", "LAST_SEEN");

        
        sem_wait(&worker_mutex);

        int row = 4;
        
        for (int i = 0 ; i < worker_pool.num_workers ; i++)
        {
            int pair = (worker_pool.workers[i].status == WORKER_IDLE) ? 1 :
                       (worker_pool.workers[i].status == WORKER_BUSY) ? 2 : 3;
            const char* status_str = (worker_pool.workers[i].status == WORKER_IDLE) ? "IDLE" :
                                     (worker_pool.workers[i].status == WORKER_BUSY) ? "BUSY" : "OFFLINE";

            attron(COLOR_PAIR(pair));
            mvprintw(row++, 0, "%-5d %-10s %-8d %-10d",
                     worker_pool.workers[i].worker_id, status_str,
                     worker_pool.workers[i].jobs_completed, worker_pool.workers[i].jobs_failed);
            attroff(COLOR_PAIR(pair));
        }

        sem_post(&worker_mutex);

        // Job queue stats

        sem_wait(&registry_mutex);
        
        int pending = 0, in_progress = 0, completed = 0, failed = 0;
        

        for (int i = 0 ; i < jobs_registered ; i++) {
            if      (registry[i]->status == JOB_PENDING)     pending++;
            else if (registry[i]->status == JOB_IN_PROGRESS) in_progress++;
            else if (registry[i]->status == JOB_COMPLETED)   completed++;
            else if (registry[i]->status == JOB_FAILED)      failed++;
        }
        
        sem_post(&registry_mutex);

        mvprintw(row + 2, 0, "JOB QUEUE:");
        attron(COLOR_PAIR(1));
        mvprintw(row + 3, 0, "  Pending:     %d", pending);
        mvprintw(row + 4, 0, "  In Progress: %d", in_progress);
        mvprintw(row + 5, 0, "  Completed:   %d", completed);
        attroff(COLOR_PAIR(1));
        attron(COLOR_PAIR(3));
        mvprintw(row + 6, 0, "  Failed:      %d", failed);
        attroff(COLOR_PAIR(3));

        // Recent job log (last 5)
        mvprintw(row + 8, 0, "RECENT JOBS:");
        // ... iterate last 5 entries in job_registry and print

        refresh();
        sleep(1);   // update every second
    }
    endwin();
}