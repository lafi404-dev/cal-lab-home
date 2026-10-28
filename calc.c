// Lafi Haneef Mulakkal 251ADB120
// https://github.com/lafi404-dev/cal-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: course assignment; ChatGPT (OpenAI), conversation history: https://chatgpt.com/share/6add3011-b14c-83eb-982b-4eb28d7ae81e
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

/* AI (ChatGPT): prompt context "Graded lab: Arithmetic Parser in C"
 * (the full assignment pasted in this conversation), followed by the
 * tokenizer source and requests for the next implementation steps.
 * ChatGPT helped generate the tokenizer, evaluator, file processing,
 * CLI, comments and tests. This assistance must remain disclosed.
 * Conversation history is linked in the resources header above.
 */

typedef enum {
    TOK_END, TOK_NUMBER, TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH,
    TOK_POWER, TOK_LPAREN, TOK_RPAREN, TOK_INVALID
} TokenKind;

/* A type tag keeps integer arithmetic exact, including division. */
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
    int allocation_failed;
} Parser;

/* AI (ChatGPT): tokenizer assistance based on the assignment supplied
 * in this conversation. offset includes complete earlier lines, even
 * comments and blank lines. index is never reset when skipping spaces,
 * so offset + index + 1 counts every original character, 1-based.
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
        /* Leave a malformed decimal point as the next invalid token.
         * This reports 2. at the dot, rather than at the digit 2.
         */
        if (p->index + 1 < p->length && p->text[p->index] == '.' &&
            isdigit((unsigned char)p->text[p->index + 1])) {
            is_float = 1;
            p->index++;
            while (p->index < p->length &&
                   isdigit((unsigned char)p->text[p->index])) {
                p->index++;
            }
        }
        /* Bound library conversion to our grammar's token. Without
         * this temporary terminator, strtod could accept forbidden e3.
         * Restoring one byte avoids repeatedly copying substrings.
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
        if (errno == ERANGE) p->token.kind = TOK_INVALID;
        p->text[p->index] = saved;
        return;
    }

    p->index++;
    switch (ch) {
        case '+': p->token.kind = TOK_PLUS; break;
        case '-': p->token.kind = TOK_MINUS; break;
        case '/': p->token.kind = TOK_SLASH; break;
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
        default: p->token.kind = TOK_INVALID; break;
    }
}

Value parse_expr(Parser *p, size_t *error);
Value parse_term(Parser *p, size_t *error);
Value parse_power(Parser *p, size_t *error);
Value parse_primary(Parser *p, size_t *error);

/* Keep the first failure while recursive calls unwind. */
void record_error(size_t *error, size_t position) {
    if (*error == 0) *error = position;
}

double as_double(Value value) {
    return value.is_float ? value.real : (double)value.integer;
}

/* AI (ChatGPT): evaluator assistance from the assignment and pasted
 * tokenizer. Capture the right operand's first token before parsing it:
 * division by zero and negative integer exponents report there, whereas
 * a non-finite floating result reports the operator that produced it.
 */
Value apply_operator(TokenKind op, Value left, Value right,
                     size_t operator_position, size_t right_position,
                     size_t *error) {
    Value result = {0};
    if (op == TOK_SLASH &&
        (right.is_float ? right.real == 0.0 : right.integer == 0)) {
        record_error(error, right_position);
        return result;
    }
    if (!left.is_float && !right.is_float) {
        switch (op) {
            case TOK_PLUS: result.integer = left.integer + right.integer; break;
            case TOK_MINUS: result.integer = left.integer - right.integer; break;
            case TOK_STAR: result.integer = left.integer * right.integer; break;
            case TOK_SLASH: result.integer = left.integer / right.integer; break;
            case TOK_POWER: {
                if (right.integer < 0) {
                    record_error(error, right_position);
                    return result;
                }
                long long base = left.integer;
                long long exponent = right.integer;
                result.integer = 1;
                /* Repeated squaring needs logarithmic time. Do not
                 * square after the last exponent bit: an unused square
                 * could overflow even when the required answer fits.
                 * Integer expression overflow is excluded by the spec.
                 */
                while (exponent > 0) {
                    if (exponent % 2 != 0) result.integer *= base;
                    exponent /= 2;
                    if (exponent > 0) base *= base;
                }
                break;
            }
            default: record_error(error, operator_position); break;
        }
        return result;
    }

    /* Promote only when either operand is floating. Retain that type
     * even for a whole-valued result, so later division stays floating.
     */
    result.is_float = 1;
    double a = as_double(left), b = as_double(right);
    switch (op) {
        case TOK_PLUS: result.real = a + b; break;
        case TOK_MINUS: result.real = a - b; break;
        case TOK_STAR: result.real = a * b; break;
        case TOK_SLASH: result.real = a / b; break;
        case TOK_POWER: result.real = pow(a, b); break;
        default:
            record_error(error, operator_position);
            return result;
    }
    if (!isfinite(result.real)) record_error(error, operator_position);
    return result;
}

Value parse_primary(Parser *p, size_t *error) {
    Value result = {0};
    if (*error != 0) return result;
    if (p->token.kind == TOK_NUMBER) {
        result = p->token.value;
        next_token(p);
        return result;
    }
    if (p->token.kind == TOK_LPAREN) {
        next_token(p);
        result = parse_expr(p, error);
        if (*error != 0) return result;
        if (p->token.kind != TOK_RPAREN) {
            record_error(error, p->token.position);
            return result;
        }
        next_token(p);
        return result;
    }
    /* No unary operators: an operator here is the offending token.
     * At line end the END token points at newline, or EOF + 1.
     */
    record_error(error, p->token.position);
    return result;
}

/* AI (ChatGPT): prompt "check it carefully, am i doen?"
 * Stress testing found that recursive exponent chains can exhaust the
 * call stack. Store pending powers on a heap stack instead, then reduce
 * from right to left. Parentheses still use the bounded grammar recursion.
 */
Value parse_power(Parser *p, size_t *error) {
    typedef struct {
        Value left;
        size_t operator_position;
        size_t right_position;
    } PendingPower;

    Value value = parse_primary(p, error);
    PendingPower *pending = NULL;
    size_t count = 0, capacity = 0;

    while (*error == 0 && p->token.kind == TOK_POWER) {
        if (count == capacity) {
            size_t limit = (size_t)-1 / sizeof(*pending);
            if (capacity > limit / 2) {
                p->allocation_failed = 1;
                record_error(error, p->token.position);
                break;
            }
            size_t next = capacity == 0 ? 16 : capacity * 2;
            if (next > limit) {
                p->allocation_failed = 1;
                record_error(error, p->token.position);
                break;
            }
            PendingPower *grown = realloc(pending, next * sizeof(*pending));
            if (grown == NULL) {
                p->allocation_failed = 1;
                record_error(error, p->token.position);
                break;
            }
            pending = grown;
            capacity = next;
        }

        size_t operator_position = p->token.position;
        next_token(p);
        pending[count++] = (PendingPower){
            value, operator_position, p->token.position
        };
        value = parse_primary(p, error);
    }

    /* The last primary is the innermost exponent. Popping the saved
     * left operands computes a ** (b ** c), preserving right association
     * without a C function call for every power in an unbounded line.
     */
    while (count > 0 && *error == 0) {
        PendingPower item = pending[--count];
        value = apply_operator(TOK_POWER, item.left, value,
                               item.operator_position, item.right_position,
                               error);
    }
    free(pending);
    return value;
}

Value parse_term(Parser *p, size_t *error) {
    Value left = parse_power(p, error);
    /* Each operand is already a power; looping gives * and / left
     * associativity while preserving the higher precedence of **.
     */
    while (*error == 0 &&
           (p->token.kind == TOK_STAR || p->token.kind == TOK_SLASH)) {
        TokenKind op = p->token.kind;
        size_t operator_position = p->token.position;
        next_token(p);
        size_t right_position = p->token.position;
        Value right = parse_power(p, error);
        if (*error == 0) {
            left = apply_operator(op, left, right,
                                  operator_position, right_position, error);
        }
    }
    return left;
}

Value parse_expr(Parser *p, size_t *error) {
    Value left = parse_term(p, error);
    /* Terms finish multiplication before this outer addition loop,
     * which makes precedence follow the grammar in the assignment.
     */
    while (*error == 0 &&
           (p->token.kind == TOK_PLUS || p->token.kind == TOK_MINUS)) {
        TokenKind op = p->token.kind;
        size_t operator_position = p->token.position;
        next_token(p);
        size_t right_position = p->token.position;
        Value right = parse_term(p, error);
        if (*error == 0) {
            left = apply_operator(op, left, right,
                                  operator_position, right_position, error);
        }
    }
    return left;
}

/*
 * AI assistance: ChatGPT, responding to the arithmetic-parser assignment
 * supplied in this conversation, helped implement file and CLI handling.
 * Conversation history is linked in the resources header above.
 */
#define STUDENT_NAME "lafi"
#define STUDENT_LASTNAME "mulakkal"
#define STUDENT_ID "251adb120"

char *append_text(const char *a, const char *b) {
    size_t na = strlen(a), nb = strlen(b);
    if (nb == (size_t)-1 || na > (size_t)-1 - nb - 1) {
        return NULL;
    }
    char *result = malloc(na + nb + 1);
    if (result != NULL) {
        memcpy(result, a, na);
        memcpy(result + na, b, nb + 1);
    }
    return result;
}

char *join_path(const char *folder, const char *name) {
    char *prefix = append_text(folder, "/");
    if (prefix == NULL) return NULL;
    char *result = append_text(prefix, name);
    free(prefix);
    return result;
}

/* Strip trailing slashes before selecting the last path component. */
char *input_base(const char *path, int strip_txt) {
    size_t end = strlen(path);
    while (end > 1 && path[end - 1] == '/') end--;
    size_t start = end;
    while (start > 0 && path[start - 1] != '/') start--;
    size_t length = end - start;
    if (strip_txt && length >= 4 &&
        memcmp(path + end - 4, ".txt", 4) == 0) {
        length -= 4;
    }
    char *base = malloc(length + 1);
    if (base != NULL) {
        memcpy(base, path + start, length);
        base[length] = '\0';
    }
    return base;
}

int ensure_output_folder(const char *path) {
    if (mkdir(path, 0777) == 0) return 0;
    struct stat info;
    if (errno == EEXIST && stat(path, &info) == 0 && S_ISDIR(info.st_mode)) {
        return 0;
    }
    return -1;
}

/* Grow geometrically: long lines require O(n) total copying, not O(n^2).
 * Keep newline bytes in length so offsets include every original byte.
 * Return 1 for a line, 0 for EOF, and -1 for an I/O/allocation failure.
 */
int read_expression_line(FILE *fp, char **buffer, size_t *capacity,
                         size_t *length) {
    *length = 0;
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (*length + 1 >= *capacity) {
            if (*capacity > (size_t)-1 / 2) return -1;
            size_t next = *capacity == 0 ? 256 : *capacity * 2;
            char *grown = realloc(*buffer, next);
            if (grown == NULL) return -1;
            *buffer = grown;
            *capacity = next;
        }
        (*buffer)[(*length)++] = (char)c;
        if (c == '\n') break;
    }
    if (ferror(fp)) return -1;
    if (*length == 0) return 0;
    (*buffer)[*length] = '\0';
    return 1;
}

int process_file(const char *input, const char *outdir) {
    struct stat info;
    if (stat(input, &info) != 0 || !S_ISREG(info.st_mode)) return -1;
    FILE *source = fopen(input, "rb");
    if (source == NULL) return -1;

    /* Stage results privately: an error anywhere must discard ALL earlier
     * results, without retaining an unbounded results array in memory.
     */
    FILE *staged = tmpfile();
    if (staged == NULL) {
        fclose(source);
        return -1;
    }

    char *line = NULL;
    size_t capacity = 0, length = 0, offset = 0, error = 0;
    int failed = 0, status;
    while ((status = read_expression_line(source, &line, &capacity, &length)) == 1) {
        size_t expression_length = length;
        if (line[length - 1] == '\n') expression_length--;
        line[expression_length] = '\0';

        size_t first = 0;
        while (first < expression_length &&
               (line[first] == ' ' || line[first] == '\t')) first++;

        if (first < expression_length && line[first] != '#') {
            Parser parser = {0};
            parser.text = line;
            parser.length = expression_length;
            parser.offset = offset;
            next_token(&parser);
            Value result = parse_expr(&parser, &error);
            if (parser.allocation_failed) failed = 1;
            if (error == 0 && parser.token.kind != TOK_END) {
                record_error(&error, parser.token.position);
            }
            if (error != 0) break;
            int written = result.is_float
                ? fprintf(staged, "%.15g\n", result.real)
                : fprintf(staged, "%lld\n", result.integer);
            if (written < 0) { failed = 1; break; }
        }
        offset += length;
    }
    if (status == -1) failed = 1;
    free(line);
    if (fclose(source) == EOF) failed = 1;

    char *base = input_base(input, 1);
    char *name = base == NULL ? NULL : append_text(base,
        "_" STUDENT_NAME "_" STUDENT_LASTNAME "_" STUDENT_ID ".txt");
    char *path = name == NULL ? NULL : join_path(outdir, name);
    free(base);
    free(name);
    if (path == NULL) failed = 1;

    FILE *output = failed ? NULL : fopen(path, "wb");
    if (output == NULL) failed = 1;
    if (!failed) {
        if (error != 0) {
            if (fprintf(output, "ERROR:%zu\n", error) < 0) failed = 1;
        } else if (fseek(staged, 0, SEEK_SET) != 0) {
            failed = 1;
        } else {
            char block[8192];
            size_t n;
            while ((n = fread(block, 1, sizeof(block), staged)) > 0) {
                if (fwrite(block, 1, n, output) != n) {
                    failed = 1;
                    break;
                }
            }
            if (ferror(staged)) failed = 1;
        }
    }
    if (output != NULL && fclose(output) == EOF) failed = 1;
    if (fclose(staged) == EOF) failed = 1;
    free(path);
    return failed ? -1 : 0;
}

typedef struct InputFile {
    char *path;
    struct InputFile *next;
} InputFile;

int process_directory(const char *directory, const char *outdir) {
    DIR *dir = opendir(directory);
    if (dir == NULL) return -1;
    InputFile *files = NULL;
    int failed = 0;

    /* Snapshot input names before writing. Otherwise an output folder
     * equal to the input folder could feed new results back as inputs.
     */
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (entry == NULL) {
            if (errno != 0) failed = 1;
            break;
        }
        size_t length = strlen(entry->d_name);
        if (length < 4 || strcmp(entry->d_name + length - 4, ".txt") != 0) {
            continue;
        }
        char *path = join_path(directory, entry->d_name);
        if (path == NULL) { failed = 1; break; }
        struct stat info;
        if (stat(path, &info) != 0) {
            free(path);
            failed = 1;
            break;
        }
        if (!S_ISREG(info.st_mode)) { free(path); continue; }
        InputFile *item = malloc(sizeof(*item));
        if (item == NULL) { free(path); failed = 1; break; }
        item->path = path;
        item->next = files;
        files = item;
    }
    if (closedir(dir) != 0) failed = 1;
    while (files != NULL) {
        InputFile *item = files;
        files = item->next;
        if (!failed && process_file(item->path, outdir) != 0) failed = 1;
        free(item->path);
        free(item);
    }
    return failed ? -1 : 0;
}

int main(int argc, char *argv[]) {
    const char *input = NULL, *directory = NULL, *outdir = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dir") == 0) {
            if (directory != NULL || i + 1 == argc) goto usage;
            directory = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 ||
                   strcmp(argv[i], "--output-dir") == 0) {
            if (outdir != NULL || i + 1 == argc) goto usage;
            outdir = argv[++i];
        } else if (argv[i][0] == '-' || input != NULL) {
            goto usage;
        } else {
            input = argv[i];
        }
    }
    if ((input == NULL) == (directory == NULL)) goto usage;

    char *default_folder = NULL;
    if (outdir == NULL) {
        char *base = input_base(input != NULL ? input : directory, input != NULL);
        if (base != NULL) {
            default_folder = append_text(base, "_" STUDENT_NAME "_" STUDENT_ID);
        }
        free(base);
        if (default_folder == NULL) {
            fprintf(stderr, "Cannot allocate output folder name.\n");
            return EXIT_FAILURE;
        }
        outdir = default_folder;
    }
    int failed = ensure_output_folder(outdir);
    if (failed == 0) {
        failed = input != NULL ? process_file(input, outdir)
                               : process_directory(directory, outdir);
    }
    if (failed != 0) fprintf(stderr, "Cannot process input or write output.\n");
    free(default_folder);
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;

usage:
    fprintf(stderr, "Usage: calc [-d DIR | --dir DIR] "
                    "[-o OUTDIR | --output-dir OUTDIR] [input.txt]\n");
    return EXIT_FAILURE;
}
