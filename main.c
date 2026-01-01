#define NOB_STRIP_PREFIX
#define NOB_IMPLEMENTATION
#include "nob.h"

#define TASKS_FOLDER_NAME "tasks"
#define TASKS_PATH_COMPONENT "/"TASKS_FOLDER_NAME
#define PARENT_PATH_COMPONENT "/.."

#define MIN(a, b) (((a)<(b))?(a):(b))
#define DEFAULT_TASK_TITLE_FIXED_LENGTH 100

void print_usage(FILE *stream) {
    fprintf(stream, "Usage: task <COMMAND> [OPTIONS]\n");
    fprintf(stream, "COMMAND:\n");
    fprintf(stream, "  ls: list tasks sorted by priority\n");
    fprintf(stream, "    -t <tag> - filter by tag; default: none\n");
    fprintf(stream, "    -c       - only show closed; default: only non-closed\n");
    fprintf(stream, "    -f       - fix task title length to align tags, 0 to disable; default: %d\n", DEFAULT_TASK_TITLE_FIXED_LENGTH);
    fprintf(stream, "\n");
    fprintf(stream, "You can also disable each flag by slashing it after the dash.\n");
    fprintf(stream, "For example: task -/c will display non-closed tasks.\n");
}

String_View temp_sv_dup(String_View sv)
{
    char *result = (char*)nob_temp_alloc(sv.count + 1);
    NOB_ASSERT(result != NULL && "Increase NOB_TEMP_CAPACITY");
    memcpy(result, sv.data, sv.count);
    return nob_sv_from_parts(result, sv.count);
}

String_View sv_chop_by_sv(String_View *sv, String_View thicc_delim) { // from sv.h
    String_View window = sv_from_parts(sv->data, thicc_delim.count);
    size_t i = 0;
    while (i + thicc_delim.count < sv->count
        && !(sv_eq(window, thicc_delim))) {
        i++;
        window.data++;
    }

    String_View result = sv_from_parts(sv->data, i);

    if (i + thicc_delim.count == sv->count) {
        result.count += thicc_delim.count;
    }

    sv->data  += i + thicc_delim.count;
    sv->count -= i + thicc_delim.count;

    return result;
}

bool scan_for_first_tasks_folder(char *dir) {
    getcwd(dir, 512);
    size_t dir_length = strlen(dir);

    struct stat s;
    if (stat("/", &s) != 0) return NULL;
    dev_t root_dev = s.st_dev;
    ino_t root_ino = s.st_ino;

    File_Paths children = {0};
    while (true) {
        children = (File_Paths){0};
        if (!read_entire_dir(dir, &children)) return NULL;

        for (size_t i = 0; i < children.count; ++i) {
            if (strcmp(TASKS_FOLDER_NAME, children.items[i]) == 0) {
                int tasks_path_length = strlen(TASKS_PATH_COMPONENT);
                memcpy(&dir[dir_length], TASKS_PATH_COMPONENT, tasks_path_length);
                dir_length += tasks_path_length;
                return true;
            }
        }

        da_free(children);

        if (stat(dir, &s) != 0 || (s.st_dev == root_dev && s.st_ino == root_ino)) break; // check that dir is not equal to "/"

        int tasks_path_length = strlen(PARENT_PATH_COMPONENT);
        memcpy(&dir[dir_length], PARENT_PATH_COMPONENT, tasks_path_length);
        dir_length += tasks_path_length;
    }

    return NULL;
}

// TODO: convert time to UTC
// 26122025-112830 is supported
// 26122025-112830-arbitrary - not yet; TODO: how exactly to detect end of huid here?
String_View get_valid_huid(const char *string) {
    String_View empty = {0};
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

    return (String_View) { .data = string, .count = 15 };
}

typedef struct {
    String_View title;
    String_View file_path;
    String_View tags;
    int priority;
} Task;

typedef struct {
    Task *items;
    size_t capacity;
    size_t count;
} Tasks;

int compare_task_priority_descending(const void* a, const void* b) {
    const Task* sa = (const Task*)a;
    const Task* sb = (const Task*)b;
    if (sa->priority < sb->priority) return 1;
    if (sa->priority > sb->priority) return -1;
    return 0;
}

bool list_all_tasks(char *filter_tag, bool only_closed, int task_title_fixed_length) {
    char tasks_folder[512] = {0};
    if (!scan_for_first_tasks_folder(tasks_folder)) {
        printf("Unable to find tasks folder.\n");
        return false;
    }

    File_Paths children = {0};
    if (!read_entire_dir(tasks_folder, &children)) return false;

    Tasks tasks = {0};

    char path[512] = {0};
    da_foreach(const char *, child, &children) {
        String_View huid = get_valid_huid(*child);
        if (huid.count > 0) {
            sprintf(path, "%s/%s/task.md", tasks_folder, *child);

            String_Builder sb = {0};
            if (!read_entire_file(path, &sb)) continue;

            int priority = 20;

            String_View file_sv = sb_to_sv(sb);
            String_View task_title = sv_chop_by_sv(&file_sv, sv_from_cstr("\n\n"));
            String_View tags = {0};

            if (sv_starts_with(task_title, sv_from_cstr("# "))) {
                sv_chop_left(&task_title, 2);
            }

            String_View parameters = sv_chop_by_sv(&file_sv, sv_from_cstr("\n\n"));
            while (true) {
                String_View parameter = sv_chop_by_delim(&parameters, '\n');
                if (!sv_starts_with(parameter, sv_from_cstr("- "))) break;
                sv_chop_left(&parameter, 2);

                String_View key = sv_chop_by_sv(&parameter, sv_from_cstr(": "));
                String_View value = parameter;

                if (sv_eq(key, sv_from_cstr("STATUS"))) {
                    if (only_closed != sv_eq(value, sv_from_cstr("CLOSED"))) {
                        goto skip;
                    }
                } else if (sv_eq(key, sv_from_cstr("PRIORITY"))) {
                    char string[50] = {0};
                    memcpy(string, value.data, value.count);
                    priority = (int)atof(string);
                } else if (sv_eq(key, sv_from_cstr("TAGS"))) {
                    tags = value;
                    if (filter_tag != NULL) {
                        String_View filter_tag_sv = sv_from_cstr(filter_tag);
                        String_View tags = value;
                        bool found_tag = false;
                        while (true) {
                            String_View tag = sv_chop_by_delim(&tags, ',');
                            tag = sv_trim(tag);
                            if (sv_eq(tag, filter_tag_sv)) found_tag = true;
                            if (tag.count == 0) break;
                        }
                        if (!found_tag) goto skip;
                    }
                }
            }

            da_append(&tasks, ((Task) {
                .priority = priority,
                .title = temp_sv_dup(task_title),
                .file_path = temp_sv_dup(sv_from_cstr(*child)),
                .tags = temp_sv_dup(tags),
            }));

skip:
            sb_free(sb);
        }
    }

    qsort(tasks.items, tasks.count, sizeof(*tasks.items), compare_task_priority_descending);

    da_foreach(Task, task, &tasks) {
        int print_length = task->title.count;
        if (task_title_fixed_length > 0) print_length = MIN(task_title_fixed_length, print_length);

        printf("tasks/" SV_Fmt "/task.md:1:1 |%2d| ", SV_Arg(task->file_path), task->priority);
        printf("%-*.*s", task_title_fixed_length, print_length, (task->title).data);
        printf(" | " SV_Fmt "\n", SV_Arg(task->tags));
    }

    da_free(children);
    nob_temp_reset();
    return true;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Command expected, got nothing\n");
        print_usage(stderr);
        return 1;
    }

    if (strcmp(argv[1], "ls") == 0) {
        int i = 2;
        char *tag = NULL;
        bool only_closed = false;
        int task_title_fixed_length = DEFAULT_TASK_TITLE_FIXED_LENGTH;

        while (i < argc) {
            if (strcmp(argv[i], "-t") == 0) {
                i += 1;
                if (i >= argc) {
                    printf("Expected value for -t, got nothing\n");
                    print_usage(stderr);
                    return 1;
                }
                tag = argv[i];
            } else if (strcmp(argv[i], "-/t") == 0) {
                i += 1;
                if (i >= argc) {
                    printf("Expected value for -t, got nothing\n");
                    print_usage(stderr);
                    return 1;
                }
                tag = NULL;
            } else if (strcmp(argv[i], "-f") == 0) {
                i += 1;
                if (i >= argc) {
                    printf("Expected value for -f, got nothing\n");
                    print_usage(stderr);
                    return 1;
                }
                task_title_fixed_length = atoi(argv[i]);
            } else if (strcmp(argv[i], "-/f") == 0) {
                i += 1;
                if (i >= argc) {
                    printf("Expected value for -f, got nothing\n");
                    print_usage(stderr);
                    return 1;
                }
                task_title_fixed_length = 0;
            } else if (strcmp(argv[i], "-c") == 0) {
                only_closed = true;
            } else if (strcmp(argv[i], "-/c") == 0) {
                only_closed = false;
            } else {
                printf("Unexpected option for ls: %s\n", argv[i]);
                print_usage(stderr);
                return 1;
            }
            i += 1;
        }

        return list_all_tasks(tag, only_closed, task_title_fixed_length) ? 0 : 1;
    } else {
        printf("Unexpected argument: %s\n", argv[1]);
        print_usage(stderr);
        return 1;
    }
}
