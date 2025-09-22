// sj.h - v0.4 - rxi 2025
// public domain - no warranty implied, use at your own risk

#ifndef SJ_H
#define SJ_H

#include <stddef.h>

typedef struct {
    char *data, *cur, *end;
    int depth;
    char *error;
} sj_Reader;

typedef struct {
    int type;
    char *start, *end;
    int depth;
} sj_Value;

enum { SJ_ERROR, SJ_END, SJ_ARRAY, SJ_OBJECT, SJ_NUMBER, SJ_STRING, SJ_BOOL, SJ_NULL };

sj_Reader sj_reader(char *data, size_t len);
sj_Value sj_read(sj_Reader *r);
int sj_iter_array(sj_Reader *r, sj_Value arr, sj_Value *val);
int sj_iter_object(sj_Reader *r, sj_Value obj, sj_Value *key, sj_Value *val);
void sj_location(sj_Reader *r, int *line, int *col);

#endif // #ifndef SJ_H

#ifdef SJ_IMPL


sj_Reader sj_reader(char *data, size_t len) {
    return (sj_Reader){ .data = data, .cur = data, .end = data + len };
}


static int sj__is_number_cont(char c) {
    return (c >= '0' && c <= '9')
        ||  c == 'e' || c == 'E' || c == '.' || c == '-' || c == '+';
}

static int sj__is_string(char *cur, char *end, char *expect) {
    while (*expect) {
        if (cur == end || *cur != *expect) {
            return 0;
        }
        expect++, cur++;
    }
    return 1;
}


sj_Value sj_read(sj_Reader *r) {
    sj_Value res;
top:
    if (r->error) { return (sj_Value){ .type = SJ_ERROR, .start = r->cur, .end = r->cur }; }
    if (r->cur == r->end) { r->error = "unexpected eof"; goto top; }
    res.start = r->cur;

    switch (*r->cur) {
    case ' ': case '\n': case '\r': case '\t':
    case ':': case ',':
        r->cur++;
        goto top;

    case '-': case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        res.type = SJ_NUMBER;
        while (r->cur != r->end && sj__is_number_cont(*r->cur)) { r->cur++; }
        break;

    case '"':
        res.type = SJ_STRING;
        res.start = ++r->cur;
        for (;;) {
            if ( r->cur == r->end) { r->error = "unclosed string"; goto top; }
            if (*r->cur ==    '"') { break; }
            if (*r->cur ==   '\\') { r->cur++; }
            if ( r->cur != r->end) { r->cur++; }
        }
        res.end = r->cur++;
        return res;

    case '{': case '[':
        res.type = (*r->cur == '{') ? SJ_OBJECT : SJ_ARRAY;
        res.depth = ++r->depth;
        r->cur++;
        break;

    case '}': case ']':
        res.type = SJ_END;
        if (--r->depth < 0) {
            r->error = (*r->cur == '}') ? "stray '}'" : "stray ']'";
            goto top;
        }
        r->cur++;
        break;

    case 'n': case 't': case 'f':
        res.type = (*r->cur == 'n') ? SJ_NULL : SJ_BOOL;
        if (sj__is_string(r->cur, r->end,  "null")) { r->cur += 4; break; }
        if (sj__is_string(r->cur, r->end,  "true")) { r->cur += 4; break; }
        if (sj__is_string(r->cur, r->end, "false")) { r->cur += 5; break; }
        // fallthrough

    default:
        r->error = "unknown token";
        goto top;
    }
    res.end = r->cur;
    return res;
}


static void sj__discard_until(sj_Reader *r, int depth) {
    sj_Value val;
    val.type = SJ_NULL;
    while (r->depth != depth && val.type != SJ_ERROR) {
        val = sj_read(r);
    }
}


int sj_iter_array(sj_Reader *r, sj_Value arr, sj_Value *val) {
    sj__discard_until(r, arr.depth);
    *val = sj_read(r);
    if (val->type == SJ_ERROR || val->type == SJ_END) { return 0; }
    return 1;
}


int sj_iter_object(sj_Reader *r, sj_Value obj, sj_Value *key, sj_Value *val) {
    sj__discard_until(r, obj.depth);
    *key = sj_read(r);
    if (key->type == SJ_ERROR || key->type == SJ_END) { return 0; }
    *val = sj_read(r);
    if (val->type == SJ_END)   { r->error = "unexpected object end"; return 0; }
    if (val->type == SJ_ERROR) { return 0; }
    return 1;
}


void sj_location(sj_Reader *r, int *line, int *col) {
    int ln = 1, cl = 1;
    for (char *p = r->data; p != r->cur; p++) {
        if (*p == '\n') { ln++; cl = 0; }
        cl++;
    }
    *line = ln;
    *col = cl;
}

#endif // #ifdef SJ_IMPL