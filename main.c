#define NOB_IMPLEMENTATION
#include "nob.h"

#define FLAG_IMPLEMENTATION
#include "flag.h"

#define TASKS_FOLDER_NAME "tasks"
#define TASKS_PATH_COMPONENT "/"TASKS_FOLDER_NAME
#define PARENT_PATH_COMPONENT "/.."

void usage(FILE *stream) {
    fprintf(stream, "Usage: $task [ARGS]\n");
    fprintf(stream, "OPTIONS:\n");
    flag_print_options(stream);
}

const char *scan_for_first_tasks_folder(void) {
    char dir[512] = {0};
    getcwd(dir, 512);
    size_t dir_length = strlen(dir);

    struct stat s;
    if (stat("/", &s) != 0) return NULL;
    dev_t root_dev = s.st_dev;
    ino_t root_ino = s.st_ino;

    Nob_File_Paths children = {0};
    while (true) {
        children = (Nob_File_Paths){0};
        if (!nob_read_entire_dir(dir, &children)) return NULL;

        for (size_t i = 0; i < children.count; ++i) {
            if (strcmp(TASKS_FOLDER_NAME, children.items[i]) == 0) {
                int tasks_path_length = strlen(TASKS_PATH_COMPONENT);
                memcpy(&dir[dir_length], TASKS_PATH_COMPONENT, tasks_path_length);
                dir_length += tasks_path_length;

                char *path = malloc(dir_length + 1);
                memcpy(path, dir, dir_length + 1);
                return path;
            }
        }

        if (stat(dir, &s) != 0 || (s.st_dev == root_dev && s.st_ino == root_ino)) break; // check that dir is not equal to "/"

        int tasks_path_length = strlen(PARENT_PATH_COMPONENT);
        memcpy(&dir[dir_length], PARENT_PATH_COMPONENT, tasks_path_length);
        dir_length += tasks_path_length;
    }

    return NULL;
}

// 26122025-112830 is supported
// 26122025-112830-arbitrary - not yet; TODO: how exactly to detect end of huid here?
Nob_String_View get_valid_huid(const char *string) {
    Nob_String_View empty = {0};
    if (strlen(string) < 15) return empty;

    if (!(string[0] >= '0' && string[0] <= '3')) return empty;
    if (!(string[1] >= '0' && string[1] <= '9')) return empty; // this fails to check against day being 39 but I don't care for now

    if (!(string[2] >= '0' && string[2] <= '1')) return empty;
    if (!(string[3] >= '0' && string[3] <= '9')) return empty;

    if (!(string[4] >= '0' && string[4] <= '2')) return empty;
    if (!(string[5] >= '0' && string[5] <= '9')) return empty;
    if (!(string[6] >= '0' && string[6] <= '9')) return empty;
    if (!(string[7] >= '0' && string[7] <= '9')) return empty;

    if (string[8] != '-') return empty;

    if (!(string[9] >= '0' && string[9] <= '2')) return empty;
    if (!(string[10] >= '0' && string[10] <= '9')) return empty;

    if (!(string[11] >= '0' && string[11] <= '5')) return empty;
    if (!(string[12] >= '0' && string[12] <= '9')) return empty;

    if (!(string[13] >= '0' && string[13] <= '5')) return empty;
    if (!(string[14] >= '0' && string[14] <= '9')) return empty;

    return (Nob_String_View) { .data = string, .count = 15 };
}

int main(int argc, char **argv) {
    bool *ls = flag_bool("ls", NULL, "Print all tasks");
    bool *help = flag_bool("h", false, "Print this message");

    if (!flag_parse(argc, argv)) {
        usage(stderr);
        flag_print_error(stderr);
        return 1;
    }

    if (help != NULL && *help) {
        usage(stdout);
        return 0;
    }

    if (ls != NULL && *ls) {
        const char *tasks_folder = scan_for_first_tasks_folder();
        if (tasks_folder == NULL) {
            printf("Unable to find tasks folder.\n");
            return 1;
        }
        printf("Printing all tasks from %s...\n", tasks_folder);

        return 0;
    }

    usage(stdout);
    return 1;
}
