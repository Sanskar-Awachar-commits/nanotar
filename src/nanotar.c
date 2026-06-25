/*
 * Copyright (c) 2017 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#include "nanotar.h"

#pragma pack(push, 1)
typedef struct {
  char name[100];
  char mode[8];
  char owner[8];
  char group[8];
  char size[12];
  char mtime[12];
  char checksum[8];
  char type;
  char linkname[100];
  char _padding[255];
} ntar_raw_header_t;
#pragma pack(pop)

static size_t round_up(size_t n, size_t incr) {
  return n + (incr - n % incr) % incr;
}


static unsigned checksum(const ntar_raw_header_t* rh) {
  size_t i;
  unsigned char *p = (unsigned char*) rh;
  unsigned res = 256;
  for (i = 0; i < offsetof(ntar_raw_header_t, checksum); i++) {
    res += p[i];
  }
  for (i = offsetof(ntar_raw_header_t, type); i < sizeof(*rh); i++) {
    res += p[i];
  }
  return res;
}


static int tread(ntar_t *tar, void *data, size_t size) {
  int err = tar->read(tar, data, size);
  tar->pos += size;
  return err;
}


static int twrite(ntar_t *tar, const void *data, size_t size) {
  int err = tar->write(tar, data, size);
  tar->pos += size;
  return err;
}


static int write_null_bytes(ntar_t *tar, int n) {
  int i, err;
  char nul = '\0';
  for (i = 0; i < n; i++) {
    err = twrite(tar, &nul, 1);
    if (err) {
      return err;
    }
  }
  return NTAR_ESUCCESS;
}


static int raw_to_header(ntar_header_t *h, const ntar_raw_header_t *rh) {
  unsigned chksum1, chksum2;

  /* If the checksum starts with a null byte we assume the record is NULL */
  if (*rh->checksum == '\0') {
    return NTAR_ENULLRECORD;
  }

  /* Build and compare checksum */
  chksum1 = checksum(rh);
  sscanf(rh->checksum, "%o", &chksum2);
  if (chksum1 != chksum2) {
    return NTAR_EBADCHKSUM;
  }

  /* Load raw header into header */
  sscanf(rh->mode, "%7o", &h->mode);   // max 7 octal chars + space/null
  sscanf(rh->owner, "%7o", &h->owner);
  sscanf(rh->size, "%11zo", &h->size); // max 11 octal chars + space/null
  sscanf(rh->mtime, "%11o", &h->mtime);
  h->type = rh->type;
  memcpy(h->name, rh->name, 100);
  h->name[100] = '\0';
  memcpy(h->linkname, rh->linkname, 100);
  h->linkname[100] = '\0';

  return NTAR_ESUCCESS;
}


static int header_to_raw(ntar_raw_header_t *rh, const ntar_header_t *h) {
  unsigned chksum;

  /* Load header into raw header */
  memset(rh, 0, sizeof(*rh));
  sprintf(rh->mode, "%07o", h->mode);
  sprintf(rh->owner, "%07o", h->owner);
  sprintf(rh->size, "%011zo", h->size); 
  sprintf(rh->mtime, "%011o", h->mtime);
  rh->type = h->type ? h->type : NTAR_TREG;
  strncpy(rh->name, h->name, 100);
  strncpy(rh->linkname, h->linkname, 100);

  /* Calculate and write checksum */
  chksum = checksum(rh);
  sprintf(rh->checksum, "%06o", chksum);
  rh->checksum[7] = ' ';

  return NTAR_ESUCCESS;
}


const char* ntar_strerror(int err) {
  switch (err) {
    case NTAR_ESUCCESS     : return "success";
    case NTAR_EFAILURE     : return "failure";
    case NTAR_EOPENFAIL    : return "could not open";
    case NTAR_EREADFAIL    : return "could not read";
    case NTAR_EWRITEFAIL   : return "could not write";
    case NTAR_ESEEKFAIL    : return "could not seek";
    case NTAR_EBADCHKSUM   : return "bad checksum";
    case NTAR_ENULLRECORD  : return "null record";
    case NTAR_ENOTFOUND    : return "file not found";
  }
  return "unknown error";
}


static int file_write(ntar_t *tar, const void *data, size_t size) {
  size_t res = fwrite(data, 1, size, (FILE*)tar->stream);
  return (res == size) ? NTAR_ESUCCESS : NTAR_EWRITEFAIL;
}

static int file_read(ntar_t *tar, void *data, size_t size) {
  size_t res = fread(data, 1, size, (FILE*)tar->stream);
  return (res == size) ? NTAR_ESUCCESS : NTAR_EREADFAIL;
}

static int file_seek(ntar_t *tar, size_t offset) {
  int res = fseek((FILE*)tar->stream, offset, SEEK_SET);
  return (res == 0) ? NTAR_ESUCCESS : NTAR_ESEEKFAIL;
}

static int file_close(ntar_t *tar) {
  fclose((FILE*)tar->stream);
  return NTAR_ESUCCESS;
}


int ntar_open(ntar_t *tar, const char *filename, const char *mode) {
  int err;
  ntar_header_t h;

  /* Init tar struct and functions */
  memset(tar, 0, sizeof(*tar));
  tar->write = file_write;
  tar->read = file_read;
  tar->seek = file_seek;
  tar->close = file_close;

  /* Assure mode is always binary */
  if ( strchr(mode, 'r') ) mode = "rb";
  if ( strchr(mode, 'w') ) mode = "wb";
  if ( strchr(mode, 'a') ) mode = "ab";
  /* Open file */
  tar->stream = fopen(filename, mode);
  if (!tar->stream) {
    return NTAR_EOPENFAIL;
  }
  /* Read first header to check it is valid if mode is `r` */
  if (*mode == 'r') {
    err = ntar_read_header(tar, &h);
    if (err != NTAR_ESUCCESS) {
      ntar_close(tar);
      return err;
    }
  }

  /* Return ok */
  return NTAR_ESUCCESS;
}


int ntar_close(ntar_t *tar) {
  return tar->close(tar);
}


int ntar_seek(ntar_t *tar, size_t pos) {
  int err = tar->seek(tar, pos);
  tar->pos = pos;
  return err;
}


int ntar_rewind(ntar_t *tar) {
  tar->remaining_data = 0;
  tar->last_header = 0;
  return ntar_seek(tar, 0);
}


int ntar_next(ntar_t *tar) {
  int err;
  size_t n;
  ntar_header_t h;
  /* Load header */
  err = ntar_read_header(tar, &h);
  if (err) {
    return err;
  }
  /* Seek to next record */
  n = round_up(h.size, 512) + sizeof(ntar_raw_header_t);
  return ntar_seek(tar, tar->pos + n);
}


int ntar_find(ntar_t *tar, const char *name, ntar_header_t *h) {
  int err;
  ntar_header_t header;
  /* Start at beginning */
  err = ntar_rewind(tar);
  if (err) {
    return err;
  }
  /* Iterate all files until we hit an error or find the file */
  while ( (err = ntar_read_header(tar, &header)) == NTAR_ESUCCESS ) {
    if ( !strcmp(header.name, name) ) {
      if (h) {
        *h = header;
      }
      return NTAR_ESUCCESS;
    }
    ntar_next(tar);
  }
  /* Return error */
  if (err == NTAR_ENULLRECORD) {
    err = NTAR_ENOTFOUND;
  }
  return err;
}


int ntar_read_header(ntar_t *tar, ntar_header_t *h) {
  int err;
  ntar_raw_header_t rh;
  /* Save header position */
  tar->last_header = tar->pos;
  /* Read raw header */
  err = tread(tar, &rh, sizeof(rh));
  if (err) {
    return err;
  }
  /* Seek back to start of header */
  err = ntar_seek(tar, tar->last_header);
  if (err) {
    return err;
  }
  /* Load raw header into header struct and return */
  return raw_to_header(h, &rh);
}


int ntar_read_data(ntar_t *tar, void *ptr, size_t size) {
  int err;
  /* If we have no remaining data then this is the first read, we get the size,
   * set the remaining data and seek to the beginning of the data */
  if (tar->remaining_data == 0) {
    ntar_header_t h;
    /* Read header */
    err = ntar_read_header(tar, &h);
    if (err) {
      return err;
    }
    /* Seek past header and init remaining data */
    err = ntar_seek(tar, tar->pos + sizeof(ntar_raw_header_t));
    if (err) {
      return err;
    }
    tar->remaining_data = h.size;
  }
  /* Read data */
  err = tread(tar, ptr, size);
  if (err) {
    return err;
  }
  tar->remaining_data -= size;
  /* If there is no remaining data we've finished reading and seek back to the
   * header */
  if (tar->remaining_data == 0) {
    return ntar_seek(tar, tar->last_header);
  }
  return NTAR_ESUCCESS;
}


int ntar_write_header(ntar_t *tar, const ntar_header_t *h) {
  ntar_raw_header_t rh;
  /* Build raw header and write */
  header_to_raw(&rh, h);
  tar->remaining_data = h->size;
  return twrite(tar, &rh, sizeof(rh));
}


int ntar_write_file_header(ntar_t *tar, const char *name, size_t size) {
  ntar_header_t h;
  /* Build header */
  memset(&h, 0, sizeof(h));
  strncpy(h.name, name, 100); 
  h.name[100] = '\0';
  h.size = size;
  h.type = NTAR_TREG;
  h.mode = 0664;
  /* Write header */
  return ntar_write_header(tar, &h);
}


int ntar_write_dir_header(ntar_t *tar, const char *name) {
  ntar_header_t h;
  /* Build header */
  memset(&h, 0, sizeof(h));
  strncpy(h.name, name, 100); 
  h.name[100] = '\0';
  h.type = NTAR_TDIR;
  h.mode = 0775;
  /* Write header */
  return ntar_write_header(tar, &h);
}


int ntar_write_data(ntar_t *tar, const void *data, size_t size) {
  int err;
  /* Write data */
  err = twrite(tar, data, size);
  if (err) {
    return err;
  }
  tar->remaining_data -= size;
  /* Write padding if we've written all the data for this file */
  if (tar->remaining_data == 0) {
    return write_null_bytes(tar, round_up(tar->pos, 512) - tar->pos);
  }
  return NTAR_ESUCCESS;
}


int ntar_finalize(ntar_t *tar) {
  /* Write two NULL records */
  return write_null_bytes(tar, sizeof(ntar_raw_header_t) * 2);
}
