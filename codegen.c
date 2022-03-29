#include "mcc.h"

static int depth;
static char *argreg8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static char *argreg64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
static Obj *current_fn;

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
        if (node->var->is_local) {
            // Local variable
            printf("    lea rax, [rbp - %d]\n", node->var->offset);
        } else {
            // Global variable
            printf("    lea rax, %s[rip]\n", node->var->name);
        }
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        return;
    }

    error_tok(node->tok, "not an lvalue");
}

// Load a value from where rax is pointing to
void load(Type *ty) {
    if (ty->kind == TY_ARRAY) {
        // If it is an array, do not attempt to load a value to the
        // register because in general we can't load an entire array to a
        // register. As a result, the result of an evaluation of an array
        // becomes not the array itself but the address of the array.
        // This is where "array is automatically converted to a pointer to
        // the first element of the array in C" occurs.
        return;
    }

    if (ty->size == 1)
        printf("    movsx rax, BYTE PTR [rax]\n");
    else
        printf("    mov rax, [rax]\n");
}

// Store rax to an address that stack top is pointing to
void store(Type *ty) {
    pop("rdi");

    if (ty->size == 1)
        printf("    mov [rdi], al\n");
    else
        printf("    mov [rdi], rax\n");
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
        load(node->ty);
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        load(node->ty);
        return;
    case ND_ADDR:
        gen_addr(node->lhs);
        return;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        push();
        gen_expr(node->rhs);
        store(node->ty);
        return;
    case ND_FUNCALL: {
        int nargs = 0;
        for (Node *arg = node->args; arg; arg = arg->next) {
            gen_expr(arg);
            push();
            nargs++;
        }

        for (int i = nargs - 1; i >= 0; i--)
            pop(argreg64[i]);

        printf("    mov rax, 0\n");
        printf("    call %s\n", node->funcname);
        return;
    }
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
        printf("    jmp .L.return.%s\n", current_fn->name);
        return;
    case ND_EXPR_STMT:
        gen_expr(node->lhs);
        return;
    }

    error_tok(node->tok, "invalid statement");
}

// Assign offsets to local variables
void assign_lvar_offsets(Obj *prog) {
    for (Obj *fn = prog; fn; fn = fn->next) {
        int offset = 0;
        for (Obj *var = fn->locals; var; var = var->next) {
            offset += var->ty->size;
            var->offset = offset;
        }
        fn->stack_size = align_to(offset, 16);
    }
}

void emit_data(Obj *prog) {
    for (Obj *var = prog; var; var = var->next) {
        if (var->is_function)
            continue;

        printf("    .data\n");
        printf("    .globl %s\n", var->name);
        printf("%s:\n", var->name);

        if (var->init_data) {
            for (int i = 0; i < var->ty->size; i++)
                printf("    .byte %d\n", var->init_data[i]);
        } else {
            printf("    .zero %d\n", var->ty->size);
        }
    }
}

void emit_text(Obj *prog) {
    for (Obj *fn = prog; fn; fn = fn->next) {
        if (!fn->is_function)
            continue;
        printf("    .globl %s\n", fn->name);
        printf("    .text\n");
        printf("%s:\n", fn->name);
        current_fn = fn;

        // Prologue
        printf("    push rbp\n");
        printf("    mov rbp, rsp\n");
        printf("    sub rsp, %d\n", fn->stack_size);

        int i = 0;
        for (Obj *var = fn->params; var; var = var->next) {
            if (var->ty->size == 1)
                printf("    mov [rbp - %d], %s\n", var->offset, argreg8[i++]);
            else
                printf("    mov [rbp - %d], %s\n", var->offset, argreg64[i++]);
        }

        // Emit code
        gen_stmt(fn->body);
        assert(depth == 0);

        // Epilogue
        printf(".L.return.%s:\n", fn->name);
        printf("    mov rsp, rbp\n");
        printf("    pop rbp\n");
        printf("    ret\n");
    }
}

void codegen(Obj *prog) {
    assign_lvar_offsets(prog);
    printf(".intel_syntax noprefix\n");
    emit_data(prog);
    emit_text(prog);
}