/*
 * efilipo/efilipo.c
 * FOOOO
 *
 * Copyright (c) 2010 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>

#include <stdarg.h>

#define PROGNAME "efilipo"

#define streq(a,b) (strcmp((a), (b)) == 0)

#define MAX_INPUT (16)
#define BUFFER_SIZE (128*1024)

#define MODE_CREATE (1)

// variables


//
// error functions
//

void error(const char *msg, ...)
{
  va_list par;
  char buf[4096];

  va_start(par, msg);
  vsnprintf(buf, 4096, msg, par);
  va_end(par);

  fprintf(stderr, PROGNAME ": %s\n", buf);
}

void errore(const char *msg, ...)
{
  va_list par;
  char buf[4096];

  va_start(par, msg);
  vsnprintf(buf, 4096, msg, par);
  va_end(par);

  fprintf(stderr, PROGNAME ": %s: %s\n", buf, strerror(errno));
}

//
// sector I/O functions
//

/*
UINTN read_sector(UINT64 lba, UINT8 *buffer)
{
    off_t   offset;
    off_t   result_seek;
    ssize_t result_read;
    
    offset = lba * 512;
    result_seek = lseek(fd, offset, SEEK_SET);
    if (result_seek != offset) {
        errore("Seek to %llu failed", offset);
        return 1;
    }
    
    result_read = read(fd, buffer, 512);
    if (result_read < 0) {
        errore("Data read failed at position %llu", offset);
        return 1;
    }
    if (result_read != 512) {
        errore("Data read fell short at position %llu", offset);
        return 1;
    }
    return 0;
}

UINTN write_sector(UINT64 lba, UINT8 *buffer)
{
    off_t   offset;
    off_t   result_seek;
    ssize_t result_write;
    
    offset = lba * 512;
    result_seek = lseek(fd, offset, SEEK_SET);
    if (result_seek != offset) {
        errore("Seek to %llu failed", offset);
        return 1;
    }
    
    result_write = write(fd, buffer, 512);
    if (result_write < 0) {
        errore("Data write failed at position %llu", offset);
        return 1;
    }
    if (result_write != 512) {
        errore("Data write fell short at position %llu", offset);
        return 1;
    }
    return 0;
}
*/


long parse_arch(const char *name) {
  if (streq(name, "i386"))
    return 0x07;
  else if (streq(name, "x86_64"))
    return 0x01000007L;
  else {
    error("Unknown architecture: %s", name);
    exit(1);
  }
}

void put_be_long(unsigned char *buffer, unsigned long value) {
  buffer[0] = (unsigned char)(value & 0xff);
  buffer[1] = (unsigned char)((value >> 8) & 0xff);
  buffer[2] = (unsigned char)((value >> 16) & 0xff);
  buffer[3] = (unsigned char)((value >> 24) & 0xff);
}

void write_buffer(int fd, void *buffer, unsigned long size) {
  unsigned long done = 0;
  ssize_t written;
  char *p = (char *)buffer;

  while (done < size) {
    written = write(fd, p + done, size - done);
    if (written <= 0) {
      if (errno == EINTR || errno == EAGAIN)
        continue;
      errore("Write failed");
      exit(1);
    }
    done += written;
  }
}

void create_fat(char *output_filename,
                int input_count,
                char *input_filename[],
                long input_arch[]) {
  int i, input_fd, output_fd;
  unsigned long input_size[MAX_INPUT];
  unsigned long input_offset[MAX_INPUT];
  unsigned long copy_done;
  unsigned long header_size;
  unsigned char *header;
  void *buffer;
  ssize_t got;
  struct stat sb;

  header_size = 20 * input_count + 8;
  header = (unsigned char *)malloc(header_size);
  if (header == NULL) {
    errore("Out of memory");
    exit(1);
  }
  put_be_long(header, 0x0ef1fab9);
  put_be_long(header + 4, input_count);
  for (i = 0; i < input_count; ++i) {
    if (stat(input_filename[i], &sb) < 0) {
      errore("Can't stat %.300s", input_filename[i]);
      exit(1);
    }
    if (!S_ISREG(sb.st_mode)) {
      error("Not a regular file: %.300s", input_filename[i]);
      exit(1);
    }
    input_size[i] = sb.st_size;
    if (i == 0)
      input_offset[i] = header_size;
    else
      input_offset[i] = input_offset[i-1] + input_size[i-1];

    put_be_long(header + 20 * i + 8, input_arch[i]);
    put_be_long(header + 20 * i + 12, 0x03);  // subtype is fixed for now
    put_be_long(header + 20 * i + 16, input_offset[i]);
    put_be_long(header + 20 * i + 20, input_size[i]);
    put_be_long(header + 20 * i + 24, 0);  // no alignment
  }

  output_fd = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (output_fd < 0) {
    errore("Can't open %.300s for writing", output_filename);
    exit(1);
  }
  write_buffer(output_fd, header, header_size);
  free(header);

  buffer = malloc(BUFFER_SIZE);
  if (buffer == NULL) {
    errore("Out of memory");
    exit(1);
  }
  for (i = 0; i < input_count; ++i) {
    input_fd = open(input_filename[i], O_RDONLY);
    if (input_fd < 0) {
      errore("Can't open %.300s for reading", input_filename[i]);
      exit(1);
    }

    copy_done = 0;
    while (copy_done < input_size[i]) {
      size_t to_read = input_size[i] - copy_done;
      if (to_read > BUFFER_SIZE)
        to_read = BUFFER_SIZE;
      got = read(input_fd, buffer, to_read);
      if (got < 0) {
        if (errno == EINTR || errno == EAGAIN)
          continue;
        errore("Read from %.300s failed", input_filename[i]);
        exit(1);
      } else if (got == 0) {
        error("Premature end of file reading %.300s", input_filename[i]);
        exit(1);
      }
      write_buffer(output_fd, buffer, got);
      copy_done += got;
    }
    close(input_fd);
  }
  if (close(output_fd) < 0) {
    errore("Can't close %.300s", output_filename);
    exit(1);
  }
}

//
// main entry point
//

int main(int argc, char *argv[])
{
  int i;
  int mode = 0;
  char *output_filename = NULL;
  int input_count = 0;
  char *input_filename[MAX_INPUT];
  long input_arch[MAX_INPUT];

  for (i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      if (streq(argv[i], "-create")) {
        mode = MODE_CREATE;
      } else if (streq(argv[i], "-output")) {
        if (i + 2 > argc) {
          error("Not enough arguments for -output");
          exit(1);
        }
        output_filename = argv[++i];
      } else if (streq(argv[i], "-arch")) {
        if (i + 3 > argc) {
          error("Not enough arguments for -arch");
          exit(1);
        }
        if (input_count >= MAX_INPUT) {
          error("Too many input files");
          exit(1);
        }
        input_arch[input_count] = parse_arch(argv[++i]);
        input_filename[input_count] = argv[++i];
        input_count++;
      } else {
        error("Unrecognized flag: %s", argv[i]);
        exit(1);
      }
    } else {
      // No flag, treat as input with unspecified architecture.
      if (input_count >= MAX_INPUT) {
        error("Too many input files");
        exit(1);
      }
      input_filename[input_count] = argv[i];
      input_arch[input_count] = 0;
      input_count++;
    }
  }

  if (mode == 0) {
    error("One of -create, ... must be specified");
    exit(1);
  }

  if (mode == MODE_CREATE) {
    if (input_count == 0) {
      error("No input files given for -create");
      exit(1);
    }
    if (output_filename == NULL) {
      error("No output file given for -create");
      exit(1);
    }

    for (i = 0; i < input_count; ++i) {
      if (input_arch[i] == 0) {
        error("Input file architecture detection not implemented, use -arch");
        exit(1);
      }
    }

    create_fat(output_filename, input_count, input_filename, input_arch);
  }

  return 0;
}
