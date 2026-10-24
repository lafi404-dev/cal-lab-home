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

typedef enum {
    TOK_END,
    TOK_NUMBER,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_POWER,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_INVALID
} TokenKind;

/* Keep integers exact instead of converting every number to double. */
typedef struct {
    int is_float;
    long long integer;
    double real;
} Value;

typedef struct {
    TokenKind kind;
    size_t position;
    Value value;
} Token;

typedef struct {
    char *text;
    size_t length;
    size_t index;
    size_t offset;
    Token token;
} Parser;

/*
 * AI assistance: ChatGPT, using the assignment supplied in this
 * conversation, suggested storing a line offset and a token start.
 * offset counts all characters before this line; adding index + 1
 * produces a 1-based position in the original file.
 */
void next_token(Parser *p) {
    while (p->index < p->length &&
           (p->text[p->index] == ' ' || p->text[p->index] == '\t')) {
        p->index++;
    }

    size_t start = p->index;
    p->token = (Token){0};
    p->token.position = p->offset + start + 1;

    if (start == p->length) {
        p->token.kind = TOK_END;
        return;
    }

    unsigned char ch = (unsigned char)p->text[p->index];

    if (isdigit(ch)) {
        while (p->index < p->length &&
               isdigit((unsigned char)p->text[p->index])) {
            p->index++;
        }

        int is_float = 0;

        /* A decimal point belongs to a number only with digits after it.
         * Otherwise it remains for the next token and is invalid there.
         */
        if (p->index + 1 < p->length &&
            p->text[p->index] == '.' &&
            isdigit((unsigned char)p->text[p->index + 1])) {
            is_float = 1;
            p->index++;

            while (p->index < p->length &&
                   isdigit((unsigned char)p->text[p->index])) {
                p->index++;
            }
        }

        /*
         * Temporarily terminate this token so library conversion cannot
         * consume forbidden exponent notation such as the e3 in 1.5e3.
         * Restore the character afterwards; no substring copy is needed.
         */
        char saved = p->text[p->index];
        p->text[p->index] = '\0';
        errno = 0;

        p->token.kind = TOK_NUMBER;
        p->token.value.is_float = is_float;

        if (is_float) {
            p->token.value.real = strtod(p->text + start, NULL);
        } else {
            p->token.value.integer = strtoll(p->text + start, NULL, 10);
        }

        if (errno == ERANGE) {
            p->token.kind = TOK_INVALID;
        }

        p->text[p->index] = saved;
        return;
    }

    p->index++;

    switch (ch) {
        case '+': p->token.kind = TOK_PLUS;   break;
        case '-': p->token.kind = TOK_MINUS;  break;
        case '/': p->token.kind = TOK_SLASH;  break;
        case '(': p->token.kind = TOK_LPAREN; break;
        case ')': p->token.kind = TOK_RPAREN; break;
        case '*':
            if (p->index < p->length && p->text[p->index] == '*') {
                p->index++;
                p->token.kind = TOK_POWER;
            } else {
                p->token.kind = TOK_STAR;
            }
            break;
        default:
            p->token.kind = TOK_INVALID;
            break;
    }
}

/*
 * Temporary tokenizer test driver.
 * The final program will read expressions from files instead.
 */
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Tokenizer test: ./calc 'expression'\n");
        return EXIT_FAILURE;
    }

    Parser parser = {0};
    parser.text = argv[1];
    parser.length = strlen(argv[1]);

    do {
        next_token(&parser);
        printf("kind=%d position=%zu\n",
               (int)parser.token.kind, parser.token.position);
    } while (parser.token.kind != TOK_END &&
             parser.token.kind != TOK_INVALID);

    return EXIT_SUCCESS;
}