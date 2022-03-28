#include "mcc.h"

int depth;

void gen_expr(Node *node);

int count() {
    static int i = 1;
    return i++;
}

void push() {
    printf("    push rax\n");
    depth++;
}

void pop(char *arg) {
    printf("    pop %s\n", arg);
    depth--;
}

int align_to(int n, int align) { return (n + align - 1) / align * align; }

void gen_addr(Node *node) {
    switch (node->kind) {
    case ND_VAR:
        printf("    lea rax, [rbp - %d]\n", node->var->offset);
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        return;
    }

    error_tok(node->tok, "not an lvalue");
}

void gen_expr(Node *node) {
    switch (node->kind) {
    case ND_NUM:
        printf("    mov rax, %d\n", node->val);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        printf("    neg rax\n");
        return;
    case ND_VAR:
        gen_addr(node);
        printf("    mov rax, [rax]\n");
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        printf("    mov rax, [rax]\n");
        return;
    case ND_ADDR:
        gen_addr(node->lhs);
        return;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        push();
        gen_expr(node->rhs);
        pop("rdi");
        printf("    mov [rdi], rax\n");
        return;
    case ND_FUNCALL:
        printf("    mov rax, 0\n");
        printf("    call %s\n", node->funcname);
        return;
    }

    gen_expr(node->rhs);
    push();
    gen_expr(node->lhs);
    pop("rdi");

    switch (node->kind) {
    case ND_ADD:
        printf("    add rax, rdi\n");
        return;
    case ND_SUB:
        printf("    sub rax, rdi\n");
        return;
    case ND_MUL:
        printf("    imul rax, rdi\n");
        return;
    case ND_DIV:
        printf("    cqo\n");
        printf("    idiv rdi\n");
        return;
    case ND_EQ:
        printf("    cmp rax, rdi\n");
        printf("    sete al\n");
        printf("    movzb rax, al\n");
        return;
    case ND_NE:
        printf("    cmp rax, rdi\n");
        printf("    setne al\n");
        printf("    movzb rax, al\n");
        return;
    case ND_LT:
        printf("    cmp rax, rdi\n");
        printf("    setl al\n");
        printf("    movzb rax, al\n");
        return;
    case ND_LE:
        printf("    cmp rax, rdi\n");
        printf("    setle al\n");
        printf("    movzb rax, al\n");
        return;
    }

    error_tok(node->tok, "invalid expression");
}

void gen_stmt(Node *node) {
    switch (node->kind) {
    case ND_IF: {
        int c = count();
        gen_expr(node->cond);
        printf("    cmp rax, 0\n");
        printf("    je .L.else.%d\n", c);
        gen_stmt(node->then);
        printf("    jmp .L.end.%d\n", c);
        printf(".L.else.%d:\n", c);
        if (node->els)
            gen_stmt(node->els);
        printf(".L.end.%d:\n", c);
        return;
    }
    case ND_FOR: {
        int c = count();
        if (node->init)
            gen_stmt(node->init);
        printf(".L.begin.%d:\n", c);
        if (node->cond) {
            gen_expr(node->cond);
            printf("    cmp rax, 0\n");
            printf("    je .L.end.%d\n", c);
        }
        gen_stmt(node->then);
        if (node->inc)
            gen_expr(node->inc);
        printf("    jmp .L.begin.%d\n", c);
        printf(".L.end.%d:\n", c);
        return;
    }
    case ND_BLOCK:
        for (Node *n = node->body; n; n = n->next)
            gen_stmt(n);
        return;
    case ND_RETURN:
        gen_expr(node->lhs);
        printf("    jmp .L.return\n");
        return;
    case ND_EXPR_STMT:
        gen_expr(node->lhs);
        return;
    }

    error_tok(node->tok, "invalid statement");
}

// Assign offsets to local variables
int assign_lvar_offsets() {
    int offset = 0;
    for (Obj *var = locals; var; var = var->next) {
        offset += 8;
        var->offset = offset;
    }
    return align_to(offset, 16);
}

void codegen(Node *node) {
    int stack_size = assign_lvar_offsets();

    printf(".intel_syntax noprefix\n");
    printf(".globl main\n");
    printf("main:\n");

    // Prologue
    printf("    push rbp\n");
    printf("    mov rbp, rsp\n");
    printf("    sub rsp, %d\n", stack_size);

    for (Node *n = node; n; n = n->next) {
        gen_stmt(n);
        assert(depth == 0);
    }

    printf(".L.return:\n");
    printf("    mov rsp, rbp\n");
    printf("    pop rbp\n");
    printf("    ret\n");
}