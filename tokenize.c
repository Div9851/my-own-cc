#include "mcc.h"

char *current_input;

void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void verror_at(char *loc, char *fmt, va_list ap) {
    int pos = loc - current_input;
    fprintf(stderr, "%s\n", current_input);
    fprintf(stderr, "%*s", pos, " ");
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void error_at(char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(loc, fmt, ap);
}

void error_tok(Token *tok, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verror_at(tok->loc, fmt, ap);
}

// トークンが`op`と等しいか判定する
bool equal(Token *tok, char *op) {
    return tok->kind == TK_RESERVED && strncmp(tok->loc, op, tok->len) == 0;
}

// トークンが`op`と等しい場合、次のトークンを返す
// それ以外の場合にはエラーを報告する
Token *consume(Token *tok, char *op) {
    if (!equal(tok, op))
        error_at(tok->loc, "'%c'ではありません", op);
    return tok->next;
}

// 新しいトークンを作成する
Token *new_token(TokenKind kind, char *start, char *end) {
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->loc = start;
    tok->len = end - start;
    return tok;
}

bool startswith(char *p, char *q) { return strncmp(p, q, strlen(q)) == 0; }

// 入力文字列pをトークナイズしてそれを返す
Token *tokenize(char *p) {
    current_input = p;
    Token head;
    head.next = NULL;
    Token *cur = &head;

    while (*p) {
        // 空白文字をスキップ
        if (isspace(*p)) {
            p++;
            continue;
        }

        if (startswith(p, "==") || startswith(p, "!=") || startswith(p, "<=") ||
            startswith(p, ">=")) {
            cur = cur->next = new_token(TK_RESERVED, p, p + 2);
            p += 2;
            continue;
        }

        if (strchr("+-*/()<>", *p)) {
            cur = cur->next = new_token(TK_RESERVED, p, p + 1);
            p++;
            continue;
        }

        if (isdigit(*p)) {
            cur = cur->next = new_token(TK_NUM, p, p);
            char *q = p;
            cur->val = strtol(p, &p, 10);
            cur->len = p - q;
            continue;
        }

        error_at(p, "トークナイズできません");
    }

    cur = cur->next = new_token(TK_EOF, p, p);
    return head.next;
}