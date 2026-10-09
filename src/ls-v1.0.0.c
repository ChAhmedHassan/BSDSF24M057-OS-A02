#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

#define SPACING 2

#define COLOR_RESET   "\033[0m"
#define COLOR_BLUE    "\033[0;34m"
#define COLOR_GREEN   "\033[0;32m"
#define COLOR_RED     "\033[0;31m"
#define COLOR_PINK    "\033[0;35m"
#define COLOR_REVERSE "\033[7m"

typedef enum { MODE_DEFAULT, MODE_LONG, MODE_HORIZONTAL } DisplayMode;

int compare_names(const void *a, const void *b) {
    const char *name_a = *(const char **)a;
    const char *name_b = *(const char **)b;
    return strcmp(name_a, name_b);
}

void print_permissions(mode_t mode) {
    char perms[11];
    perms[0] = S_ISDIR(mode) ? 'd' : (S_ISLNK(mode) ? 'l' : '-');
    perms[1] = (mode & S_IRUSR) ? 'r' : '-';
    perms[2] = (mode & S_IWUSR) ? 'w' : '-';
    perms[3] = (mode & S_IXUSR) ? 'x' : '-';
    perms[4] = (mode & S_IRGRP) ? 'r' : '-';
    perms[5] = (mode & S_IWGRP) ? 'w' : '-';
    perms[6] = (mode & S_IXGRP) ? 'x' : '-';
    perms[7] = (mode & S_IROTH) ? 'r' : '-';
    perms[8] = (mode & S_IWOTH) ? 'w' : '-';
    perms[9] = (mode & S_IXOTH) ? 'x' : '-';
    perms[10] = '\0';
    printf("%s ", perms);
}

/* Decide what color a file should be printed in, based on its type */
const char *get_color(const char *path) {
    struct stat st;
    if (lstat(path, &st) == -1) return "";

    if (S_ISLNK(st.st_mode)) return COLOR_PINK;
    if (S_ISDIR(st.st_mode)) return COLOR_BLUE;
    if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode) ||
        S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode)) return COLOR_REVERSE;

    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    int len = strlen(name);
    if ((len > 4 && strcmp(name + len - 4, ".tar") == 0) ||
        (len > 3 && strcmp(name + len - 3, ".gz")  == 0) ||
        (len > 4 && strcmp(name + len - 4, ".zip") == 0)) {
        return COLOR_RED;
    }

    if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) return COLOR_GREEN;

    return "";
}

void print_long_listing(const char *path, const char *name) {
    struct stat st;
    if (lstat(path, &st) == -1) { perror("lstat"); return; }
    print_permissions(st.st_mode);
    printf("%ld ", (long)st.st_nlink);
    struct passwd *pw = getpwuid(st.st_uid);
    struct group *gr = getgrgid(st.st_gid);
    printf("%s ", pw ? pw->pw_name : "?");
    printf("%s ", gr ? gr->gr_name : "?");
    printf("%6ld ", (long)st.st_size);
    char timebuf[64];
    struct tm *tm_info = localtime(&st.st_mtime);
    strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", tm_info);
    printf("%s ", timebuf);

    const char *color = get_color(path);
    printf("%s%s%s\n", color, name, color[0] ? COLOR_RESET : "");
}

int get_terminal_width(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col == 0) return 80;
    return w.ws_col;
}

int max_name_length(char **names, int count) {
    int max_len = 0;
    for (int i = 0; i < count; i++) {
        int len = strlen(names[i]);
        if (len > max_len) max_len = len;
    }
    return max_len;
}

/* Print one name in color, then pad with spaces to col_width.
   Padding is done manually (not with %-*s) because the invisible
   escape-code characters would otherwise throw off the alignment. */
void print_colored_name(const char *dir, const char *name, int col_width) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    const char *color = get_color(path);
    int name_len = strlen(name);
    printf("%s%s%s", color, name, color[0] ? COLOR_RESET : "");
    for (int i = name_len; i < col_width; i++) printf(" ");
}

void print_columns(const char *dir, char **names, int count) {
    if (count == 0) return;
    int col_width = max_name_length(names, count) + SPACING;
    int term_width = get_terminal_width();
    int num_cols = term_width / col_width;
    if (num_cols < 1) num_cols = 1;
    int num_rows = (count + num_cols - 1) / num_cols;

    for (int row = 0; row < num_rows; row++) {
        for (int col = 0; col < num_cols; col++) {
            int idx = col * num_rows + row;
            if (idx < count) print_colored_name(dir, names[idx], col_width);
        }
        printf("\n");
    }
}

void print_horizontal(const char *dir, char **names, int count) {
    if (count == 0) return;
    int col_width = max_name_length(names, count) + SPACING;
    int term_width = get_terminal_width();

    int current_width = 0;
    for (int i = 0; i < count; i++) {
        if (current_width != 0 && current_width + col_width > term_width) {
            printf("\n");
            current_width = 0;
        }
        print_colored_name(dir, names[i], col_width);
        current_width += col_width;
    }
    printf("\n");
}

 void do_ls(const char *dir, DisplayMode mode, int recursive_flag) {
    DIR *dp = opendir(dir);
    if (dp == NULL) { perror("opendir"); return; }

    char **names = NULL;
    int count = 0, capacity = 0;

    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (count >= capacity) {
            capacity = capacity == 0 ? 16 : capacity * 2;
            names = realloc(names, capacity * sizeof(char *));
        }
        names[count] = strdup(entry->d_name);
        count++;
    }
    closedir(dp);

    qsort(names, count, sizeof(char *), compare_names);

        if (recursive_flag) {
        printf("%s:\n", dir);
    }

    switch (mode) {
        case MODE_LONG:
            for (int i = 0; i < count; i++) {
                char path[1024];
                snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
                print_long_listing(path, names[i]);
            }
            break;
        case MODE_HORIZONTAL:
            print_horizontal(dir, names, count);
            break;
        default:
            print_columns(dir, names, count);
    }

    /* Recurse into any subdirectories, after listing this one */
    if (recursive_flag) {
        for (int i = 0; i < count; i++) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", dir, names[i]);

            struct stat st;
            if (lstat(path, &st) == -1) continue;

            if (S_ISDIR(st.st_mode)) {
                printf("\n");
                do_ls(path, mode, recursive_flag); /* base case: not a directory, or we just print and return */
            }
        }
    }

    for (int i = 0; i < count; i++) free(names[i]);
    free(names);
}

int main(int argc, char *argv[]) {
    DisplayMode mode = MODE_DEFAULT;
    int opt;

        int recursive_flag = 0;

    while ((opt = getopt(argc, argv, "lxR")) != -1) {
        switch (opt) {
            case 'l': mode = MODE_LONG; break;
            case 'x': mode = MODE_HORIZONTAL; break;
            case 'R': recursive_flag = 1; break;
            default:
                fprintf(stderr, "Usage: %s [-l] [-x] [-R] [directory]\n", argv[0]);
                return 1;
        }
    }

    char *dir = ".";
    if (optind < argc) dir = argv[optind];

    do_ls(dir, mode, recursive_flag);
    return 0;
}
