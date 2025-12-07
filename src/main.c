#include "cirf/version.h"
#include "cirf/error.h"
#include "cirf/config.h"
#include "cirf/codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    const char *config_path;
    const char *output_path;
    const char *header_path;
} cli_options_t;

static void print_usage(const char *prog)
{
    fprintf(stderr, "Usage: %s -n <name> -c <config> -o <output.c> -H <output.h>\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -n, --name <name>      Base name for generated symbols (required)\n");
    fprintf(stderr, "  -c, --config <file>    Input configuration file (JSON)\n");
    fprintf(stderr, "  -o, --output <file>    Output C source file\n");
    fprintf(stderr, "  -H, --header <file>    Output C header file\n");
    fprintf(stderr, "  -h, --help             Show this help message\n");
    fprintf(stderr, "  -v, --version          Show version information\n");
}

static void print_version(void)
{
    printf("cirf version %s\n", CIRF_VERSION_STRING);
}

static int streq(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

static int parse_args(int argc, char **argv, cli_options_t *opts)
{
    memset(opts, 0, sizeof(*opts));

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (streq(arg, "-h") || streq(arg, "--help")) {
            print_usage(argv[0]);
            exit(0);
        }

        if (streq(arg, "-v") || streq(arg, "--version")) {
            print_version();
            exit(0);
        }

        if (streq(arg, "-n") || streq(arg, "--name")) {
            if (++i >= argc) {
                fprintf(stderr, "Error: %s requires an argument\n", arg);
                return -1;
            }
            opts->name = argv[i];
            continue;
        }

        if (streq(arg, "-c") || streq(arg, "--config")) {
            if (++i >= argc) {
                fprintf(stderr, "Error: %s requires an argument\n", arg);
                return -1;
            }
            opts->config_path = argv[i];
            continue;
        }

        if (streq(arg, "-o") || streq(arg, "--output")) {
            if (++i >= argc) {
                fprintf(stderr, "Error: %s requires an argument\n", arg);
                return -1;
            }
            opts->output_path = argv[i];
            continue;
        }

        if (streq(arg, "-H") || streq(arg, "--header")) {
            if (++i >= argc) {
                fprintf(stderr, "Error: %s requires an argument\n", arg);
                return -1;
            }
            opts->header_path = argv[i];
            continue;
        }

        fprintf(stderr, "Error: Unknown option: %s\n", arg);
        return -1;
    }

    return 0;
}

static int validate_options(const cli_options_t *opts, const char *prog)
{
    int valid = 1;

    if (!opts->name) {
        fprintf(stderr, "Error: -n/--name is required\n");
        valid = 0;
    }

    if (!opts->config_path) {
        fprintf(stderr, "Error: -c/--config is required\n");
        valid = 0;
    }

    if (!opts->output_path) {
        fprintf(stderr, "Error: -o/--output is required\n");
        valid = 0;
    }

    if (!opts->header_path) {
        fprintf(stderr, "Error: -H/--header is required\n");
        valid = 0;
    }

    if (!valid) {
        fprintf(stderr, "\n");
        print_usage(prog);
    }

    return valid;
}

int main(int argc, char **argv)
{
    cli_options_t opts;

    if (parse_args(argc, argv, &opts) != 0) {
        return 1;
    }

    if (!validate_options(&opts, argv[0])) {
        return 1;
    }

    /* Load configuration */
    cirf_config_t *config = NULL;
    cirf_error_t err = config_load(opts.config_path, opts.name, &config);
    if (err != CIRF_OK) {
        fprintf(stderr, "Error loading config '%s': %s\n",
                opts.config_path, cirf_error_string(err));
        return 1;
    }

    /* Generate code */
    codegen_options_t gen_opts = {
        .name = opts.name,
        .source_path = opts.output_path,
        .header_path = opts.header_path
    };

    err = codegen_generate(config, &gen_opts);
    if (err != CIRF_OK) {
        fprintf(stderr, "Error generating code: %s\n", cirf_error_string(err));
        config_destroy(config);
        return 1;
    }

    config_destroy(config);

    printf("Generated %s and %s\n", opts.output_path, opts.header_path);
    return 0;
}
