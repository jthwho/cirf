#include "cirf/mime.h"
#include <string.h>
#include <ctype.h>

#define MIME_TYPE_LIST(X) \
    X("txt",   "text/plain") \
    X("text",  "text/plain") \
    X("html",  "text/html") \
    X("htm",   "text/html") \
    X("css",   "text/css") \
    X("csv",   "text/csv") \
    X("js",    "application/javascript") \
    X("mjs",   "application/javascript") \
    X("json",  "application/json") \
    X("xml",   "application/xml") \
    X("xhtml", "application/xhtml+xml") \
    X("pdf",   "application/pdf") \
    X("zip",   "application/zip") \
    X("gz",    "application/gzip") \
    X("tar",   "application/x-tar") \
    X("rar",   "application/vnd.rar") \
    X("7z",    "application/x-7z-compressed") \
    X("png",   "image/png") \
    X("jpg",   "image/jpeg") \
    X("jpeg",  "image/jpeg") \
    X("gif",   "image/gif") \
    X("bmp",   "image/bmp") \
    X("webp",  "image/webp") \
    X("svg",   "image/svg+xml") \
    X("ico",   "image/x-icon") \
    X("tiff",  "image/tiff") \
    X("tif",   "image/tiff") \
    X("woff",  "font/woff") \
    X("woff2", "font/woff2") \
    X("ttf",   "font/ttf") \
    X("otf",   "font/otf") \
    X("eot",   "application/vnd.ms-fontobject") \
    X("wav",   "audio/wav") \
    X("mp3",   "audio/mpeg") \
    X("ogg",   "audio/ogg") \
    X("oga",   "audio/ogg") \
    X("flac",  "audio/flac") \
    X("aac",   "audio/aac") \
    X("m4a",   "audio/mp4") \
    X("mp4",   "video/mp4") \
    X("webm",  "video/webm") \
    X("avi",   "video/x-msvideo") \
    X("mkv",   "video/x-matroska") \
    X("mov",   "video/quicktime") \
    X("ogv",   "video/ogg") \
    X("glsl",  "text/plain") \
    X("vert",  "text/plain") \
    X("frag",  "text/plain") \
    X("hlsl",  "text/plain") \
    X("c",     "text/x-c") \
    X("h",     "text/x-c") \
    X("cpp",   "text/x-c++") \
    X("hpp",   "text/x-c++") \
    X("cc",    "text/x-c++") \
    X("hh",    "text/x-c++") \
    X("py",    "text/x-python") \
    X("rb",    "text/x-ruby") \
    X("rs",    "text/x-rust") \
    X("go",    "text/x-go") \
    X("java",  "text/x-java") \
    X("sh",    "application/x-sh") \
    X("bash",  "application/x-sh") \
    X("zsh",   "application/x-sh") \
    X("md",    "text/markdown") \
    X("markdown", "text/markdown") \
    X("yaml",  "text/yaml") \
    X("yml",   "text/yaml") \
    X("toml",  "application/toml") \
    X("ini",   "text/plain") \
    X("cfg",   "text/plain") \
    X("conf",  "text/plain") \
    X("sql",   "application/sql") \
    X("wasm",  "application/wasm")

typedef struct {
    const char *ext;
    const char *mime;
} mime_entry_t;

#define MIME_ENTRY(ext, mime) { ext, mime },

static const mime_entry_t mime_table[] = {
    MIME_TYPE_LIST(MIME_ENTRY)
};

#undef MIME_ENTRY

static const size_t mime_table_size = sizeof(mime_table) / sizeof(mime_table[0]);

static int strcasecmp_local(const char *s1, const char *s2)
{
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

const char *mime_from_extension(const char *extension)
{
    if (!extension) {
        return "application/octet-stream";
    }

    /* Skip leading dot if present */
    if (*extension == '.') {
        extension++;
    }

    for (size_t i = 0; i < mime_table_size; i++) {
        if (strcasecmp_local(mime_table[i].ext, extension) == 0) {
            return mime_table[i].mime;
        }
    }

    return "application/octet-stream";
}

const char *mime_from_path(const char *path)
{
    if (!path) {
        return "application/octet-stream";
    }

    const char *dot = strrchr(path, '.');
    if (!dot || dot == path) {
        return "application/octet-stream";
    }

    return mime_from_extension(dot + 1);
}
