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

enum { LVAL_LONG, LVAL_DOUBLE, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

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
lval* lval_err(char* m) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
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

// pointer to empty qexpr lval
lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
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

        // if sexpr or qexpr, delete all elements inside
        case LVAL_QEXPR:
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

lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}


lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;

    if (strchr(t->contents, '.') != NULL) {
        double x_double = strtof(t->contents, NULL);
            return errno != ERANGE ? lval_num_double(x_double) : lval_err("That's a bad number.");
    }

    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ?
        lval_num_long(x) : lval_err("That's a bad number.");
}

lval* lval_read(mpc_ast_t* t) {

    // if it's a smybol or number, return the conversion of that type
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    // if it's the root, a sexpr, or a qexpr create an empty list
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
    if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }

    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}


void lval_print(lval* v);

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
        case LVAL_SYM:    printf("%s", v->sym); break;
        case LVAL_ERR:    printf("Error: %s", v->err); break;
        case LVAL_SEXPR:  lval_expr_print(v, '(', ')'); break;
        case LVAL_QEXPR:  lval_expr_print(v, '{', '}'); break;
    }
}

// prints an lval, but with a newline after
void lval_println(lval* v) { lval_print(v); putchar('\n'); }

// takes first element from s-expression and shifts the rest into its place
lval* lval_pop(lval* v, int i) {
    // get item at index i
    lval* x = v->cell[i];

    // move memory down one after taking item at i
    memmove(&v->cell[i], &v->cell[i+1],
        sizeof(lval*) * (v->count-i-1));

    v->count--;

    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

// takes the first element in the s-expression, deletes the rest
lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval* builtin_op(lval* a, char* op) {

    // check if all arguments are numbers, throw error if not
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_DOUBLE && a->cell[i]->type != LVAL_LONG) {
            lval_del(a);
            return lval_err("You need to give me numbers!");
        }
    }

    lval* x = lval_pop(a, 0);

    if ((strcmp(op, "-") == 0) && a->count == 0) {
        if (x->type == LVAL_DOUBLE) {
            x->num_double = -x->num_double;
        } else if (x->type == LVAL_LONG) {
            x->num_long = -x->num_long;
        }
    }

    while (a->count > 0) {

        lval* y = lval_pop(a, 0);

        if (x->type == LVAL_DOUBLE || y->type == LVAL_DOUBLE) {
            if (x->type == LVAL_LONG) { double x_double = x->num_long; x->type = LVAL_DOUBLE; x->num_double = x_double; }
            if (y->type == LVAL_LONG) { double y_double = y->num_long; y->type = LVAL_DOUBLE; y->num_double = y_double; }
            if (strcmp(op, "+") == 0) { x->num_double += y->num_double; }
            if (strcmp(op, "-") == 0) { x->num_double -= y->num_double; }
            if (strcmp(op, "*") == 0) { x->num_double *= y->num_double; }
            if (strcmp(op, "/") == 0) { 
                if (y->num_double == 0) {
                    lval_del(x); lval_del(y);
                    x = lval_err("Are you serious? You can't divide by zero!");
                    break;
                } 
                x->num_double = y->num_double;
            }
        } else if (y->type == LVAL_LONG) {
            if (strcmp(op, "+") == 0) { x->num_long += y->num_long; }
            if (strcmp(op, "-") == 0) { x->num_long -= y->num_long; }
            if (strcmp(op, "*") == 0) { x->num_long *= y->num_long; }
            if (strcmp(op, "/") == 0) { 
                if (y->num_long == 0) {
                    lval_del(x); lval_del(y);
                    x = lval_err("Are you serious? You can't divide by zero!");
                    break;
                } 
                x->num_long = y->num_long;
            }
        }
        lval_del(y);
    }

    lval_del(a);
    return x;
}

lval* lval_eval_sexpr(lval* v);

lval* lval_eval(lval* v) {
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }

    // anything other than an s-expression is returned as is
    return v;
}

lval* lval_eval_sexpr(lval* v) {

    // evaluate children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    // check for errors
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    // empty expression
    if (v->count == 0) { return v; }

    // single expression
    if (v->count == 1) { return lval_take(v, 0); }

    // make sure first element is a symbol
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f); lval_del(v);
        return lval_err("You need to start with a symbol!");
    }

    // call built-in operator
    lval* result = builtin_op(v, f->sym);
    lval_del(f);
    return result;
}

int main(int argc, char** argv) {

    // parsers
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr  = mpc_new("sexpr");
    mpc_parser_t* Qexpr  = mpc_new("qexpr");
    mpc_parser_t* Expr   = mpc_new("expr");
    mpc_parser_t* Teddy  = mpc_new("teddy");

    // define my parsers with the following language
    mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
      number   : /-?[0-9]+(\\.[0-9]+)?/ ;                 \
      symbol   : '+' | '-' | '*' | '/' | '%' | '^' |      \
                \"min\" | \"max\" ;                       \
      sexpr    :  '(' <expr>* ')' ;                       \
      qexpr    :  '{' <expr>* '}' ;                       \
      expr     : <number> | <symbol> | <sexpr> | qexpr ;  \
      teddy    : /^/ <expr>* /$/ ;                        \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Teddy);

    puts("Teddy Version 0.0.0.0.1");
    puts("Welcome to the party!");
    puts("Press Ctrl+c to Exit\n");

    while(1) {

        char* input = readline("teddycat> ");
        add_history(input);

        // attempt to parse user input
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Teddy, &r)) {
            lval* x = lval_eval(lval_read(r.output));
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        } else {
            // else print the error
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    // undefined and delete the parsers
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Teddy);

    return 0;
}
