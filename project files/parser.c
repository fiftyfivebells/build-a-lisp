// to compile: cc -std=c99 -Wall parser.c mpc.c -ledit -lm -o parsing

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

// some forward declarations
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

// lisp values
enum { LVAL_LONG, LVAL_DOUBLE, LVAL_ERR, 
    LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR, LVAL_FUN };

typedef lval*(*lbuiltin)(lenv*, lval*);

typedef struct lval {
    int type;

    long num_long;
    double num_double;
    char* err;
    char* sym;

    lbuiltin builtin;
    lenv* env;
    lval* formals;
    lval* body;

    int count;
    struct lval** cell;
} lval;

struct lenv {
    lenv* par;
    int count;
    char** syms;
    lval** vals;
};

lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lenv));
    e->par = NULL;
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

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
lval* lval_err(char* fmt, ...) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;

    va_list va;
    va_start(va, fmt);

    v->err = malloc(512);

    vsnprintf(v->err, 511, fmt, va);

    v->err = realloc(v->err, strlen(v->err)+1);

    va_end(va);

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

// pointer to a function lval
lval* lval_fun(lbuiltin func) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->builtin = func;
    return v;
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
        case LVAL_FUN:    
            if (v->builtin) {
                printf("<builtin>");
            } else {
                printf("(\\ "); lval_print(v->formals);
                putchar(' '); lval_print(v->body); putchar(')');
            }
        break;
    }
}

// prints an lval, but with a newline after
void lval_println(lval* v) { lval_print(v); putchar('\n'); }

// forward declare lenv_copy, lenv_put and lval_del
lenv* lenv_copy(lenv* e);
void lval_del(lval* v);
void lenv_put(lenv* e, lval* k, lval* v);

// copies an lval
lval* lval_copy(lval* v) {
    lval* x = malloc(sizeof(lval));
    x->type = v->type;

    switch(v->type) {
        // numbers and functions get copied directly
        case LVAL_DOUBLE: x->num_double = v->num_double; break;
        case LVAL_LONG:   x->num_long = v->num_long; break;
        case LVAL_FUN:    
            if (v->builtin) {
                x->builtin = v->builtin;
            } else {
                x->builtin = NULL;
                x->env = lenv_copy(v->env);
                x->formals = lval_copy(v->formals);
                x->body = lval_copy(v->body);
            }
        break;

        // copy strings with malloc
        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err); break;
        
        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym); break;
        
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval) * x->count);
            for (int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }

    return x;
}

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

// copy an environment
lenv* lenv_copy(lenv* e) {
    lenv* n = malloc(sizeof(lenv));
    n->par = e->par;
    n->count = e->count;
    n->syms = malloc(sizeof(char*) * n->count);
    n->vals = malloc(sizeof(lval*) * n->count);
    for (int i = 0; i < e->count; i++) {
        n->syms[i] = malloc(strlen(e->syms[i]) + 1);
        strcpy(n->syms[i], e->syms[i]);
        n->vals[i] = lval_copy(e->vals[i]);
    }
    return n;
}

void lenv_def(lenv* e, lval* k, lval* v) {
    while (e->par) {
        e = e->par;
    }

    lenv_put(e, k, v);
}

// delete an environment
void lenv_del(lenv* e) {
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

// get a lisp value from an environment
lval* lenv_get(lenv* e, lval* k) {
    for (int i = 0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }

    if (e->par) {
        return lenv_get(e->par, k);
    } else {
        return lval_err("The symbol '%s' is not bound!", k->sym);
    }
}

// takes an env, a variable name, and a value. puts the variable into
// the env as the value, or overwrites if it exists
void lenv_put(lenv* e, lval* k, lval* v) {
    for (int i = 0; i < e->count; i++) {

        // if variable exists, delete and overwrite
        if (strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
        }
    }

    // otherwise, make space and add it
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);

    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = malloc(strlen(k->sym)+1);
    strcpy(e->syms[e->count-1], k->sym);
}

// this function deletes pointers to lvals
void lval_del(lval* v) {

    switch (v->type) {
        // do nothing in the case of a number or double
        case LVAL_LONG: break;
        case LVAL_DOUBLE: break;
        case LVAL_FUN: 
            if (!v->builtin) {
                lenv_del(v->env);
                lval_del(v->formals);
                lval_del(v->body);
            }
        break;

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

// user-defined function
lval* lval_lambda(lval* formals, lval* body) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;

    v->builtin = NULL;

    v->env = lenv_new();

    v->formals = formals;
    v->body = body;
    return v;
}

lval* lval_eval_sexpr(lenv* e, lval* v);

lval* lval_eval(lenv* e, lval* v) {
    // check the environment for the lval first, and get it's
    // value back if it's in there
    if (v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }

    // otherwise, evaluate the s-expression or simply return the value
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }

    return v;
}

lval* builtin_op(lenv* e, lval* a, char* op) {

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
            if (strcmp(op, "%") == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("You can't use modulo with doubles!");
                break;
            }
	    if (strcmp(op, "^") == 0) { x->num_double = power(x->num_double, y->num_double); }
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
            if (strcmp(op, "%") == 0) { x->num_long = x->num_long % y->num_long; }
	    if (strcmp(op, "^") == 0) { x->num_long = power(x->num_long, y->num_long); }
        }
        lval_del(y);
    }

    lval_del(a);
    return x;
}

char* ltype_name(int t) {
    switch(t) {
	case LVAL_FUN: return "Function";
	case LVAL_ERR: return "Error";
	case LVAL_SYM: return "Symbol";
	case LVAL_DOUBLE:
	case LVAL_LONG: return "Number";
	case LVAL_SEXPR: return "S-Expression";
	case LVAL_QEXPR: return "Q-Expression";
	default: return "Unknown";
    }
}



#define LASSERT(args, cond, fmt, ...) \
    if (!(cond)) { \
	lval* err= lval_err(fmt,##__VA_ARGS__); \
	lval_del(args); \
	return err; \
    }

#define LASSERT_TYPE(func, args, index, expect) \
    LASSERT(args, args->cell[index]->type == expect, \
        "Function '%s' passed incorrect type for argument %i. " \
        "Got %s, Expected %s.", \
        func, index, ltype_name(args->cell[index]->type), ltype_name(expect))

#define LASSERT_NUM(func, args, num) \
    LASSERT(args, args->count == num, \
        "Function '%s' passed incorrect number of arguments. " \
        "Got %i, Expected %i.", \
        func, args->count, num)

#define LASSERT_NOT_EMPTY(func, args, index) \
    LASSERT(args, args->cell[index]->count != 0, \
        "Function '%s' passed {} for argument %i.", func, index);



lval* builtin_var(lenv* e, lval* a, char* func) {
    LASSERT_TYPE(func, a, 0, LVAL_QEXPR);

    lval* syms = a->cell[0];
    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
        "Function '%s' can't define a non-symbol. Got %s, expected %s.", func,
        ltype_name(syms->cell[i]->type),
        ltype_name(LVAL_SYM));
    }

    LASSERT(a, (syms->count == a->count - 1),
    "You gave %s too many arguments for symbols! Got %i, expected %i!", func, syms->count, a->count-1);

    for (int i = 0; i < syms->count; i++) {
        if (strcmp(func, "def") == 0) {
            lenv_def(e, syms->cell[i], a->cell[i+1]);
        }

        if (strcmp(func, "=") == 0) {
            lenv_put(e, syms->cell[i], a->cell[i+1]);
        }
    }

    lval_del(a);
    return lval_sexpr();
}

lval* builtin_def(lenv* e, lval* a) {
    return builtin_var(e, a, "def");
}

lval* builtin_put(lenv* e, lval* a) {
    return builtin_var(e, a, "=");
}

lval* builtin_head(lenv* e, lval* a) {
    // error conditions
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "You gave 'head' the wrong type for the first argument! "
	    "You gave it a %s, but it wanted a %s.",
	ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));    
    LASSERT(a, a->count == 1,
        "You passed 'head' too many arguments! "
        "Got %i, but it needs %i.",
        a->count, 1);
    LASSERT(a, a->count != 0, 
        "You passed 'head' an empty list!");

    lval* v = lval_take(a, 0);

    while (v->count > 1) { lval_del(lval_pop(v, 1)); }
    return v;
}

lval* builtin_tail(lenv* e, lval* a) {
    // error conditions
    LASSERT(a, a->count == 1,
        "You passed 'tail' too many arguments! "
        "Got %i, but it needs %i.",
        a->count, 1);
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "You passed 'tail' the wrong thing!");
    LASSERT(a, a->count != 0, 
        "You passed 'tail' an empty list!");

    lval* v = lval_take(a, 0);

    // delete first element and return the rest
    lval_del(lval_pop(v, 0));
    return v;    
}

lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lenv* e, lval* a) {
    LASSERT(a, a->count == 1, 
    "You gave 'eval' too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "You gave 'eval' the wrong type!");
    
    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

lval* lval_join(lval* x, lval* y) {
    while (y->count) {
        x = lval_add(x, lval_pop(y, 0));
    }

    // delete now empty y and return x
    lval_del(y);
    return x;
}

lval* builtin_join(lenv* e, lval* a) {
    for (int i = 0; i < a->count; i++) {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
            "You gave 'join' the wrong thing!");
    }

    lval* x = lval_pop(a, 0);

    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}

lval* builtin_len(lenv* e, lval* a) {
    LASSERT(a, a->count == 1,
        "Function 'len' only takes ONE argument!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "You passed 'len' the wrong thing!");    

    lval* v = lval_num_long(0);
    v->type = LVAL_LONG;
    v->num_long = a->cell[0]->count;

    lval_del(a);
    return v;
}

lval* builtin_init(lenv* e, lval* a) {
    LASSERT(a, a->count == 1,
        "You passed 'init' too many arguments! "
        "Got %i, but it needs %i.",
        a->count, 1);
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "You passed 'init' the wrong thing!");
    LASSERT(a, a->count != 0, 
        "You passed 'init' an empty list!");

    lval* v = lval_take(a, 0);

    lval_del(lval_pop(v, v->count - 1));
    lval_del(a);

    return v;
}

lval* builtin_cons(lenv* e, lval* a) {
    LASSERT(a, a->count == 2,
        "Function 'cons' needs one value and one list!");
    LASSERT(a, a->cell[0]->type != LVAL_QEXPR,
        "Function 'cons' takes a simple value as a first argument, not a list!")

    lval* b = lval_qexpr();
    lval_add(b, lval_pop(a, 0));
    
    int i = 0;
    while(i <= a->count) {
        while (a->cell[i]->count) {
            lval_add(b, lval_pop(a->cell[i], 0));
        }
        i++;
    }

    return b;
}

lval* builtin_add(lenv* e, lval* a) {
    return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
    return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
    return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
    return builtin_op(e, a, "/");
}

lval* builtin_mod(lenv* e, lval* a) {
    return builtin_op(e, a, "%");
}

lval* builtin_pow(lenv* e, lval* a) {
    return builtin_op(e, a, "^");
}

lval* builtin_ord(lenv* e, lval* a, char* op) {
    LASSERT_NUM(op, a, 2);
    LASSERT_TYPE(op, a, 0, LVAL_LONG);
    LASSERT_TYPE(op, a, 1, LVAL_LONG);
    LASSERT_TYPE(op, a, 0, LVAL_DOUBLE);
    LASSERT_TYPE(op, a, 1, LVAL_DOUBLE);

    int r;
    if (strcmp(op, ">") == 0) {
        r = (a->cell[0]->num_long > a->cell[1]->num_long ||
            a->cell[0]->num_double > a->cell[1]->num_double);
    }
    if (strcmp(op, "<") == 0) {
        r = (a->cell[0]->num_long < a->cell[1]->num_long ||
            a->cell[0]->num_double < a->cell[1]->num_double);
    }
    if (strcmp(op, ">=") == 0) {
        r = (a->cell[0]->num_long >= a->cell[1]->num_long ||
            a->cell[0]->num_double >= a->cell[1]->num_double);
    }
    if (strcmp(op, "<=") == 0) {
        r = (a->cell[0]->num_long <= a->cell[1]->num_long ||
            a->cell[0]->num_double <= a->cell[1]->num_long);
    }

    lval_del(a);
    return lval_num_long(r);
}

lval* builtin_gt(lenv* e, lval* a) {
    return builtin_ord(e, a, ">");
}

lval* builtin_lt(lenv* e, lval* a) {
    return builtin_ord(e, a, "<");
}

lval* builtin_gte(lenv* e, lval* a) {
    return builtin_ord(e, a, ">=");
}

lval* builtin_lte(lenv* e, lval* a) {
    return builtin_ord(e, a, "<=");
}


int lval_eq(lval* x, lval* y) {
    // different types are always !=
    if (x->type != y->type) { return 0; }

    // compare types
    switch (x->type) {
        // compare number values
        case LVAL_LONG: return (x->num_long == y->num_long);
        case LVAL_DOUBLE: return (x->num_double == y->num_double);

        // compare string values
        case LVAL_ERR: return (strcmp(x->err, y->err) == 0);
        case LVAL_SYM: return (strcmp(x->sym, y->sym) == 0);

        // compare builtins, otherwise compare body and formals
        case LVAL_FUN:
            if (x->builtin || y->builtin) {
                return x->builtin == y->builtin;
            } else {
                return lval_eq(x->formals, y->formals)
                    && lval_eq(x->body, y->body);
            }

        // if it's a list, compare all the elements
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            if (x->count != y->count) { return 0; }
            for (int i = 0; i < x->count; i++) {
                // return 0 if any element is not equal
                if (!lval_eq(x->cell[i], y->cell[i])) { return 0; }
            }
            // if it makes it through the loop, the lists must be equal
            return 1;
        break;
    }
    return 0;
}

lval* builtin_cmp(lenv* e, lval* a, char* op) {
    LASSERT_NUM(op, a, 2);
    int r;

    if (strcmp(op, "==") == 0) {
        r = lval_eq(a->cell[0], a->cell[1]);
    }
    if (strcmp(op, "!=") == 0) {
        r = !lval_eq(a->cell[0], a->cell[1]);
    }

    lval_del(a);
    return lval_num_long(r);
}

lval* builtin_eq(lenv* e, lval* a) {
    return builtin_cmp(e, a, "==");
}

lval* builtin_ne(lenv* e, lval* a) {
    return builtin_cmp(e, a, "!=");
}

lval* builtin_if(lenv* e, lval* a) {
    LASSERT_NUM("if", a, 3);
    LASSERT_TYPE("if", a, 0, LVAL_LONG);
    LASSERT_TYPE("if", a, 0, LVAL_DOUBLE);
    LASSERT_TYPE("if", a, 1, LVAL_QEXPR);
    LASSERT_TYPE("if", a, 2, LVAL_QEXPR);

    lval* x;
    a->cell[1]->type = LVAL_SEXPR;
    a->cell[2]->type = LVAL_SEXPR;

    if (a->cell[0]->num_long || a->cell[0]->num_double) {
        // if the condition is true, evaluate the first expression
        x = lval_eval(e, lval_pop(a, 1));
    } else{
        // otherwise evaluate the second expression
        x = lval_eval(e, lval_pop(a, 2));
    }

    lval_del(a);
    return x;
}

lval* builtin_print(lenv* e, lval* a) {
    for (int i = 0; i < e->count; i++) {
        printf("%d. %s\n", i+1, e->syms[i]);
    }
    lval_del(a);
    return lval_sexpr();
}

lval* builtin_lambda(lenv* e, lval* a) {
    LASSERT_NUM("\\", a, 2);
    LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
    LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

    for (int i = 0; i < a->cell[0]->count; i++) {
        LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
        "Can't define a non-symbol. You gave a %s, but I expected a %s.",
        ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
    }

    lval* formals = lval_pop(a, 0);
    lval* body = lval_pop(a, 0);
    lval_del(a);

    return lval_lambda(formals, body);
}

lval* lval_call(lenv* e, lval* f, lval* a) {

    // if builtin, call the builtin
    if (f->builtin) { return f->builtin(e, a); }

    int given = a->count;
    int total = f->formals->count;

    while(a->count) {
        // if no more formal arguments to bind
        if (f->formals->count == 0) {
            lval_del(a); return lval_err(
                "You gave the function too many arguments! "
                "Got %i, wanted %i.", given, total);
        }

        lval* sym = lval_pop(f->formals, 0);

        if (strcmp(sym->sym, "&") == 0) {
            if (f->formals->count != 1) {
                lval_del(a);
                return lval_err("Format invalid. "
                "Symbol '&' not followed by a single symbol.");
            }

            lval* nsym = lval_pop(f->formals, 0);
            lenv_put(f->env, nsym, builtin_list(e, a));
            lval_del(sym); lval_del(nsym);
            break;
        }

        lval* val = lval_pop(a, 0);

        lenv_put(f->env, sym, val);
        lval_del(sym); lval_del(val);
    }

    // now argument list is all bound, delete it
    lval_del(a);

    // if & remains in list, bind to empty list
    if (f->formals->count > 0 &&
        strcmp(f->formals->cell[0]->sym, "&") == 0) {
            if (f->formals->count != 2) {
                return lval_err("Function format invalid. "
                "Symbol '&' not followed by single symbol.");
            }

            lval_del(lval_pop(f->formals, 0));

            lval* sym = lval_pop(f->formals, 0);
            lval* val = lval_qexpr();

            lenv_put(f->env, sym, val);
            lval_del(sym); lval_del(val);
        }

    // if all formals bound, evaluate
    if (f->formals->count == 0) {

        f->env->par = e;

        return builtin_eval(
            f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
    } else {
        // otherwise, return partially evaluated function
        return lval_copy(f);
    }
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k); lval_del(v);
}

void lenv_add_builtins(lenv* e) {
    // list functions
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);
    lenv_add_builtin(e,  "len", builtin_len);
    lenv_add_builtin(e, "init", builtin_init);
    lenv_add_builtin(e, "cons", builtin_cons);

    // arithmetic functions
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
    lenv_add_builtin(e, "%", builtin_mod);
    lenv_add_builtin(e, "^", builtin_pow);

    // variable functions
    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "=", builtin_put);
    lenv_add_builtin(e, "print", builtin_print);

    // comparison functions
    lenv_add_builtin(e, "if", builtin_if);
    lenv_add_builtin(e, "==", builtin_eq);
    lenv_add_builtin(e, "!=", builtin_ne);
    lenv_add_builtin(e, ">",  builtin_gt);
    lenv_add_builtin(e, "<",  builtin_lt);
    lenv_add_builtin(e, ">=", builtin_gte);
    lenv_add_builtin(e, "<=", builtin_lte);

    // function functions
    lenv_add_builtin(e, "\\", builtin_lambda);
}

lval* lval_eval_sexpr(lenv* e, lval* v) {

    // evaluate children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
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
    if (f->type != LVAL_FUN) {
        lval* err = lval_err(
            "S-Expression started with incorrect type. "
            "Got %s, wanted a %s.",
            ltype_name(f->type), ltype_name(LVAL_FUN));
        
        lval_del(f); lval_del(v);
        return err;
    }

    // call built-in operator
    lval* result = lval_call(e, f, v);
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
    "                                                      \
      number   : /-?[0-9]+(\\.[0-9]+)?/ ;                  \
      symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<%>^!&]+/ ;       \
      sexpr    :  '(' <expr>* ')' ;                        \
      qexpr    :  '{' <expr>* '}' ;                        \
      expr     : <number> | <symbol> | <sexpr> | <qexpr> ; \
      teddy    : /^/ <expr>* /$/ ;                         \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Teddy);

    puts("Teddy Version 0.0.0.0.1");
    puts("Welcome to the party!");
    puts("Press Ctrl+c to Exit\n");

    lenv* e = lenv_new();
    lenv_add_builtins(e);

    while(1) {

        char* input = readline("teddycat> ");
        add_history(input);

        // attempt to parse user input
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Teddy, &r)) {
            lval* x = lval_eval(e, lval_read(r.output));
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

    // delete the environment after use
    lenv_del(e);

    // undefined and delete the parsers
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Teddy);

    return 0;
}
