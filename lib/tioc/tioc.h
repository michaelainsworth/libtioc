#ifndef LIB_TIOC_H
#define LIB_TIOC_H

#include <stdio.h>
#include <uuid/uuid.h>

/*******************************************************************************
 * OVERVIEW
 *
 * The io library is used for serialising data textually.
 *
 * By convention, all data is prefixed with a label, and the label is separated
 * from the content with a colon ':'.
 *
 * Labels must be between 1 and 80 characters long, and must consist of the
 * characters 'a' through 'z', and '_' (basically C identifiers minus digits).
 ******************************************************************************/

/*******************************************************************************
 * WRITE FUNCTION DECLARATIONS
 ******************************************************************************/

/*
 * Writes an unsigned long long integer to the file.
 *
 * Unsigned integers are written in "<label>:<value>\n" format.
 *
 * Returns -1 on failure, 0 on success.
 */
int write_unsigned
(
    FILE *file,
    const char *label,
    unsigned long long value
);

/*
 * Writes a UUID to the file.
 *
 * The UUID is first converted to a string.
 *
 *     name:e4fa98a6-929a-4436-9f66-c38f9371db62
 *
 * Returns -1 on failure, 0 on success.
 */
int write_uuid
(
    FILE *file,
    const char *label,
    uuid_t uuid
);

/*
 * Writes a NULL-terminated string to the file specified.
 *
 * Like blobs, strings are written in "<label>:<size>:<content>\n" format, e.g.:
 *
 *     name:4:John
 *
 * Returns -1 on failure, 0 on success.
 */
int write_string
(
    FILE *file,
    const char *label,
    const char *string
);

/*
 * Writes a blob to the file specified.
 *
 * Strings are written in "<label>:<size>:<content>\n" format, e.g.:
 *
 *     name:4:John
 *
 * Returns -1 on failure, 0 on success.
 */
int write_blob
(
    FILE *file,
    const char *label,
    const char *blob,
    size_t size
);

/*******************************************************************************
 * READ FUNCTION DECLARATIONS
 ******************************************************************************/

/*
 * Reads an unsigned value into value.
 *
 * Returns -1 on failure, 0 on success.
 */
int read_unsigned
(
    FILE *file,
    const char *label,
    unsigned long long *value
);

/*
 * Reads a UUID from the file.
 *
 * Returns -1 on failure, 0 on success.
 */
int read_uuid
(
    FILE *file,
    const char *label,
    uuid_t uuid
);

/*
 * Reads a string from the file.
 *
 * Returns -1 on failure, 0 on success.
 *
 * The resulting string should be free()'d with malloc.
 */
int read_string
(
    FILE *file,
    const char *label,
    char **value
);

/*
 * Reads a blob from the file.
 *
 * Returns -1 on failure, 0 on success.
 *
 * The resulting string should be free()'d with malloc.
 */
int read_blob
(
    FILE *file,
    const char *label,
    char **data,
    size_t *size
);

/*******************************************************************************
 * EXPECT FUNCTION DECLARATIONS
 ******************************************************************************/

/*
 * Expects an unsigned value from a value.
 *
 * This is the same as read_unsigned, except that if the value that is read
 * does not match the value supplied, an error is returned.
 *
 * Returns -1 on failure, 0 on success.
 */
int expect_unsigned
(
    FILE *file,
    const char *label,
    unsigned long long expected
);

/*
 * Expect an exact UUID from the file.
 *
 * This is similar to read_uuid, but if the uuid in the file
 * does not match the UUID specified, an error is returned.
 *
 * Returns -1 on failure, 0 on success.
 */
int expect_uuid
(
    FILE *file,
    const char *label,
    uuid_t expected
);

/*
 * Expect an exact string from the file.
 *
 * This is similar to read_string, but if the string in the file
 * does not match the string specified, an error is returned.
 *
 * Returns -1 on failure, 0 on success.
 */
int expect_string
(
    FILE *file,
    const char *label,
    const char *expected
);

/*******************************************************************************
 * FILE FUNCTION DECLARATIONS
 ******************************************************************************/

/*
 * Reads the entire file into a buffer allocated with malloc().
 *
 * Returns -1 on failure, 0 on success.
 */
int read_file_content(const char *filename, char **data, size_t *size);

#endif /* #ifndef LIB_TIOC_H */

