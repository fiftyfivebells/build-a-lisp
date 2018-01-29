#include "mpc.h"
#include "helpers.h"

#include <stdio.h>
#include <stdlib.h>

// if compiling on windows, compile these functions
#ifdef _WIN32
#include <string.h>

// Fake readline function
char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer)+1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy)-1] = '\0';
    return cpy;
}

// fake add_history function
void add_history(char* unused){}

// otherwise include the original headers
#else
#include <editline/readline.h>
#include <histedit.h>
#endif

typedef struct lval {
    int type;
        long num_long;
        double num_double;
        char* err;
        char* sym;
    int count;
    struct lval** cell;
} lval;

enum { LVAL_LONG, LVAL_DOUBLE, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

// pointer to a number lval
lval* lval_num_long(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_LONG;
    v->num_long = x;
    return v;
}

// pointer to a double lval
lval* lval_num_double(double x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_DOUBLE;
    v->num_double = x;
    return v;
}

// pointer to an error lval
lval* lval_err(int x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = x;
    return v;
}

// pointer to a symbol lval
lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

// pointer to a sexpr lval
lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

// this function deletes pointers to lvals
void lval_del(lval* v) {

    switch (v->type) {
        // do nothing in the case of a number or double
        case LVAL_LONG: break;
        case LVAL_DOUBLE: break;

        // free the string for err and sym
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;

        // if sexpr, delete all elements inside
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            // free memory allocated for pointers
            free(v->cell);
        break;
    }

    // free memory allocated for lval struct
    free(v);
}

lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;

    if (strchr(t->contents, '.') != NULL) {
        double x_double = strtof(t->contents, NULL);
            return errno != ERANGE ? lval_num_double(x_double) : lval_err("That's a bad number.");
    }

    long x = strtol(t->contents, null, 10);
    return errno != ERANGE ?
        lval_num_long(x) : lval_err("That's a bad number.");
}

lval* lval_read(mpc_ast_t* t) {

    // if it's a smybol or number, return the conversion of that type
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    // if it's the root or a sexpr, create an empty list
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }

    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }
}

lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

void lval_print(lval* v) {}

void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);

        // no trailing spaces if last element
        if (i != (v->count-1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lval* v) {
    switch(v->type) {
        case LVAL_LONG:   printf("%li", v->num_long); break;
        case LVAL_DOUBLE: printf("%lf", v->num_double); break;
        case LVAL_ERR:    printf("Error: %s", v->err); break;
        case LVAL_SEXPR:  lval_expr_print(v, '(', ')'); break;
    }
}

// prints an lval, but with a newline after
void lval_println(lval* v) { lval_print(v); putchar('\n'); }

lval eval_op(lval x, char* op, lval y) {

    // if either value is an error, return it
    if (x.type == LVAL_ERR) { return x; }
    if (y.type == LVAL_ERR) { return y; }

    if (x.type == LVAL_DOUBLE || y.type == LVAL_DOUBLE) {
        if (x.type == LVAL_LONG) { double x_num_double = x.num_long; x.num_double = x_num_double; }
        if (y.type == LVAL_LONG) { double y_num_double = y.num_long; y.num_double = y_num_double; }

        if (strcmp(op, "%") == 0) { return lval_err(LERR_BAD_OP); }
        if (strcmp(op, "+") == 0) { return lval_num_double(x.num_double + y.num_double); }
        if (strcmp(op, "-") == 0) { return lval_num_double(x.num_double - y.num_double); }
        if (strcmp(op, "*") == 0) { return lval_num_double(x.num_double * y.num_double); }
        if (strcmp(op, "/") == 0) { 
            return y.num_double == 0
                ? lval_err(LERR_DIV_ZERO)
                : lval_num_double(x.num_double / y.num_double);
        }
        if (strcmp(op, "^") == 0) { return lval_num_double(power(x.num_double, y.num_double)); }
        if (strcmp(op, "min") == 0) { return (x.num_double <= y.num_double) ? lval_num_double(x.num_double) : lval_num_double(y.num_double); }
        if (strcmp(op, "max") == 0) { return (x.num_double >= y.num_double) ? lval_num_double(x.num_double) : lval_num_double(y.num_double); }


    }

    // otherwise, do the math on the values
    if (strcmp(op, "+") == 0) { return lval_num_long(x.num_long + y.num_long); }
    if (strcmp(op, "-") == 0) { return lval_num_long(x.num_long - y.num_long); }
    if (strcmp(op, "*") == 0) { return lval_num_long(x.num_long * y.num_long); }
    if (strcmp(op, "/") == 0) { 
        return y.num_long == 0
            ? lval_err(LERR_DIV_ZERO)
            : lval_num_long(x.num_long / y.num_long);
    }
    if (strcmp(op, "%") == 0) { return lval_num_long(x.num_long % y.num_long); }
    if (strcmp(op, "^") == 0) { return lval_num_long(power(x.num_long, y.num_long)); }
    if (strcmp(op, "min") == 0) { return (x.num_long <= y.num_long) ? lval_num_long(x.num_long) : lval_num_long(y.num_long); }
    if (strcmp(op, "max") == 0) { return (x.num_long >= y.num_long) ? lval_num_long(x.num_long) : lval_num_long(y.num_long); }
    
    return lval_err(LERR_BAD_OP);
}    

lval eval(mpc_ast_t* t) {

    // base case: if tagged as number, return it directly
    if (strstr(t->tag, "number")) {
        errno = 0;

        if (strchr(t->contents, '.') != NULL) {
            double x_double = strtof(t->contents, NULL);
            return errno != ERANGE ? lval_num_double(x_double) : lval_err(LERR_BAD_NUM);
        }
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_num_long(x) : lval_err(LERR_BAD_NUM);
    }

    // operator is always the second child
    char* op = t->children[1]->contents;

    lval x = eval(t->children[2]);

    int i = 3;
    while(strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}

int main(int argc, char** argv) {

    // parsers
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr  = mpc_new("sexpr");
    mpc_parser_t* Expr   = mpc_new("expr");
    mpc_parser_t* Teddy  = mpc_new("teddy");

    // define my parsers with the following language
    mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
      number   : /-?[0-9]+(\\.[0-9]+)?/ ;                 \
      symbol   : '+' | '-' | '*' | '/' | '%' | '^' |      \
                \"min\" | \"max\" ;                       \
      sepr     : '(' <expr>* ')' ;                        \
      expr     : <number> | <symbol> | <sexpr> ;          \
      teddy    : /^/ <expr>* /$/ ;                        \
    ",
    Number, Operator, Expr, Teddy);

    puts("Teddy Version 0.0.0.0.1");
    puts("Welcome to the party!");
    puts("Press Ctrl+c to Exit\n");

    while(1) {

        char* input = readline("teddycat> ");
        add_history(input);

        // attempt to parse user input
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Teddy, &r)) {

            lval result = eval(r.output);
            lval_println(result);
            mpc_ast_delete(r.output);
        } else {
            // else print the error
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    // undefined and delete the parsers
    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Teddy);

    return 0;
}
