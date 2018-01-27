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

typedef struct {
    int type;
    long num;
    long err;
} lval;

enum { LVAL_NUM, LVAL_ERR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

// number type lval
lval lval_num(long x) {
    lval v;
    v.type = LVAL_NUM;
    v.num = x;
    return v;
}

// error type lval
lval lval_err(int x) {
    lval v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
}

void lval_print(lval v) {
    switch(v.type) {
        case LVAL_NUM: printf("%li", v.num);
        break;

        case LVAL_ERR:
            if (v.err == LERR_DIV_ZERO) {
                printf("Error: you can't divide by zero!");
            }
            if (v.err == LERR_BAD_OP) {
                printf("That's a bad operator, don't use it!");
            }
            if (v.err == LERR_BAD_NUM) {
                printf("Bad number, shouldn't have used that!");
            }
        break;
    }
}

// prints an lval, but with a newline after
void lval_println(lval v) { lval_print(v); putchar('\n'); }
int main(int argc, char** argv) {

    // parsers
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Float = mpc_new("float");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Teddy = mpc_new("teddy");

    // define my parsers with the following language
    mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
      number   : /-?[0-9]+/ ;                             \
      float    : /[0-9]+(\\.[0-9][0-9]?)?/ ;              \
      operator : '+' | '-' | '*' | '/' | '%';             \
      expr     : <number> | '(' <operator> <expr>+ ')' ;  \
      teddy    : /^/ <operator> <expr>+ /$/ ;             \
    ",
    Number, Float, Operator, Expr, Teddy);

    puts("Teddy Version 0.0.0.0.1");
    puts("Welcome to the party!");
    puts("Press Ctrl+c to Exit\n");

    while(1) {

        char* input = readline("teddycat> ");
        add_history(input);

        // attempt to parse user input
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Teddy, &r)) {
            // on success print the ast
            mpc_ast_print(r.output);
            mpc_ast_delete(r.output);
        } else {
            // else print the error
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    // undefined and delete the parsers
    mpc_cleanup(4, Number, Operator, Expr, Teddy);

    return 0;
}
