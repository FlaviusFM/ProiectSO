#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_USERS 100

#define USERNAME_SIZE 32
#define CLUE_SIZE 128
#define ID_SIZE 16
#define BUFFER_SIZE 256

typedef struct {
    char treasure_id[ID_SIZE];
    char username[USERNAME_SIZE];
    float latitude;
    float longitude;
    char clue[CLUE_SIZE];
    int value;
} Treasure;

typedef struct {
    char username[USERNAME_SIZE];
    int total_score;
} UserScore;

int main (int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <hunt_id>\n", argv[0]);
        return 1;
    }

    char path[128];
    snprintf(path, sizeof(path), "%s/treasures.dat", argv[1]);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open treasures.dat");
        return 1;
    }

    UserScore scores[MAX_USERS];
    int user_count = 0;

    Treasure t;
    ssize_t bytes_read;
    
    while ((bytes_read = read(fd, &t, sizeof(Treasure))) == sizeof(Treasure)) {
        if (strlen(t.username) == 0)
	    continue;

        int found = 0;
        for (int i = 0; i < user_count; i ++) {
            if (strcmp(scores[i].username, t.username) == 0) {
                scores[i].total_score += t.value;
                found = 1;
                break;
            }
        }

        if (!found && user_count < MAX_USERS) {
            strcpy(scores[user_count].username, t.username);
            scores[user_count].total_score = t.value;
            user_count ++;
        }
    }

    if (bytes_read < 0) {
        printf("Error reading from file\n");
        close(fd);
        return 1;
    }

    close(fd);

    for (int i = 0; i < user_count; i ++)
        printf("User: %s, Score: %d\n", scores[i].username, scores[i].total_score);

    return 0;
}
