#include <tioc/tioc.h>
#include <errno.h>
#include <locale.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>

int help(void);

/*
 * Writes the remaining bytes on stdin to stdout.
 */
void chain(void);

/*
 * Obtains a value from the next argument.
 *
 * On success, this function increments *argi, sets *value to the next argument,
 * sets *type to dtype, and returns 0.
 *
 * Returns -1 on failure.
 */
int get_argument(int argc, const char *argv[], int *argi, const char *arg, const char **value, int *type, int dtype);

/*
 * Called by main().
 */
int write(int argc, const char *argv[]);

/*
 * Called by main().
 */
int read(int argc, const char *argv[]);

/*
 * Called by main().
 */
int expect(int argc, const char *argv[]);

int main(int argc, const char *argv[])
{
    int argi = 1;
    const char *arg = NULL;

    for (; argi < argc; ++argi)
    {
        arg = argv[argi];
        if (!strcmp(arg, "help"))
        {
            return help();
        }
        else if (!strcmp(arg, "write"))
        {
            return write(argc - argi - 1, argv + argi + 1);
        }
        else if (!strcmp(arg, "read"))
        {
            return read(argc - argi - 1, argv + argi + 1);
        }
        else if (!strcmp(arg, "expect"))
        {
            return expect(argc - argi - 1, argv + argi + 1);
        }
        else
        {
            warnx("Invalid command. Use 'help' for usage.");
            return EXIT_FAILURE;
        }
    }

    warnx("Missing command. Use 'help' for usage.");
    return EXIT_FAILURE;
}

int help()
{
    return WEXITSTATUS(system("man 1 tioc"));
}

int write(int argc, const char *argv[])
{
    int argi = 0;
    const char *arg = NULL;
    const char *label = NULL;
    unsigned long long int n = 0;
    uuid_t u;
	char *end = NULL;
    int rc = EXIT_FAILURE;
    int chn = 0;
    char *blobdata = NULL;
    size_t blobsize = 0;

    /*
     * 1 = unsigned
     * 2 = UUID
     * 3 = string
     * 4 = blob
     */
    int type = 0;
    const char *value = NULL;

    for (; argi < argc; ++argi)
    {
        arg = argv[argi];

        if (!strcmp(arg, "-l") || !strcmp(arg, "--label"))
        {
            if (argi >= argc - 1)
            {
                warnx("The '%s' argument requires a value.", arg);
                goto cleanup;
            }

            if (label)
            {
                warnx("A label has already been supplied.");
                goto cleanup;
            }

            ++argi;
            label = argv[argi];
        }
        else if (!strcmp(arg, "-c") || !strcmp(arg, "--chain"))
        {
            chn = 1;
        }
        else if (!strcmp(arg, "-n") || !strcmp(arg, "--unsigned"))
        {
            if (-1 == get_argument(argc, argv, &argi, arg, &value, &type, 1))
                goto cleanup;
        }
        else if (!strcmp(arg, "-u") || !strcmp(arg, "--uuid"))
        {
            if (-1 == get_argument(argc, argv, &argi, arg, &value, &type, 2))
                goto cleanup;
        }
        else if (!strcmp(arg, "-s") || !strcmp(arg, "--string"))
        {
            if (-1 == get_argument(argc, argv, &argi, arg, &value, &type, 3))
                goto cleanup;
        }
        else if (!strcmp(arg, "-b") || !strcmp(arg, "--blob"))
        {
            if (-1 == get_argument(argc, argv, &argi, arg, &value, &type, 4))
                goto cleanup;
        }
        else
        {
            warnx("Unknown argument.");
            goto cleanup;
        }
    }

    if (!label)
    {
        warnx("No label supplied.");
        goto cleanup;
    }

    if (chn) chain();

    if (1 == type)
    {
        errno = 0;
        n = strtoull(value, &end, 10);

        if (!*value)
        {
            warnx("Empty unsigned value.");
            goto cleanup;
        }

        if (value == end)
        {
            warnx("Invalid unsigned value.");
            goto cleanup;
        }

        if (n == ULLONG_MAX && ERANGE == errno)
        {
            warnx("Unsigned value out of range.");
            goto cleanup;
        }

        if (*end)
        {
            warnx("Unterminated unsigned value.");
            goto cleanup;
        }

        if (-1 == write_unsigned(stdout, label, n))
        {
            warnx("Unable to write unsigned value.");
            goto cleanup;
        }

        rc = EXIT_SUCCESS;
    }
    else if (2 == type)
    {
        if (-1 == uuid_parse(value, u))
        {
            warnx("Invalid UUID.");
            goto cleanup;
        }

        if (-1 == write_uuid(stdout, label, u))
        {
            warnx("Unable to write string value.");
            goto cleanup;
        }

        rc = EXIT_SUCCESS;
    }
    else if (3 == type)
    {
        if (-1 == write_string(stdout, label, value))
        {
            warnx("Unable to write string value.");
            goto cleanup;
        }

        rc = EXIT_SUCCESS;
    }
    else if (4 == type)
    {
        if (-1 == read_file_content(value, &blobdata, &blobsize))
        {
            warnx("Unable to read blob file '%s'.", value);
            goto cleanup;
        }

        if (-1 == write_blob(stdout, label, blobdata, blobsize))
        {
            warnx("Unable to write blob.");
            goto cleanup;
        }

        rc = EXIT_SUCCESS;
    }
    else
    {
        warnx("Invalid type. This is a programming error.");
        goto cleanup;
    }

cleanup:
    free(blobdata);
    return rc;
}

int get_argument(int argc, const char *argv[], int *argi, const char *arg, const char **value, int *type, int dtype)
{
    if (*argi >= argc - 1)
    {
        warnx("The '%s' argument requires a value.", arg);
        return -1;
    }

    if (*value)
    {
        warnx("A value has already been supplied.");
        return -1;
    }

    ++(*argi);

    *value = argv[*argi];
    *type = dtype;

    return 0;
}

int read(int argc, const char *argv[])
{
    int argi = 0;
    const char *arg = NULL;
    const char *label = NULL;
    unsigned long long n = 0;
    uuid_t u;
    char uuid_string[37];
    int quiet = 0;
    int chn = 0;
    char *string = NULL;
    char *blobdata = NULL;
    size_t blobsize = 0;

    /*
     * 1 = unsigned
     * 2 = UUID
     * 3 = string
     * 4 = blob
     */
    int type = 0;

    int rc = EXIT_FAILURE;

    for (; argi < argc; ++argi)
    {
        arg = argv[argi];

        if (!strcmp(arg, "-l") || !strcmp(arg, "--label"))
        {
            if (argi >= argc - 1)
            {
                warnx("The '%s' argument requires a value.", arg);
                goto cleanup;
            }

            if (label)
            {
                warnx("A label has already been supplied.");
                goto cleanup;
            }

            ++argi;
            label = argv[argi];
        }
        else if (!strcmp(arg, "-q") || !strcmp(arg, "--quiet"))
        {
            quiet = 1;
        }
        else if (!strcmp(arg, "-c") || !strcmp(arg, "--chain"))
        {
            chn = 1;
            quiet = 1;
        }
        else if (!strcmp(arg, "-n") || !strcmp(arg, "--unsigned"))
        {
            if (type)
            {
                warnx("A type was already specified.");
                goto cleanup;
            }

            type = 1;
        }
        else if (!strcmp(arg, "-u") || !strcmp(arg, "--uuid"))
        {
            if (type)
            {
                warnx("A type was already specified.");
                goto cleanup;
            }

            type = 2;
        }
        else if (!strcmp(arg, "-s") || !strcmp(arg, "--string"))
        {
            if (type)
            {
                warnx("A type was already specified.");
                goto cleanup;
            }

            type = 3;
        }
        else if (!strcmp(arg, "-b") || !strcmp(arg, "--blob"))
        {
            if (type)
            {
                warnx("A type was already specified.");
                goto cleanup;
            }

            type = 4;
        }
        else
        {
            warnx("Unknown argument.");
            goto cleanup;
        }
    }

    if (1 == type)
    {
        if (-1 == read_unsigned(stdin, label, &n))
        {
            warnx("Unable to read unsigned value.");
            goto cleanup;
        }

        if (!quiet) printf("%llu\n", n);

        rc = EXIT_SUCCESS;
    }
    else if (2 == type)
    {
        if (-1 == read_uuid(stdin, label, u))
        {
            warnx("Unable to read UUID.");
            goto cleanup;
        }

        if (!quiet)
        {
            uuid_unparse(u, uuid_string);
            printf("%s\n", uuid_string);
        }

        rc = EXIT_SUCCESS;
    }
    else if (3 == type)
    {
        if (-1 == read_string(stdin, label, &string))
        {
            warnx("Unable to read string.");
            goto cleanup;
        }

        if (!quiet) printf("%s\n", string);

        rc = EXIT_SUCCESS;
    }
    else if (4 == type)
    {
        if (-1 == read_blob(stdin, label, &blobdata, &blobsize))
        {
            warnx("Unable to read blob.");
            goto cleanup;
        }

        if (!quiet)
        {
            if (1 != fwrite(blobdata, blobsize, 1, stdout))
            {
                warnx("Unable to write blob.");
                goto cleanup;
            }
        }

        rc = EXIT_SUCCESS;
    }
    else
    {
        warnx("Invalid type. This is a programming error.");
        goto cleanup;
    }

    if (EXIT_SUCCESS == rc && chn) chain();

cleanup:
    free(string);
    free(blobdata);

    return rc;
}

int expect(int argc, const char *argv[])
{
    int argi = 0;
    const char *arg = NULL;
    const char *label = NULL;
    unsigned long long n = 0;
    uuid_t u;
    int quiet = 0;
    int chn = 0;
    const char *value = NULL;
    char *end = NULL;
    char *blobdata_expected = NULL;
    size_t blobsize_expected = 0;
    char *blobdata_actual = NULL;
    size_t blobsize_actual = 0;

    /*
     * 1 = unsigned
     * 2 = UUID
     * 3 = string
     * 4 = blob
     */
    int type = 0;

    int rc = EXIT_FAILURE;

    for (; argi < argc; ++argi)
    {
        arg = argv[argi];

        if (!strcmp(arg, "-l") || !strcmp(arg, "--label"))
        {
            if (argi >= argc - 1)
            {
                warnx("The '%s' argument requires a value.", arg);
                goto cleanup;
            }

            if (label)
            {
                warnx("A label has already been supplied.");
                goto cleanup;
            }

            ++argi;
            label = argv[argi];
        }
        else if (!strcmp(arg, "-q") || !strcmp(arg, "--quiet"))
        {
            quiet = 1;
        }
        else if (!strcmp(arg, "-c") || !strcmp(arg, "--chain"))
        {
            chn = 1;
            quiet = 1;
        }
        else if (!strcmp(arg, "-n") || !strcmp(arg, "--unsigned"))
        {
            if (-1 == get_argument(argc, argv, &argi, arg, &value, &type, 1))
                goto cleanup;
        }
        else if (!strcmp(arg, "-u") || !strcmp(arg, "--uuid"))
        {
            if (-1 == get_argument(argc, argv, &argi, arg, &value, &type, 2))
                goto cleanup;
        }
        else if (!strcmp(arg, "-s") || !strcmp(arg, "--string"))
        {
            if (-1 == get_argument(argc, argv, &argi, arg, &value, &type, 3))
                goto cleanup;
        }
        else if (!strcmp(arg, "-b") || !strcmp(arg, "--blob"))
        {
            if (-1 == get_argument(argc, argv, &argi, arg, &value, &type, 4))
                goto cleanup;
        }
        else
        {
            warnx("Unknown argument.");
            goto cleanup;
        }
    }

    if (1 == type)
    {
        errno = 0;
        n = strtoull(value, &end, 10);

        if (!*value)
        {
            warnx("Empty unsigned value.");
            goto cleanup;
        }

        if (value == end)
        {
            warnx("Invalid unsigned value.");
            goto cleanup;
        }

        if (n == ULLONG_MAX && ERANGE == errno)
        {
            warnx("Unsigned value out of range.");
            goto cleanup;
        }

        if (*end)
        {
            warnx("Unterminated unsigned value.");
            goto cleanup;
        }

        if (-1 == expect_unsigned(stdin, label, n))
        {
            warnx("Unexpected unsigned value.");
            goto cleanup;
        }

        if (!quiet) printf("%llu\n", n);

        rc = EXIT_SUCCESS;
    }
    else if (2 == type)
    {
        if (-1 == uuid_parse(value, u))
        {
            warnx("Invalid UUID.");
            goto cleanup;
        }

        if (-1 == expect_uuid(stdin, label, u))
        {
            warnx("Unexpected UUID.");
            goto cleanup;
        }

        if (!quiet) printf("%s\n", value);

        rc = EXIT_SUCCESS;
    }
    else if (3 == type)
    {
        if (-1 == expect_string(stdin, label, value))
        {
            warnx("Unexpected string.");
            goto cleanup;
        }

        if (!quiet) printf("%s\n", value);

        rc = EXIT_SUCCESS;
    }
    else if (4 == type)
    {
        if (-1 == read_blob(stdin, label, &blobdata_actual, &blobsize_actual))
        {
            warnx("Unable to read actual blob.");
            goto cleanup;
        }

        if (-1 == read_file_content(value, &blobdata_expected,
                    &blobsize_expected))
        {
            warnx("Unable to read expected blob.");
            goto cleanup;
        }

        if (blobsize_expected != blobsize_actual)
        {
            warnx
            (
                "Expected a blob size of %zu but found %zu.",
                blobsize_expected,
                blobsize_actual
            );
            goto cleanup;
        }

        if (memcmp(blobdata_actual, blobdata_expected, blobsize_actual))
        {
            warnx("Blob content mismatch.");
            goto cleanup;
        }

        if (!quiet)
        {
            if (1 != fwrite(blobdata_actual, blobsize_actual, 1, stdout))
            {
                warnx("Unable to write blob data.");
                goto cleanup;
            }
        }

        rc = EXIT_SUCCESS;
    }
    else
    {
        warnx("Invalid type. This is a programming error.");
        goto cleanup;
    }

    if (EXIT_SUCCESS == rc && chn) chain();

cleanup:
    free(blobdata_expected);
    free(blobdata_actual);
    return rc;
}

void chain(void)
{
    int c;

    while (EOF != (c = getchar()))
    {
        if (EOF == putchar(c))
        {
            warnx("Unable to write byte to standard output.");
            return;
        }
    }
}
