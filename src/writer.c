#include "cirf/writer.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

struct writer {
        FILE       *fp;
        int         indent_level;
        int         at_line_start;
        const char *indent_string;
};

writer_t *writer_create(FILE *fp) {
    writer_t *w = calloc(1, sizeof(writer_t));
    if(!w) return NULL;

    w->fp = fp;
    w->indent_level = 0;
    w->at_line_start = 1;
    w->indent_string = "    ";

    return w;
}

void writer_destroy(writer_t *w) {
    free(w);
}

static void write_indent(writer_t *w) {
    if(w->at_line_start) {
        for(int i = 0; i < w->indent_level; i++) {
            fputs(w->indent_string, w->fp);
        }
        w->at_line_start = 0;
    }
}

void writer_printf(writer_t *w, const char *fmt, ...) {
    write_indent(w);

    va_list args;
    va_start(args, fmt);
    vfprintf(w->fp, fmt, args);
    va_end(args);

    /* Check if we ended with newline */
    size_t len = strlen(fmt);
    if(len > 0 && fmt[len - 1] == '\n') {
        w->at_line_start = 1;
    }
}

void writer_puts(writer_t *w, const char *s) {
    write_indent(w);
    fputs(s, w->fp);

    size_t len = strlen(s);
    if(len > 0 && s[len - 1] == '\n') {
        w->at_line_start = 1;
    }
}

void writer_putc(writer_t *w, char c) {
    write_indent(w);
    fputc(c, w->fp);

    if(c == '\n') {
        w->at_line_start = 1;
    }
}

void writer_newline(writer_t *w) {
    fputc('\n', w->fp);
    w->at_line_start = 1;
}

void writer_indent(writer_t *w) {
    w->indent_level++;
}

void writer_dedent(writer_t *w) {
    if(w->indent_level > 0) {
        w->indent_level--;
    }
}

void writer_write_bytes_hex(writer_t *w, const unsigned char *data, size_t len,
                            int bytes_per_line) {
    for(size_t i = 0; i < len; i++) {
        if(i > 0) {
            fputc(',', w->fp);
            if((i % bytes_per_line) == 0) {
                fputc('\n', w->fp);
                w->at_line_start = 1;
                write_indent(w);
            } else {
                fputc(' ', w->fp);
            }
        } else {
            write_indent(w);
        }
        fprintf(w->fp, "0x%02x", data[i]);
    }
}

void writer_write_string_escaped(writer_t *w, const char *s) {
    write_indent(w);
    fputc('"', w->fp);

    while(*s) {
        switch(*s) {
            case '\n':
                fputs("\\n", w->fp);
                break;
            case '\r':
                fputs("\\r", w->fp);
                break;
            case '\t':
                fputs("\\t", w->fp);
                break;
            case '\\':
                fputs("\\\\", w->fp);
                break;
            case '"':
                fputs("\\\"", w->fp);
                break;
            default:
                if((unsigned char)*s < 0x20) {
                    fprintf(w->fp, "\\x%02x", (unsigned char)*s);
                } else {
                    fputc(*s, w->fp);
                }
                break;
        }
        s++;
    }

    fputc('"', w->fp);
}
