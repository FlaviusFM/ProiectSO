#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#include <dirent.h>
#include <sys/stat.h>

#define CMD_FILE "hub_command.txt"
#define BUFFER_SIZE 256

#define USERNAME_SIZE 32
#define CLUE_SIZE 128
#define ID_SIZE 16

typedef struct {
    char treasure_id[ID_SIZE];
    char username[USERNAME_SIZE];
    float latitude;
    float longitude;
    char clue[CLUE_SIZE];
    int value;
} Treasure;

volatile sig_atomic_t command_ready = 0;

int output_fd = 1;//stdout

void handle_signal (int signo) {
    if (signo == SIGUSR1) {
        command_ready = 1;
    }
}

void list_hunts() {
    DIR* dir = opendir(".");
    if (!dir) {
        printf("Nu pot deschide directorul curent\n");
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char path[256];
            snprintf(path, sizeof(path), "%s/treasures.dat", entry->d_name);

            struct stat st;
            if (stat(path, &st) == 0) {
                int count = st.st_size / sizeof(Treasure);
                dprintf(output_fd, "Hunt: %s | Comori: %d\n", entry->d_name, count);
            }
        }
    }
    closedir(dir);
}

void process_command() {
    char buffer[BUFFER_SIZE];

    int fd = open(CMD_FILE, O_RDONLY);
    if (fd < 0) {
        dprintf(output_fd, "Nu pot citi fișierul de comandă\n");
        return;
    }

    ssize_t len = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);

    if (len < 0) {
        dprintf(output_fd, "Eroare la citire\n");
        return;
    }

    buffer[len] = '\0';

    if (strcmp(buffer, "stop") == 0) {
        dprintf(output_fd, "Monitor: recieved stop command, exiting in 3 seconds...\n");
        usleep(3000000);
        exit(0);
    } else if (strcmp(buffer, "list_hunts") == 0) {
        list_hunts();
    } else if (strncmp(buffer, "list_treasures ", 15) == 0) {
        char hunt_id[64];
	sscanf(buffer, "list_treasures %s", hunt_id);

	int pipefd[2];
	if (pipe(pipefd) == -1) {
	    dprintf(output_fd, "Monitor: Pipe error\n");
	    return;
	}
      
	pid_t pid = fork();
	if (pid == 0) {
	    close(pipefd[0]);
	    dup2(pipefd[1], 1);
	    close(pipefd[1]);
	
	    execl("./treasure_manager", "./treasure_manager", "--list", hunt_id, NULL);
	    printf("execl failed\n");
	    exit(1);
	} else {
	    close(pipefd[1]);
	    char buf[256];
	    ssize_t r;
	    while ((r = read(pipefd[0], buf, sizeof(buf))) > 0) {
	        write(output_fd, buf, r);
	    }
	    close(pipefd[0]);
	
	    waitpid(pid, NULL, 0);
	}
    } else if (strncmp(buffer, "view_treasure ", 14) == 0) {
        char hunt_id[64];
        char treasure_id[64];
      
        if (sscanf(buffer + 14, "%s %s", hunt_id, treasure_id) == 2) {
	    int pipefd[2];
	    if (pipe(pipefd) == -1) {
	        dprintf(output_fd, "Monitor: Pipe error\n");
		return;
	    }
	  
	    pid_t pid = fork();
	    if (pid == 0) {
	        close(pipefd[0]);
		dup2(pipefd[1], 1);
		close(pipefd[1]);
	      
		execl("./treasure_manager", "./treasure_manager", "--view", hunt_id, treasure_id, NULL);
		printf("execl failed\n");
		exit(1);
	    } else {
	        close(pipefd[1]);
                char buf[1024];
                ssize_t r;
                while ((r = read(pipefd[0], buf, sizeof(buf))) > 0) {
                    write(output_fd, buf, r);
                }
                close(pipefd[0]);
	      
		waitpid(pid, NULL, 0);
	    }
	}  else {
	    dprintf(output_fd, "Format invalid. Foloseste: view_treasure hunt_id treasure_id\n");
	}
    } else if (strcmp(buffer, "calculate_score") == 0) {
        DIR *dir = opendir(".");
	if (!dir) {
	  printf("Cannot open hunts directory\n");
	  return;
	}
      
	struct dirent *entry;
	while ((entry = readdir(dir))) {
	  if (entry->d_type != DT_DIR || 
	      strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
	    continue;
	  
	  int pipefd[2];
	  if (pipe(pipefd) == -1) {
	    printf("pipe failed\n");
	    continue;
	  }
	  
	  pid_t pid = fork();
	  if (pid == 0) {
	    close(pipefd[0]);
	    dup2(pipefd[1], 1);
	    close(pipefd[1]);
	    execl("./calculate_score", "./calculate_score", entry->d_name, NULL);
	    printf("execl failed\n");
	    exit(1);
	  } else {
	    close(pipefd[1]);
	    char buf[256];
	    ssize_t n;
	    printf("Hunt: %s\n", entry->d_name);
	    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
	      write(output_fd, buf, n);
	    }
	    close(pipefd[0]);
	    waitpid(pid, NULL, 0);
	  }
	}
	closedir(dir);
    } else {
      dprintf(output_fd, "Monitor: Comandă necunoscută: %s\n", buffer);
    }
}

int main (int argc, char** argv) {
    if (argc >= 2)
        output_fd = atoi(argv[1]);
  
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        printf("sigaction error\n");
        exit(1);
    }

    printf("Monitor started. Waiting for commands...\n");

    while (1) {
        pause();

	if (command_ready) {
	  process_command();
	  command_ready = 0;
	}
    }

    return 0;
}
