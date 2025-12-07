#ifndef CIRF_WRITER_H
#define CIRF_WRITER_H

#include <stddef.h>
#include <stdio.h>

typedef struct writer writer_t;

writer_t *writer_create(FILE *fp);
void      writer_destroy(writer_t *w);

void writer_printf(writer_t *w, const char *fmt, ...);
void writer_puts(writer_t *w, const char *s);
void writer_putc(writer_t *w, char c);
void writer_newline(writer_t *w);

void writer_indent(writer_t *w);
void writer_dedent(writer_t *w);

void writer_write_bytes_hex(writer_t *w, const unsigned char *data, size_t len, int bytes_per_line);
void writer_write_string_escaped(writer_t *w, const char *s);

#endif /* CIRF_WRITER_H */
