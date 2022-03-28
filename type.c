#include "mcc.h"

Type *ty_int = &(Type){TY_INT};

bool is_integer(Type *ty) { return ty->kind == TY_INT; }

bool is_pointer(Type *ty) { return ty->kind == TY_PTR; }

Type *copy_type(Type *ty) {
    Type *ret = calloc(1, sizeof(Type));
    *ret = *ty;
    return ret;
}

Type *pointer_to(Type *base) {
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_PTR;
    ty->base = base;
    return ty;
}

Type *func_type(Type *return_ty) {
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_FUNC;
    ty->return_ty = return_ty;
    return ty;
}

void add_type(Node *node) {
    if (!node)
        return;

    switch (node->kind) {
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
    case ND_NEG:
    case ND_ASSIGN:
        node->ty = node->lhs->ty;
        return;
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_NUM:
    case ND_FUNCALL:
        node->ty = ty_int;
        return;
    case ND_VAR:
        node->ty = node->var->ty;
        return;
    case ND_ADDR:
        node->ty = pointer_to(node->lhs->ty);
        return;
    case ND_DEREF:
        if (node->lhs->ty->kind != TY_PTR)
            error_tok(node->tok, "invalid pointer dereference");
        node->ty = node->lhs->ty->base;
        return;
    }
}