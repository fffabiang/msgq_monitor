#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Function to resolve environment variables in a given path
void resolve_env_vars(const char* path, char* resolved_path, size_t resolved_path_size) {
    if (path == NULL || resolved_path == NULL) {
        return;
    }

    const char* p = path;
    char* rp = resolved_path;
    size_t remaining_size = resolved_path_size;

    while (*p && remaining_size > 1) {
        if (*p == '$') {
            p++;
            if (*p == '{') {
                p++;
                const char* end = strchr(p, '}');
                if (!end) {
                    fprintf(stderr, "Unmatched '{' in path.\n");
                    return;
                }

                char var_name[256] = {0};
                strncpy(var_name, p, end - p);
                var_name[end - p] = '\0';

                const char* var_value = getenv(var_name);
                if (var_value) {
                    while (*var_value && remaining_size > 1) {
                        *rp++ = *var_value++;
                        remaining_size--;
                    }
                }

                p = end + 1;
            } else {
                const char* start = p;
                while (*p && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')) {
                    p++;
                }

                char var_name[256] = {0};
                strncpy(var_name, start, p - start);
                var_name[p - start] = '\0';

                const char* var_value = getenv(var_name);
                if (var_value) {
                    while (*var_value && remaining_size > 1) {
                        *rp++ = *var_value++;
                        remaining_size--;
                    }
                }
            }
        } else {
            *rp++ = *p++;
            remaining_size--;
        }
    }

    *rp = '\0';
}

int main() {
    char logpath[] = "$SIXDIR/log";
    char resolved_logpath[1024];

    resolve_env_vars(logpath, resolved_logpath, sizeof(resolved_logpath));

    printf("Resolved log path: %s\n", resolved_logpath);

    return 0;
}
