/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `nanotar.c` for details.
 */

#ifndef NANOTAR_H
#define NANOTAR_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define NTAR_VERSION "0.1.0"

enum {
  NTAR_ESUCCESS     =  0,
  NTAR_EFAILURE     = -1,
  NTAR_EOPENFAIL    = -2,
  NTAR_EREADFAIL    = -3,
  NTAR_EWRITEFAIL   = -4,
  NTAR_ESEEKFAIL    = -5,
  NTAR_EBADCHKSUM   = -6,
  NTAR_ENULLRECORD  = -7,
  NTAR_ENOTFOUND    = -8
};

enum {
  NTAR_TREG   = '0',
  NTAR_TLNK   = '1',
  NTAR_TSYM   = '2',
  NTAR_TCHR   = '3',
  NTAR_TBLK   = '4',
  NTAR_TDIR   = '5',
  NTAR_TFIFO  = '6'
};

typedef struct {
  unsigned mode;
  unsigned owner;
  size_t size;
  unsigned mtime;
  unsigned type;
  char name[257];
  char linkname[101];
} ntar_header_t;


typedef struct ntar_t ntar_t;

struct ntar_t {
  int (*read)(ntar_t *tar, void *data, size_t size);
  int (*write)(ntar_t *tar, const void *data, size_t size);
  int (*seek)(ntar_t *tar, size_t pos);
  int (*close)(ntar_t *tar);
  void *stream;
  size_t pos;
  size_t remaining_data;
  size_t last_header;
};


const char* ntar_strerror(int err);

int ntar_open(ntar_t *tar, const char *filename, const char *mode);
int ntar_close(ntar_t *tar);

int ntar_seek(ntar_t *tar, size_t pos);
int ntar_rewind(ntar_t *tar);
int ntar_next(ntar_t *tar);
int ntar_find(ntar_t *tar, const char *name, ntar_header_t *h);
int ntar_read_header(ntar_t *tar, ntar_header_t *h);
int ntar_read_data(ntar_t *tar, void *ptr, size_t size);

int ntar_write_header(ntar_t *tar, const ntar_header_t *h);
int ntar_write_file_header(ntar_t *tar, const char *name, size_t size);
int ntar_write_dir_header(ntar_t *tar, const char *name);
int ntar_write_data(ntar_t *tar, const void *data, size_t size);
int ntar_finalize(ntar_t *tar);

#ifdef __cplusplus
}
#endif

#endif
