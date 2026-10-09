#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

int main(int argc, char *argv[]) {
    char *dir = ".";
    if (argc > 1) dir = argv[1];

    DIR *dp = opendir(dir);
    if (dp == NULL) {
        perror("opendir");
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.') continue; /* skip hidden files */
        printf("%s\n", entry->d_name);
    }

    closedir(dp);
    return 0;
}
