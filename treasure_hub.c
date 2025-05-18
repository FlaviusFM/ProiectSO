#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#define CMD_FILE "hub_command.txt"

pid_t monitor_pid = -1;
int monitor_running = 0;
int pending_stop = 0;

int read_fd_from_monitor = -1;

void sigchld_handler (int signo) {
    int status;
    pid_t pid = wait(&status);
    if (pid == monitor_pid) {
        printf("Monitor exited with status %d\n", WEXITSTATUS(status));
	monitor_running = 0;
    }
    pending_stop = 0;

    if (read_fd_from_monitor != -1) {
        close(read_fd_from_monitor);
	read_fd_from_monitor = -1;
    }
}

void send_command_to_monitor (const char* cmd) {
    int fd = open(CMD_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        write(fd, cmd, strlen(cmd));
        close(fd);
    }
    kill(monitor_pid, SIGUSR1);

    //citeste rezultatul din pipe
    char buffer[256];
    ssize_t len = read(read_fd_from_monitor, buffer, sizeof(buffer) - 1);
    if (len > 0) {
        buffer[len] = '\0';
        printf("%s", buffer);
    }
}

int main (void) {
    struct sigaction sa;
    //memset(&sigchld_handler, 0x00, sizeof(struct sigaction));
    sa.sa_handler = sigchld_handler;
    
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) < 0)
      return 1;

    char input[128];

    while (1) {
        printf("hub> ");

        if (!fgets(input, sizeof(input), stdin))
	  break;
        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "start_monitor") == 0) {
            if (monitor_running) {
                printf("Monitor already running.\n");
                continue;
            }

	    int pipefd[2];
	    if (pipe(pipefd) == -1) {
	        printf("pipe failed\n");
	        exit(1);
	    }
	    
            monitor_pid = fork();
            if (monitor_pid == 0) {
	        close(pipefd[0]);
		char write_fd_str[16];
		sprintf(write_fd_str, "%d", pipefd[1]);
                execl("./treasure_monitor", "./treasure_monitor", write_fd_str, NULL);
		
                printf("execl failed\n");
                exit(1);
            }
            monitor_running = 1;

	    close(pipefd[1]);
	    read_fd_from_monitor = pipefd[0];
	    
            printf("Monitor started (PID %d)\n", monitor_pid);
        }
        else if (strcmp(input, "stop_monitor") == 0) {
            if (!monitor_running) {
                printf("No monitor running.\n");
                continue;
            }

            send_command_to_monitor("stop");
	    pending_stop = 1;
            printf("Sent stop command to monitor. Waiting for it to exit...\n");
        }
        else if (strcmp(input, "list_hunts") == 0) {
	    if (pending_stop) {
	        printf("Monitor is shutting down. Please wait...\n");
	        continue;
	    }

            if (!monitor_running) {
                printf("Monitor not running.\n");
                continue;
            }

            send_command_to_monitor("list_hunts");
        }
        else if (strncmp(input, "list_treasures ", 15) == 0) {
	    if (pending_stop) {
	        printf("Monitor is shutting down. Please wait...\n");
	        continue;
	    }
	  
            if (!monitor_running) {
                printf("Monitor not running.\n");
                continue;
            }

            char* hunt_id = input + 15; // tot ce urmeaza dupa "list_treasures "
	    char full_cmd[256];
	    snprintf(full_cmd, sizeof(full_cmd), "list_treasures %s", hunt_id);
	    send_command_to_monitor(full_cmd);
        }
        else if (strncmp(input, "view_treasure ", 14) == 0) {
	    if (pending_stop) {
	        printf("Monitor is shutting down. Please wait...\n");
	        continue;
	    }

            if (!monitor_running) {
                printf("Monitor not running.\n");
                continue;
            }

            char full_cmd[256];
	    snprintf(full_cmd, sizeof(full_cmd), "%s", input);
	    send_command_to_monitor(full_cmd);
        }
	else if (strcmp(input, "calculate_score") == 0) {
	   if (pending_stop) {
	        printf("Monitor is shutting down. Please wait...\n");
	        continue;
	    }

            if (!monitor_running) {
                printf("Monitor not running.\n");
                continue;
            }

	    char full_cmd[256];
	    snprintf(full_cmd, sizeof(full_cmd), "%s", input);
	    send_command_to_monitor(full_cmd);
	}
	else if (strcmp(input, "exit") == 0) {
            if (monitor_running) {
                printf("Monitor still running. Use stop_monitor first.\n");
            } else {
                printf("Bye!\n");
                break;
            }
        }
        else {
            printf("Unknown command.\n");
        }
    }

    return 0;
}
