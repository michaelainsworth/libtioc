#include "tioc.h"
#include <stdlib.h>
#include <locale.h>
#include <uuid/uuid.h>
#include <string.h>
#include <stdarg.h>

/*******************************************************************************
 * TYPES
 ******************************************************************************/

typedef int (*write_callback_t)(FILE *file, const void *data);
typedef int (*read_callback_t)(FILE *file, void *data);

struct wblob
{
    const char *data;
    size_t size;
};

struct rblob
{
    char **data;
    size_t *size;
};

/*******************************************************************************
 * WARNING FUNCTTIOCN DECLARATIONS
 ******************************************************************************/

/*
 * Prints a warning message to standard error.
 */
static void w(const char *fmt, ...);

/*
 * Prints a warning message to standard error, using the arguments supplied.
 */
static void wv(const char *fmt, va_list args);

/*******************************************************************************
 * LABEL FUNCTION DECLARATIONS
 ******************************************************************************/

/*
 * Returns 1 if the label is valid, otherwise 0.
 */
static int is_label_valid(const char *label);

/*******************************************************************************
 * WRITE FUNCTION DECLARATIONS
 ******************************************************************************/

/*
 * The write_callback function performs the operations common to all write_xxx()
 * functions.
 *
 * E.g., it checks that file and label are valid, sets the locale to "C" and
 * restores the locale afterwards.
 *
 * The actually writing of the data is performed by the callback.
 */
static int write_callback
(
    FILE *file,
    const char *label,
    write_callback_t callback,
    void *data
);

/*
 * This is the callback used for writing unsigned values.
 */
static int unsigned_writer(FILE *file, const void *data);

/*
 * This is the callback used for writing UUIDs.
 */
static int uuid_writer(FILE *file, const void *data);

/*
 * This is the callback used for writing blobs.
 *
 * The data argument should be a struct blob.
 */
static int blob_writer(FILE *file, const void *data);

/*******************************************************************************
 * READ FUNCTION DECLARATIONS
 ******************************************************************************/

/*
 * This function does the common reading tasks, like resetting the locale to
 * the "C" locale, etc, before calling the callback.
 */
static int read_callback
(
    FILE *file,
    const char *label,
    read_callback_t callback,
    void *data
);

static int unsigned_reader
(
    FILE *file,
    void *data
);

static int uuid_reader
(
    FILE *file,
    void *data
);

static int string_reader
(
    FILE *file,
    void *data
);

static int blob_reader
(
    FILE *file,
    void *data
);

/*******************************************************************************
 * EXPECT FUNCTION DECLARATIONS
 ******************************************************************************/

/*
 * Reads a label from file.
 *
 * Returns -1 if something went wrong, or 0 if the label was read.
 */
static int expect_label
(
    FILE *file,
    const char *expected
);

/*******************************************************************************
 * WARNING FUNCTION DECLARATIONS
 ******************************************************************************/

static void w(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    wv(fmt, args);
    va_end(args);
}

static void wv(const char *fmt, va_list args)
{
    fprintf(stderr, "libtioc: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}

/*******************************************************************************
 * LABEL FUNCTION DEFINITIONS
 ******************************************************************************/

static int is_label_valid(const char *label)
{
    size_t i, len;
    char c;

    if (!label) return 0;

    len = strlen(label);
    if (!len || len > 80) return 0;

    for (i = 0; i < len; ++i)
    {
        c = label[i];
        if ('_' != c && (c < 'a' || 'z' < c)) return 0;
    }

    return 1;
}

/*******************************************************************************
 * WRITE FUNCTION DEFINITIONS
 ******************************************************************************/

static int write_callback
(
    FILE *file,
    const char *label,
    write_callback_t callback,
    void *data
)
{
    int rc = -1;
    int crc = -1;
    char *old_locale = NULL;
    char *old_locale2 = NULL;

    if (!file)
    {
        w("write_callback(): Invalid 'file' argument.");
        goto cleanup;
    }

    if (!label)
    {
        w("write_callback(): Invalid 'label' argument.");
        goto cleanup;
    }

    if (!callback)
    {
        w("write_callback(): Invalid 'callback' argument.");
        goto cleanup;
    }

    if (!data)
    {
        w("write_callback(): Invalid 'data' argument.");
        goto cleanup;
    }

    if (!is_label_valid(label)) 
    {
        w("write_callback(): Invalid label '%s'.", label);
        goto cleanup;
    }

    if (!(old_locale = setlocale(LC_ALL, NULL)))
    {
        w("write_callback(): setlocale(LC_ALL, NULL) failed.");
        goto cleanup;
    }

    if (!(old_locale2 = strdup(old_locale)))
    {
        w("write_callback(): strdup(old_locale) failed.");
        goto cleanup;
    }

    if (!setlocale(LC_ALL, "C"))
    {
        w("write_callback(): setlocale(LC_ALL, \"C\") failed.");
        goto cleanup;
    }

    if (0 > fprintf(file, "%s:", label)) 
    {
        w("write_callback(): Unable to write label.");
    }

    if (-1 == (crc = callback(file, data))) goto cleanup;

    if (0 > fprintf(file, "\n"))
    {
        w("write_callback(): Unable to write newline.");
        goto cleanup;
    }

    rc = 0;

cleanup:
    if (old_locale2)
    {
        if (!setlocale(LC_ALL, old_locale2))
        {
            w("write_callback(): setlocale(LC_ALL, old_locale2) failed.");
        }
        free(old_locale2);
    }

    if (rc || crc) return -1;

    return 0;
}

static int unsigned_writer(FILE *file, const void *data)
{
    const unsigned long long *value = data;

    if (0 > fprintf(file, "%llu", *value))
    {
        w("unsigned_writer(): fprintf() failed.");
        return -1;
    }

    return 0;
}

static int uuid_writer(FILE *file, const void *data)
{
    char uuid_string[37];
    const uuid_t * const * uuid = data;

    uuid_unparse(**uuid, uuid_string);
    if (0 > fprintf(file, "%s", uuid_string)) 
    {
        w("uuid_writer(): fprintf() failed.");
        return -1;
    }
    return 0;
}

static int blob_writer(FILE *file, const void *data)
{
    const struct wblob *wblob = data;

    if (0 > fprintf(file, "%llu:", (unsigned long long)wblob->size))
    {
        w("blob_writer(): fprintf() failed.");
        return -1;
    }

    if (1 != fwrite(wblob->data, wblob->size, 1, file))
    {
        w("blob_writer(): fwrite() failed.");
        return -1;
    }

    return 0;
}

int write_unsigned
(
    FILE *file,
    const char *label,
    unsigned long long value
)
{
    return write_callback
           (
               file,
               label,
               unsigned_writer,
               &value
           );
}

int write_uuid
(
    FILE *file,
    const char *label,
    uuid_t uuid
)
{
    return write_callback
           (
               file,
               label,
               uuid_writer,
               &uuid
           );
}

int write_string
(
    FILE *file,
    const char *label,
    const char *string
)
{
    return write_blob(file, label, string, strlen(string));
}

int write_blob
(
    FILE *file,
    const char *label,
    const char *blob,
    size_t size
)
{
    struct wblob b;

    b.data = blob;
    b.size = size;

    return write_callback
           (
               file,
               label,
               blob_writer,
               &b
           );
}

/*******************************************************************************
 * READ FUNCTION DEFINITIONS
 ******************************************************************************/

int read_unsigned
(
    FILE *file,
    const char *label,
    unsigned long long *value
)
{
    return read_callback
           (
                file,
                label,
                unsigned_reader,
                value
           );
}

static int uuid_reader
(
    FILE *file,
    void *data
)
{
    char uuid_string[37];
    uuid_t **uuid;

    memset(uuid_string, 0, 37);
    uuid = data;

    if (1 != fscanf(file, "%36c", uuid_string))
    {
        w("uuid_reader(): Unable to read UUID.");
        return -1;
    }

    if (0 != uuid_parse(uuid_string, **uuid))
    {
        w("uuid_reader(): Unable to parse UUID.");
        return -1;
    }

    return 0;
}

int read_uuid
(
    FILE *file,
    const char *label,
    uuid_t uuid
)
{
    return read_callback
           (
                file,
                label,
                uuid_reader,
                &uuid
           );
}

static int string_reader
(
    FILE *file,
    void *data
)
{
    int rc = -1;
    char **string = data;
    unsigned long long length = 0;

    if (string) *string = NULL;

    if (!string)
    {
        w("string_reader(): Invalid string.");
        goto cleanup;
    }

    if (1 != fscanf(file, "%llu:", &length))
    {
        w("string_reader(): Unable to read string length.");
        goto cleanup;
    }

    if (!(*string = malloc(length + 1)))
    {
        w("string_reader(): malloc() failed.");
        goto cleanup;
    }

    if (1 != fread(*string, length, 1, file))
    {
        w("string_reader(): fread() failed.");
        goto cleanup;
    }

    (*string)[length] = 0;
    rc = 0;

cleanup:
    if (-1 == rc && string)
    {
        free(*string);
    }

    return rc;
}

int read_string
(
    FILE *file,
    const char *label,
    char **value
)
{
    return read_callback
           (
                file,
                label,
                string_reader,
                value
           );
}

static int blob_reader
(
    FILE *file,
    void *data
)
{
    int rc = -1;
    struct rblob *b;

    b = data;

    if (b->data) *(b->data) = NULL;
    if (b->size) *(b->size) = 0;

    if (!b->data)
    {
        w("blob_reader(): Invalid 'data' argument.");
        goto cleanup;
    }

    if (!b->size)
    {
        w("blob_reader(): Invalid 'size' argument.");
        goto cleanup;
    }

    if (1 != fscanf(file, "%zu:", b->size))
    {
        w("blob_reader(): Unable to read blob length.");
        goto cleanup;
    }

    if (!(*(b->data) = malloc(*(b->size))))
    {
        w("blob_reader(): malloc() failed.");
        goto cleanup;
    }

    if (1 != fread(*(b->data), *(b->size), 1, file))
    {
        w("blob_reader(): fread() failed.");
        goto cleanup;
    }

    rc = 0;

cleanup:
    if (-1 == rc && *(b->data))
    {
        free(*(b->data));
    }

    return rc;
}

int read_blob
(
    FILE *file,
    const char *label,
    char **data,
    size_t *size
)
{
    struct rblob b;

    b.data = data;
    b.size = size;

    return read_callback
           (
                file,
                label,
                blob_reader,
                &b
           );
}

/*******************************************************************************
 * READ FUNCTION DEFINITIONS
 ******************************************************************************/

static int read_callback
(
    FILE *file,
    const char *label,
    read_callback_t callback,
    void *data
)
{
    int rc = -1;
    int crc = -1;
    char nl = 0, col = 0;
    char *old_locale = NULL;
    char *old_locale2 = NULL;

    if (!file)
    {
        w("read_callback(): Invalid 'file' argument.");
        goto cleanup;
    }
    
    if (!label)
    {
        w("read_callback(): Invalid 'label' argument.");
        goto cleanup;
    }

    if (!callback)
    {
        w("read_callback(): Invalid 'callback' argument.");
        goto cleanup;
    }

    if (!data)
    {
        w("read_callback(): Invalid 'data' argument.");
        goto cleanup;
    }

    if (!(old_locale = setlocale(LC_ALL, NULL)))
    {
        w("read_callback(): setlocale(LC_ALL, NULL) failed.");
        goto cleanup;
    }

    if (!(old_locale2 = strdup(old_locale)))
    {
        w("read_callback(): strdup(old_locale) failed.");
        goto cleanup;
    }

    if (!setlocale(LC_ALL, "C"))
    {
        w("read_callback(): setlocale(LC_ALL, \"C\") failed.");
        goto cleanup;
    }

    if (-1 == expect_label(file, label))
    {
        w("read_callback(): Unable to read label '%s'.", label);
        goto cleanup;
    }

    if (1 != fscanf(file, "%c", &col))
    {
        w("read_callback(): Unable to read colon character.");
        goto cleanup;
    }

    if (col != ':') 
    {
        w("read_callback(): Missing colon.");
        goto cleanup;
    }

    if (-1 == (crc = callback(file, data))) goto cleanup;

    if (1 != fscanf(file, "%c", &nl))
    {
        w("read_callback(): Unable to read newline character.");
        goto cleanup;
    }

    if (nl != '\n') 
    {
        w("read_callback(): Missing newline.");
        goto cleanup;
    }

    rc = 0;
    
cleanup:
    if (old_locale2)
    {
        if (!setlocale(LC_ALL, old_locale2))
        {
            w("read_callback(): setlocale(LC_ALL, old_locale2) failed.");
        }
        free(old_locale2);
    }

    if (rc || crc) return -1;

    return 0;
}

static int unsigned_reader
(
    FILE *file,
    void *data
)
{
    unsigned long long *value = data;

    if (1 != fscanf(file, "%llu", value))
    {
        w("unsigned_reader(): Unable to read unsigned value.");
        return -1;
    }

    return 0;
}

/*******************************************************************************
 * EXPECT FUNCTION DEFINITIONS
 ******************************************************************************/

static int expect_label
(
    FILE *file,
    const char *expected
)
{
    int rc = -1;
    size_t len;
    char fmt[16];
    char actual[81];

    if (!file)
    {
        w("expect_label(): Invalid 'file' argument.");
        goto cleanup;
    }

    if (!expected)
    {
        w("expect_label(): Invalid 'expected' argument.");
        goto cleanup;
    }

    if (!is_label_valid(expected))
    {
        w("expect_label(): Invalid label.");
        goto cleanup;
    }

    len = strlen(expected);

    memset(actual, 0, 81);
    snprintf(fmt, 16, "%%%uc", (unsigned)len);

    if (1 != fscanf(file, fmt, actual))
    {
        w("expect_label(): Unable to read label.");
        goto cleanup;
    }

    if (0 != strcmp(expected, actual)) 
    {
        w("expect_label(): Expected '%s' but found '%s'.", expected, actual);
        goto cleanup;
    }

    rc = 0;

cleanup:
    return rc;
}

int expect_unsigned
(
    FILE *file,
    const char *label,
    unsigned long long expected
)
{
    unsigned long long actual;

    if (-1 == read_unsigned(file, label, &actual))
    {
        w("expect_unsigned(): Unable to read unsigned value.");
        return -1;
    }

    if (expected != actual) 
    {
        w("expect_unsigned(): Expected %llu but read %llu.", expected, actual);
        return -1;
    }

    return 0;
}

int expect_uuid
(
    FILE *file,
    const char *label,
    uuid_t expected
)
{
    uuid_t actual;
    char expected_str[37];

    if (-1 == read_uuid(file, label, actual))
    {
        w("expect_uuid(): Unable to read UUID.");
        return -1;
    }

    if (0 != uuid_compare(expected, actual)) 
    {
        uuid_unparse(expected, expected_str);
        w("expect_uuid(): Expected UUID '%s' not found.", expected_str);
        return -1;
    }

    return 0;
}

int expect_string
(
    FILE *file,
    const char *label,
    const char *expected
)
{
    int rc = -1;
    char *actual = NULL;

    if (!expected)
    {
        w("expect_string(): Invalid 'expected' argument.");
        goto cleanup;
    }
    
    if (-1 == read_string(file, label, &actual))
    {
        w("expect_string(): Unable to read string.");
        goto cleanup;
    }

    if (strcmp(expected, actual))
    {
        w("expect_string(): Expected '%s' but found '%s'.", expected, actual);
        goto cleanup;
    }

    rc = 0;

cleanup:
    free(actual);
    return rc;
}

/*******************************************************************************
 * FILE FUNCTION DEFINITIONS
 ******************************************************************************/

int read_file_content(const char *filename, char **data, size_t *size)
{
    int rc = -1;
    FILE *file;
    long offset;

    if (data) *data = NULL;
    if (size) *size = 0;

    if (!filename)
    {
        w("read_file_content(): Invalid 'filename' argument.");
        goto cleanup;
    }

    if (!data)
    {
        w("read_file_content(): Invalid 'data' argument.");
        goto cleanup;
    }

    if (!size)
    {
        w("read_file_content(): Invalid 'size' argument.");
        goto cleanup;
    }

    file = fopen(filename, "rb");
    if (!file)
    {
        w("read_file_content(): Unable to open file '%s'.", filename);
        goto cleanup;
    }

    if (-1 == fseek(file, 0, SEEK_END))
    {
        fclose(file);
        w("read_file_content(): Unable to seek to the end of the file '%s'.", filename);
        goto cleanup;
    }

    if (-1 == (offset = ftell(file)))
    {
        fclose(file);
        w("read_file_content(): Unable to obtain offset of file '%s'.", filename);
        goto cleanup;
    }

    if (-1 == fseek(file, 0, SEEK_SET))
    {
        fclose(file);
        w("read_file_content(): Unable to seek to the beginning of the file '%s'.", filename);
        goto cleanup;
    }

    *data = (char*)malloc(offset + 1);
    if (!*data)
    {
        fclose(file);
        w("read_file_content(): Unable to allocate data for file '%s'.", filename);
        goto cleanup;
    }

    if (1 != fread(*data, offset, 1, file))
    {
        free(*data);
        fclose(file);
        w("read_file_content(): Unable to read file content '%s'.", filename);
        goto cleanup;
    }

    (*data)[offset] = 0;
    *size = offset;

    rc = 0;

cleanup:
    if (file)
    {
        if (EOF == fclose(file))
        {
            w("read_file_content(): Error closing file.");
        }
    }

    if (-1 == rc)
    {
        if (data) free(*data);
    }

    return rc;
}
