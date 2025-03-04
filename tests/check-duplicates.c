#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

static inline bool is_hex_char(char c)
{
    return (c >= '0' && c <= '9')
        || (c >= 'a' && c <= 'f')
        || (c >= 'A' && c <= 'F')
        || c == 'x' || c == 'X';
}

int main(int argc, char **argv)
{
    if (argc < 2) return EXIT_SUCCESS;
    FILE *fp = fopen(argv[1], "rb");
    char *line_p = NULL;
    size_t size = 0;
    int line_len = 0;
    char prev_digit_buf[1024] = { 0 };
    unsigned int i = 0;
    while (i++, line_len = getline(&line_p, &size, fp), line_len != EOF) {

        /* Find the end of the digit */
        char *end_p = line_p + line_len - 2;
        while (end_p > line_p && !is_hex_char(*end_p))
            end_p--;

        if (end_p <= line_p) {
            memset(prev_digit_buf, 0, 1024);
            continue;
        }

        /* Find the start of the digit */
        char *start_p = end_p;
        while (start_p > line_p && is_hex_char(*(start_p - 1)))
            start_p--;

        if (start_p == end_p || !(
                (*start_p >= '0' && *start_p <= '9') ||
                (*start_p >= 'a' && *start_p <= 'f') ||
                (*start_p == 'x')
            ))
        {
            memset(prev_digit_buf, 0, 1024);
            continue;
        }
        *(end_p + 1) = '\0';

        /* Found a duplicate! */
        if (!strncmp(start_p, prev_digit_buf, 1024))
            printf("line %u and %u: %s\n", i, i - 1, prev_digit_buf);
        else
            strncpy(prev_digit_buf, start_p, 1024);
    }
    free(line_p);
    line_p = NULL;
    fclose(fp);
    return EXIT_SUCCESS;
}
