#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "mpc.h"

static char* name = "lip";
static char* version = "0.0.4";

/*
 * LIP
 * is a toy lisp-like language I'm writing
 * while reading the book "build your own lisp"
 */

enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };

typedef struct lval {
  int type;
  long num;
  char *err;
  char *sym;
  int count;
  struct lval** cell;
} lval;

/*
 * Code notes:
 *   "lval" == "lis?p value". This is used throughout the next section
 *   Expressions are lvals too, but that's liable to change
 */

lval * lval_num(long input) {
  lval *r = malloc(sizeof(lval));
  r->type = LVAL_NUM;
  r->num = input;
  return r;
}

lval * lval_err(char* message) {
  lval *r = malloc(sizeof(lval));
  r->type = LVAL_ERR;
  r->err = malloc(strlen(message) + 1);
  strcpy(r->err, message);
  return r;
}

lval * lval_sym(char* s) {
  lval *r = malloc(sizeof(lval));
  r->type = LVAL_SYM;
  r->sym = malloc(strlen(s) + 1);
  strcpy(r->sym, s);
  return r;
}

lval * lval_sexpr(void) {
  lval *r = malloc(sizeof(lval));
  r->type = LVAL_SEXPR;
  r->count = 0;
  r->cell = NULL;
  return r;
}

void lval_free(lval *l) {
  switch (l->type) {
    case LVAL_ERR:
      free(l->err);
      break;
    case LVAL_SYM:
      free(l->sym);
      break;
    case LVAL_SEXPR:
      for (int i=0; i<l->count; i++) {
        lval_free(l->cell[i]);
      }
      free(l->cell);
    default:
      break;
  }
  free(l);
}

void lval_expr_print(lval *l, char open, char close);

void lval_show(lval *input) {
  switch (input->type) {
    case LVAL_NUM:
      printf("%li", input->num);
      break;
    case LVAL_ERR:
      printf("Error: %s", input->err);
      break;
    case LVAL_SYM:
      printf("%s", input->sym);
      break;
    case LVAL_SEXPR:
      lval_expr_print(input, '(', ')');
      break;
  }
}

void lval_expr_print(lval *l, char open, char close) {
  putchar(open);
  for (int i=0; i<l->count; i++) {
    lval_show(l->cell[i]);
    if (i != (l->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval *input) {
  lval_show(input);
  putchar('\n');
}

// Predeclare for...
lval * evaluate_sexpr(lval *input);

lval * evaluate_lval(lval *input) {
  if (input->type == LVAL_SEXPR) {
    return evaluate_sexpr(input);
  } else {
    return input;
  }
}

lval * lval_pop(lval *input, int i) {
  if (i >= input->count) {
    return lval_err("Attempted to pop beyond the bounds of a lval's cell space");
  }
  lval *x = input->cell[i];
  // Move everything forward one spot
  memmove(&input->cell[i], &input->cell[i+1], sizeof(lval*) * (input->count-i-1));
  input->count--;
  input->cell = realloc(input->cell, sizeof(lval*) * input->count);
  return x;
}

lval * evaluate_builtin(lval *x, char *operator) {
  for (int i=0; i<x->count; i++) {
    if (x->cell[i]->type != LVAL_NUM) {
      lval_free(x);
      return lval_err("Non-number passed as argument to a function that takes numbers");
    }
  }
  lval *first = lval_pop(x, 0);
  // Unary negation
  if (x->count == 0) {
    if (strcmp(operator, "-") == 0) {
      first->num = -first->num;
    } // else if (strcmp(operator, "+") == 0) {
  }
  // Do the number dance
  while (x->count > 0) {
    lval *next = lval_pop(x, 0);
    if (strcmp(operator, "+") == 0) {
      first->num += next->num;
    } else if (strcmp(operator, "-") == 0) {
      first->num -= next->num;
    } else if (strcmp(operator, "*") == 0) {
      first->num *= next->num;
    } else if (strcmp(operator, "^") == 0) {
      first->num = powl(first-> num, next->num);
    } else if (strcmp(operator, "%") == 0) {
      first->num %= next->num;
    } else if (strcmp(operator, "/") == 0) {
      if (next->num == 0) {
        lval_free(first);
        lval_free(next);
        first = lval_err("Cannot divide by zero fool");
        break;
      }
      first->num /= next->num;
    }
    lval_free(next);
  }
  lval_free(x);
  return first;
}

lval * lval_take(lval *input, int i) {
  lval *x = lval_pop(input, i);
  lval_free(input);
  return x;
}

lval * evaluate_sexpr(lval *input) {
  // Recursively evaluate each cell
  for (int i=0; i<input->count; i++) {
    input->cell[i] = evaluate_lval(input->cell[i]);
  }
  // If there are errors, surface them
  for (int i=0; i<input->count; i++) {
    if (input->cell[i]->type == LVAL_ERR) {
      return lval_take(input, i);
    }
  }
  // Return singletons inside or outside expressions
  if (input->count == 0) {
    return input;
  } else if (input->count == 1) {
    return lval_take(input, 0);
  }
  lval *front = lval_pop(input, 0);
  if (front->type != LVAL_SYM) {
    lval_free(front);
    lval_free(input);
    return lval_err("A symbol must start an s-expression");
  }
  // Call arithmetic expression
  lval *result = evaluate_builtin(input, front->sym);
  lval_free(front);
  return result;
}


lval * lval_expand(lval *o, lval *new) {
  o->count++;
  o->cell = realloc(o->cell, sizeof(lval*)*o->count);
  o->cell[o->count-1] = new;
  return o;
}

lval * ast_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  if (errno == ERANGE) {
    return lval_err("Bad number");
  } else {
    return lval_num(x);
  }
}

lval * ast_parse_node(mpc_ast_t *t) {
  if (strstr(t->tag, "number")) { return ast_read_num(t); }
  if (strstr(t->tag, "sym")) { return lval_sym(t->contents); } 
  lval * x = NULL;
  if (strcmp(t->tag, ">") == 0) {
    x = lval_sexpr();
  }
  if (strstr(t->tag, "sexpr")) {
    x = lval_sexpr();
  }

  for (int i=0; i<t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
    x = lval_expand(x, ast_parse_node(t->children[i]));
  }
  if (x == NULL) {
    printf("It returned null :(\n");
  }
  return x;
}

int main(int argc, char **argv) {
  // Parser types
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Sym = mpc_new("sym");
  mpc_parser_t* Sexpr = mpc_new("sexpr");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Lip = mpc_new("lip");
  // Language definition
  mpca_lang(MPCA_LANG_DEFAULT,
    "\
     number   :/-?[0-9]+/;\
     sym      :'+'|'-'|'*'|'/'|'^'|'\%';\
     sexpr    :'(' <expr>* ')';\
     expr     :<number>|<sym>|<sexpr>;\
     lip      :/^/ <expr>+ /$/;\
    ",
  Number, Sym, Sexpr, Expr, Lip);
  char *input;
  char prompt[5];
  sprintf(prompt, "%s> ", name);
  printf("%s version %s (SIGINT to exit)\n", name, version);
  // User input loop
  while (1) {
    input = readline(prompt);
    add_history(input);
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lip, &r)) {
      //mpc_ast_print(r.output);
      lval *result = evaluate_lval(ast_parse_node(r.output));
      if (result != NULL) {
        lval_print(result);
        lval_free(result);
      }
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    free(input);
  }
  mpc_cleanup(5, Number, Sym, Sexpr, Expr, Lip);
  return 0;
}
