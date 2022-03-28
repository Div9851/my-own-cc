#include "mcc.h"

// All local variable instances created during parsing are
// accumulated to this list
Obj *locals;

Type *declspec(Token **rest, Token *tok);
Type *declarator(Token **rest, Token *tok, Type *ty);
Node *declaration(Token **rest, Token *tok);
Node *compound_stmt(Token **rest, Token *tok);
Node *stmt(Token **rest, Token *tok);
Node *expr_stmt(Token **rest, Token *tok);
Node *expr(Token **rest, Token *tok);
Node *assign(Token **rest, Token *tok);
Node *equality(Token **rest, Token *tok);
Node *relational(Token **rest, Token *tok);
Node *add(Token **rest, Token *tok);
Node *mul(Token **rest, Token *tok);
Node *unary(Token **rest, Token *tok);
Node *primary(Token **rest, Token *tok);

// Find a local variable by name
Obj *find_var(Token *tok) {
    for (Obj *var = locals; var; var = var->next)
        if (strncmp(var->name, tok->loc, tok->len) == 0 &&
            var->name[tok->len] == '\0')
            return var;
    return NULL;
}

Node *new_node(NodeKind kind, Token *tok) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->tok = tok;
    return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok) {
    Node *node = new_node(kind, tok);
    node->lhs = lhs;
    node->rhs = rhs;
    add_type(node);
    return node;
}

Node *new_unary(NodeKind kind, Node *expr, Token *tok) {
    Node *node = new_node(kind, tok);
    node->lhs = expr;
    add_type(node);
    return node;
}

Node *new_num(int val, Token *tok) {
    Node *node = new_node(ND_NUM, tok);
    node->val = val;
    add_type(node);
    return node;
}

Node *new_var_node(Obj *var, Token *tok) {
    Node *node = new_node(ND_VAR, tok);
    node->var = var;
    add_type(node);
    return node;
}

Obj *new_lvar(char *name, Type *ty) {
    Obj *var = calloc(1, sizeof(Obj));
    var->name = name;
    var->ty = ty;
    var->next = locals;
    locals = var;
    return var;
}

char *get_ident(Token *tok) {
    if (tok->kind != TK_IDENT)
        error_tok(tok, "expected an identifier");
    return strndup(tok->loc, tok->len);
}

Type *declspec(Token **rest, Token *tok) {
    *rest = skip(tok, "int");
    return ty_int;
}

Type *type_suffix(Token **rest, Token *tok, Type *ty) {
    if (equal(tok, "(")) {
        *rest = skip(tok->next, ")");
        return func_type(ty);
    }
    *rest = tok;
    return ty;
}

Type *declarator(Token **rest, Token *tok, Type *ty) {
    while (consume(&tok, tok, "*"))
        ty = pointer_to(ty);

    if (tok->kind != TK_IDENT)
        error_tok(tok, "expected a variable name");

    ty - type_suffix(rest, tok->next, ty);
    ty->name = tok;
    return ty;
}

Node *declaration(Token **rest, Token *tok) {
    Type *basety = declspec(&tok, tok);

    Node head = {};
    Node *cur = &head;
    int i = 0;

    while (!equal(tok, ";")) {
        if (i++ > 0)
            tok = skip(tok, ",");

        Type *ty = declarator(&tok, tok, basety);
        Obj *var = new_lvar(get_ident(ty->name), ty);

        if (!equal(tok, "="))
            continue;

        Node *lhs = new_var_node(var, ty->name);
        Node *rhs = assign(&tok, tok->next);
        Node *node = new_node(ND_EXPR_STMT, tok);
        node->lhs = new_binary(ND_ASSIGN, lhs, rhs, tok);
        cur = cur->next = node;
    }
    Node *node = new_node(ND_BLOCK, tok);
    node->body = head.next;
    *rest = tok->next;
    return node;
}

Node *stmt(Token **rest, Token *tok) {
    if (equal(tok, "return")) {
        Node *node = new_node(ND_RETURN, tok);
        node->lhs = expr(&tok, tok->next);
        *rest = skip(tok, ";");
        return node;
    }

    if (equal(tok, "if")) {
        Node *node = new_node(ND_IF, tok);
        tok = skip(tok->next, "(");
        node->cond = expr(&tok, tok);
        tok = skip(tok, ")");
        node->then = stmt(&tok, tok);
        if (equal(tok, "else"))
            node->els = stmt(&tok, tok->next);
        *rest = tok;
        return node;
    }

    if (equal(tok, "for")) {
        Node *node = new_node(ND_FOR, tok);
        tok = skip(tok->next, "(");
        node->init = expr_stmt(&tok, tok);
        if (!equal(tok, ";"))
            node->cond = expr(&tok, tok);
        tok = skip(tok, ";");
        if (!equal(tok, ")"))
            node->inc = expr(&tok, tok);
        tok = skip(tok, ")");
        node->then = stmt(rest, tok);
        return node;
    }

    if (equal(tok, "while")) {
        Node *node = new_node(ND_FOR, tok);
        tok = skip(tok->next, "(");
        node->cond = expr(&tok, tok);
        tok = skip(tok, ")");
        node->then = stmt(rest, tok);
        return node;
    }

    if (equal(tok, "{"))
        return compound_stmt(rest, tok->next);

    return expr_stmt(rest, tok);
}

Node *compound_stmt(Token **rest, Token *tok) {
    Node head = {};
    Node *cur = &head;
    while (!equal(tok, "}")) {
        if (equal(tok, "int"))
            cur = cur->next = declaration(&tok, tok);
        else
            cur = cur->next = stmt(&tok, tok);
    }
    Node *node = new_node(ND_BLOCK, tok);
    node->body = head.next;
    *rest = tok->next;
    return node;
}

Node *expr_stmt(Token **rest, Token *tok) {
    if (equal(tok, ";")) {
        *rest = tok->next;
        return new_node(ND_BLOCK, tok);
    }

    Node *node = new_node(ND_EXPR_STMT, tok);
    node->lhs = expr(&tok, tok);
    *rest = skip(tok, ";");
    return node;
}

Node *expr(Token **rest, Token *tok) { return assign(rest, tok); }

Node *assign(Token **rest, Token *tok) {
    Node *node = equality(&tok, tok);
    if (equal(tok, "="))
        node = new_binary(ND_ASSIGN, node, assign(&tok, tok->next), tok);
    *rest = tok;
    return node;
}

Node *equality(Token **rest, Token *tok) {
    Node *node = relational(&tok, tok);

    for (;;) {
        if (equal(tok, "==")) {
            node = new_binary(ND_EQ, node, relational(&tok, tok->next), tok);
            continue;
        }

        if (equal(tok, "!=")) {
            node = new_binary(ND_NE, node, relational(&tok, tok->next), tok);
            continue;
        }

        *rest = tok;
        return node;
    }
}

Node *relational(Token **rest, Token *tok) {
    Node *node = add(&tok, tok);

    for (;;) {
        if (equal(tok, "<")) {
            node = new_binary(ND_LT, node, add(&tok, tok->next), tok);
            continue;
        }

        if (equal(tok, "<=")) {
            node = new_binary(ND_LE, node, add(&tok, tok->next), tok);
            continue;
        }

        if (equal(tok, ">")) {
            node = new_binary(ND_LT, add(&tok, tok->next), node, tok);
            continue;
        }

        if (equal(tok, ">=")) {
            node = new_binary(ND_LE, add(&tok, tok->next), node, tok);
            continue;
        }

        *rest = tok;
        return node;
    }
}

Node *new_add(Node *lhs, Node *rhs, Token *tok) {
    // num + num
    if (is_integer(lhs->ty) && is_integer(rhs->ty))
        return new_binary(ND_ADD, lhs, rhs, tok);

    // `num + ptr` to `ptr + num`
    if (is_integer(lhs->ty) && is_pointer(rhs->ty)) {
        Node *tmp = lhs;
        lhs = rhs;
        rhs = tmp;
    }

    // ptr + num
    if (is_pointer(lhs->ty) && is_integer(rhs->ty)) {
        rhs = new_binary(ND_MUL, rhs, new_num(8, tok), tok);
        return new_binary(ND_ADD, lhs, rhs, tok);
    }

    error_tok(tok, "invalid operands");
}

Node *new_sub(Node *lhs, Node *rhs, Token *tok) {
    // num - num
    if (is_integer(lhs->ty) && is_integer(rhs->ty))
        return new_binary(ND_SUB, lhs, rhs, tok);

    // ptr - num
    if (is_pointer(lhs->ty) && is_integer(rhs->ty)) {
        rhs = new_binary(ND_MUL, rhs, new_num(8, tok), tok);
        Node *node = new_binary(ND_SUB, lhs, rhs, tok);
        return node;
    }

    // ptr - ptr
    if (is_pointer(lhs->ty) && is_pointer(rhs->ty)) {
        Node *node = new_binary(ND_SUB, lhs, rhs, tok);
        node->ty = ty_int;
        return new_binary(ND_DIV, node, new_num(8, tok), tok);
    }

    error_tok(tok, "invalid operands");
}

Node *add(Token **rest, Token *tok) {
    Node *node = mul(&tok, tok);

    for (;;) {
        if (equal(tok, "+")) {
            node = new_add(node, mul(&tok, tok->next), tok);
            continue;
        }

        if (equal(tok, "-")) {
            node = new_sub(node, mul(&tok, tok->next), tok);
            continue;
        }

        *rest = tok;
        return node;
    }
}

Node *mul(Token **rest, Token *tok) {
    Node *node = unary(&tok, tok);

    for (;;) {
        if (equal(tok, "*")) {
            node = new_binary(ND_MUL, node, unary(&tok, tok->next), tok);
        }

        if (equal(tok, "/")) {
            node = new_binary(ND_DIV, node, unary(&tok, tok->next), tok);
        }

        *rest = tok;
        return node;
    }
}

Node *unary(Token **rest, Token *tok) {
    if (equal(tok, "+"))
        return unary(rest, tok->next);

    if (equal(tok, "-"))
        return new_unary(ND_NEG, unary(rest, tok->next), tok);

    if (equal(tok, "&"))
        return new_unary(ND_ADDR, unary(rest, tok->next), tok);

    if (equal(tok, "*"))
        return new_unary(ND_DEREF, unary(rest, tok->next), tok);

    return primary(rest, tok);
}

Node *funcall(Token **rest, Token *tok) {
    Token *start = tok;
    tok = tok->next->next;

    Node head = {};
    Node *cur = &head;

    while (!equal(tok, ")")) {
        if (cur != &head)
            tok = skip(tok, ",");
        cur = cur->next = assign(&tok, tok);
    }

    *rest = skip(tok, ")");

    Node *node = new_node(ND_FUNCALL, start);
    node->funcname = strndup(start->loc, start->len);
    node->args = head.next;
    return node;
}

Node *primary(Token **rest, Token *tok) {
    if (equal(tok, "(")) {
        Node *node = expr(&tok, tok->next);
        *rest = skip(tok, ")");
        return node;
    }

    if (tok->kind == TK_IDENT) {
        // Function call
        if (equal(tok->next, "("))
            return funcall(rest, tok);

        // Variable
        Obj *var = find_var(tok);
        if (!var)
            error_tok(tok, "undefined variable");
        *rest = tok->next;
        return new_var_node(var, tok);
    }

    if (tok->kind == TK_NUM) {
        Node *node = new_num(tok->val, tok);
        *rest = tok->next;
        return node;
    }

    error_tok(tok, "expected an expression");
}

Function *function(Token **rest, Token *tok) {
    Type *ty = declspec(&tok, tok);
    ty = declarator(&tok, tok, ty);

    locals = NULL;

    Function *fn = calloc(1, sizeof(Function));
    fn->name = get_ident(ty->name);

    tok = skip(tok, "{");
    fn->body = compound_stmt(rest, tok);
    fn->locals = locals;
    return fn;
}

Function *parse(Token *tok) {
    Function head = {};
    Function *cur = &head;
    while (tok->kind != TK_EOF)
        cur = cur->next = function(&tok, tok);
    return head.next;
}