// Lafi Haneef Mulakkal 251ADB120
// https://github.com/lafi404-dev/cal-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: course assignment; ChatGPT (OpenAI). Conversation history must be added before submission.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

/*
 * Development stage 1: program entry point.
 * The expression evaluator and file handling will be added next.
 *
 * AI assistance: ChatGPT, in the conversation containing the supplied
 * "Graded lab: Arithmetic Parser in C" instructions, helped plan the
 * implementation stages and prepare this initial structure.
 */

int main(int argc, char *argv[]) {
    (void)argv;

    if (argc == 1) {
        fprintf(stderr,
                "Usage: calc [-d DIR] [-o OUTDIR] [input.txt]\n");
        return EXIT_FAILURE;
    }

    fprintf(stderr, "Expression evaluation is not implemented yet.\n");
    return EXIT_FAILURE;
}
