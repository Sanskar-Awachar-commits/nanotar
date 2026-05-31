# nanotar
A lightweight tar library written in C

> **Note:** This project is a modified fork of `microtar`, originally created by rxi. It has been updated to include `ustar` format support, various bug fixes and strict typing (removing void pointers for easy c++ integration).

## Basic Usage
The library consists of `nanotar.c` and `nanotar.h`. These two files can be
dropped into an existing project and compiled along with it.


#### Reading
```c
ntar_t tar;
ntar_header_t h;
char *p;

/* Open archive for reading */
ntar_open(&tar, "test.tar", "r");

/* Print all file names and sizes */
while ( (ntar_read_header(&tar, &h)) != NTAR_ENULLRECORD ) {
  printf("%s (%d bytes)\n", h.name, h.size);
  ntar_next(&tar);
}

/* Load and print contents of file "test.txt" */
ntar_find(&tar, "test.txt", &h);
p = calloc(1, h.size + 1);
ntar_read_data(&tar, p, h.size);
printf("%s", p);
free(p);

/* Close archive */
ntar_close(&tar);
```

#### Writing
```c
ntar_t tar;
const char *str1 = "Hello world";
const char *str2 = "Goodbye world";

/* Open archive for writing */
ntar_open(&tar, "test.tar", "w");

/* Write strings to files `test1.txt` and `test2.txt` */
ntar_write_file_header(&tar, "test1.txt", strlen(str1));
ntar_write_data(&tar, str1, strlen(str1));
ntar_write_file_header(&tar, "test2.txt", strlen(str2));
ntar_write_data(&tar, str2, strlen(str2));

/* Finalize -- this needs to be the last thing done before closing */
ntar_finalize(&tar);

/* Close archive */
ntar_close(&tar);
```


## Error handling
All functions which return an `int` will return `NTAR_ESUCCESS` if the operation
is successful. If an error occurs an error value less-than-zero will be
returned; this value can be passed to the function `ntar_strerror()` to get its
corresponding error string.


## Wrapping a stream
If you want to read or write from something other than a file, the `ntar_t`
struct can be manually initialized with your own callback functions and a
`stream` pointer.

All callback functions are passed a pointer to the `ntar_t` struct as their
first argument. They should return `NTAR_ESUCCESS` if the operation succeeds
without an error, or an integer below zero if an error occurs.

After the `stream` field has been set, all required callbacks have been set and
all unused fields have been zeroset the `ntar_t` struct can be safely used with
the nanotar functions. `ntar_open` *should not* be called if the `ntar_t`
struct was initialized manually.

#### Reading
The following callbacks should be set for reading an archive from a stream:

Name    | Arguments                                | Description
--------|------------------------------------------|---------------------------
`read`  | `ntar_t *tar, void *data, unsigned size` | Read data from the stream
`seek`  | `ntar_t *tar, unsigned pos`              | Set the position indicator
`close` | `ntar_t *tar`                            | Close the stream

#### Writing
The following callbacks should be set for writing an archive to a stream:

Name    | Arguments                                      | Description
--------|------------------------------------------------|---------------------
`write` | `ntar_t *tar, const void *data, unsigned size` | Write data to the stream


## License
This library is free software; you can redistribute it and/or modify it under
the terms of the MIT license. See [LICENSE](LICENSE) for details.
