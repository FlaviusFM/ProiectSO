#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

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

void log_operation (const char *hunt_id, const char *message);
void create_symlink (const char *hunt_id);
char *get_hunt_path (const char *hunt_id, const char *filename);
void add_treasure (const char *hunt_id);
void list_treasures (const char *hunt_id);
void view_treasure (const char *hunt_id, const char *treasure_id);
void remove_treasure (const char *hunt_id, const char *treasure_id);
void remove_hunt (const char *hunt_id);

int main (int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "eroare argumente\nUsage: %s --<comanda> <hunt_id> [<id>]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char* command = argv[1];
    const char* hunt_id = argv[2];

    if (strcmp(command, "--add") == 0)
        add_treasure(hunt_id);
    else if (strcmp(command, "--list") == 0)
        list_treasures(hunt_id);
    else if (strcmp(command, "--view") == 0 && argc == 4)
        view_treasure(hunt_id, argv[3]);
    else if (strcmp(command, "--remove_treasure") == 0 && argc == 4)
        remove_treasure(hunt_id, argv[3]);
    else if (strcmp(command, "--remove_hunt") == 0)
        remove_hunt(hunt_id);
    else {
        fprintf(stderr, "operatie invalida\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}

char* get_hunt_path (const char* hunt_id, const char* filename) {
    static char path[BUFFER_SIZE];
    snprintf(path, sizeof(path), "./%s/%s", hunt_id, filename);
    return path;
}

void log_operation (const char* hunt_id, const char* message) {
    char *log_path = get_hunt_path(hunt_id, "logged_hunt");

    int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd < 0) {
        perror("Failed to open log file");
        return;
    }

    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", localtime(&now));

    dprintf(log_fd, "%s %s\n", timestamp, message);
    close(log_fd);

    create_symlink(hunt_id);
}

void create_symlink (const char* hunt_id) {
    char linkname[BUFFER_SIZE];
    snprintf(linkname, sizeof(linkname), "./logged_hunt-%s", hunt_id);

    char target[BUFFER_SIZE];
    snprintf(target, sizeof(target), "./%s/logged_hunt", hunt_id);

    if (access(linkname, F_OK) == -1) {
        symlink(target, linkname);
    }
}

// ================================
// Core Function Placeholders
// ================================

void add_treasure (const char* hunt_id) {
  mkdir(hunt_id, 0755);
    char* filepath = get_hunt_path(hunt_id, "treasures.dat");

    int fd = open(filepath, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("Error opening treasure file");
        return;
    }

    Treasure t;
    printf("Enter Treasure ID: ");
    scanf("%15s", t.treasure_id);
    printf("Enter Username: ");
    scanf("%31s", t.username);
    printf("Enter Latitude: ");
    scanf("%f", &t.latitude);
    printf("Enter Longitude: ");
    scanf("%f", &t.longitude);
    printf("Enter Clue: ");
    getchar();
    fgets(t.clue, CLUE_SIZE, stdin);
    t.clue[strcspn(t.clue, "\n")] = '\0';
    printf("Enter Value: ");
    scanf("%d", &t.value);

    write(fd, &t, sizeof(Treasure));
    close(fd);

    char log_msg[BUFFER_SIZE];
    snprintf(log_msg, sizeof(log_msg), "Added treasure %s by user %s", t.treasure_id, t.username);
    log_operation(hunt_id, log_msg);
}

void list_treasures (const char* hunt_id) {
    char* filepath = get_hunt_path(hunt_id, "treasures.dat");

    struct stat st;
    if (stat(filepath, &st) < 0) {
        perror("Cannot stat file");
        return;
    }

    printf("Hunt: %s\n", hunt_id);
    printf("File size: %ld bytes\n", st.st_size);
    printf("Last modified: %s", ctime(&st.st_mtime));

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        perror("Error opening treasure file");
        return;
    }

    Treasure t;
    printf("\nTreasure List:\n");
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        printf("ID: %s | User: %s | Lat: %.2f | Lon: %.2f | Value: %d\n", t.treasure_id, t.username, t.latitude, t.longitude, t.value);
    }
    close(fd);
}

void view_treasure (const char* hunt_id, const char* treasure_id) {
    char* filepath = get_hunt_path(hunt_id, "treasures.dat");

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        perror("Error opening treasure file");
        return;
    }

    Treasure t;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        if (strcmp(t.treasure_id, treasure_id) == 0) {
            printf("Treasure Details:\n");
            printf("ID: %s\nUser: %s\nLat: %.2f\nLon: %.2f\nClue: %s\nValue: %d\n",
                t.treasure_id, t.username, t.latitude, t.longitude, t.clue, t.value);

            char log_msg[BUFFER_SIZE];
            snprintf(log_msg, sizeof(log_msg), "Viewed treasure %s", t.treasure_id);
            log_operation(hunt_id, log_msg);
            close(fd);
            return;
        }
    }

    printf("Treasure ID '%s' not found.\n", treasure_id);
    close(fd);
}

void remove_treasure (const char* hunt_id, const char* treasure_id) {
    char* filepath = get_hunt_path(hunt_id, "treasures.dat");
    char tmpfile[] = "./tmp_treasuresXXXXXX";
    int tmp_fd = mkstemp(tmpfile);

    int fd = open(filepath, O_RDONLY);
    if (fd < 0 || tmp_fd < 0) {
        perror("Error opening files");
        return;
    }

    Treasure t;
    int found = 0;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        if (strcmp(t.treasure_id, treasure_id) != 0) {
            write(tmp_fd, &t, sizeof(Treasure));
        } else {
            found = 1;
        }
    }

    close(fd);
    close(tmp_fd);

    if (found) {
        rename(tmpfile, filepath);
        char log_msg[BUFFER_SIZE];
        snprintf(log_msg, sizeof(log_msg), "Removed treasure %s", treasure_id);
        log_operation(hunt_id, log_msg);
        printf("Treasure removed.\n");
    } else {
        unlink(tmpfile);
        printf("Treasure not found.\n");
    }
}

void remove_hunt (const char* hunt_id) {
    char path[BUFFER_SIZE];
    snprintf(path, sizeof(path), "./%s", hunt_id);

    char* log_path = get_hunt_path(hunt_id, "logged_hunt");
    char* data_path = get_hunt_path(hunt_id, "treasures.dat");

    unlink(data_path);
    unlink(log_path);
    rmdir(path);

    char linkname[BUFFER_SIZE];
    snprintf(linkname, sizeof(linkname), "./logged_hunt-%s", hunt_id);
    unlink(linkname);

    printf("Hunt '%s' removed.\n", hunt_id);
}
