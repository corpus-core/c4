#include "./bytes.h"
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t uint32_from_le(uint8_t* data) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  // Check alignment before dereferencing
  if (((uintptr_t) data % sizeof(uint32_t)) == 0) {
    // Safe to dereference
    return *(uint32_t*) data;
  }
  else {
    // Fallback to manual byte manipulation
    return (uint32_t) data[0] |
           ((uint32_t) data[1] << 8) |
           ((uint32_t) data[2] << 16) |
           ((uint32_t) data[3] << 24);
  }
#else
  // Manual byte manipulation for big-endian or unknown endianness
  return (uint32_t) data[0] |
         ((uint32_t) data[1] << 8) |
         ((uint32_t) data[2] << 16) |
         ((uint32_t) data[3] << 24);
#endif
}

uint16_t uint16_from_le(uint8_t* data) {
  return (uint16_t) data[0] |
         ((uint16_t) data[1] << 8);
}

uint64_t uint64_from_le(uint8_t* data) {
  return (uint64_t) data[0] |
         ((uint64_t) data[1] << 8) |
         ((uint64_t) data[2] << 16) |
         ((uint64_t) data[3] << 24) |
         ((uint64_t) data[4] << 32) |
         ((uint64_t) data[5] << 40) |
         ((uint64_t) data[6] << 48) |
         ((uint64_t) data[7] << 56);
}

uint64_t uint64_from_be(uint8_t* data) {
  return (uint64_t) data[7] |
         ((uint64_t) data[6] << 8) |
         ((uint64_t) data[5] << 16) |
         ((uint64_t) data[4] << 24) |
         ((uint64_t) data[3] << 32) |
         ((uint64_t) data[2] << 40) |
         ((uint64_t) data[1] << 48) |
         ((uint64_t) data[0] << 56);
}
void uint64_to_be(uint8_t* data, uint64_t value) {
  data[0] = (value >> 56) & 0xFF;
  data[1] = (value >> 48) & 0xFF;
  data[2] = (value >> 40) & 0xFF;
  data[3] = (value >> 32) & 0xFF;
  data[4] = (value >> 24) & 0xFF;
  data[5] = (value >> 16) & 0xFF;
  data[6] = (value >> 8) & 0xFF;
  data[7] = value & 0xFF;
}

void buffer_grow(buffer_t* buffer, size_t min_len) {
  if (buffer->data.data == NULL) {
    if (buffer->allocated > min_len) min_len = (size_t) buffer->allocated;
    buffer->data.data = malloc(min_len);
    buffer->allocated = (int32_t) min_len;
  }
  else if (buffer->allocated >= 0 && buffer->allocated < min_len) {
    size_t new_len = (size_t) buffer->allocated;
    while (new_len < min_len) new_len = (new_len + 1) * 3 / 2;
    buffer->data.data = realloc(buffer->data.data, new_len);
    buffer->allocated = (int32_t) new_len;
  }
}

void buffer_append(buffer_t* buffer, bytes_t data) {
  if (buffer->allocated < 0 && buffer->data.len + data.len > (uint32_t) (0 - buffer->allocated))
    data.len = ((uint32_t) (0 - buffer->allocated)) - buffer->data.len;
  if (!data.len) return;
  buffer_grow(buffer, buffer->data.len + data.len);
  if (data.data)
    memcpy(buffer->data.data + buffer->data.len, data.data, data.len);
  else
    memset(buffer->data.data + buffer->data.len, 0, data.len);
  buffer->data.len += data.len;
}

void buffer_free(buffer_t* buffer) {
  if (buffer->data.data && buffer->allocated > 0)
    free(buffer->data.data);
}

void print_hex(FILE* f, bytes_t data, char* prefix, char* suffix) {
  if (prefix) fprintf(f, "%s", prefix);
  for (uint32_t i = 0; i < data.len; i++)
    fprintf(f, "%02x", data.data[i]);
  if (suffix) fprintf(f, "%s", suffix);
}

bool bytes_all_equal(bytes_t a, uint8_t value) {
  for (uint32_t i = 0; i < a.len; i++)
    if (a.data[i] != value) return false;
  return true;
}

static inline int hexchar_to_int(char c) {
  if (isdigit(c)) return c - '0';
  if (isxdigit(c)) return tolower(c) - 'a' + 10;
  return -1; // Ungültiges Zeichen
}

int hex_to_bytes(const char* hexstring, int len, bytes_t buffer) {
  size_t hex_len = len == -1 ? strlen(hexstring) : (size_t) len;
  if (!hexstring || !buffer.data) return -1;
  int dst_offset = hex_len % 2;
  int src_offset = (hexstring[0] == '0' && hexstring[1] == 'x') ? 2 : 0;
  if (dst_offset) buffer.data[0] = hexchar_to_int(hexstring[src_offset++]);

  if ((hex_len - src_offset) % 2 || (buffer.len - dst_offset) < (hex_len - src_offset) / 2)
    return -1;

  for (size_t i = src_offset; i < hex_len; i += 2) {
    int high = hexchar_to_int(hexstring[i]);
    int low  = hexchar_to_int(hexstring[i + 1]);
    if (high == -1 || low == -1) return -1;
    buffer.data[dst_offset++] = (high << 4) | low;
  }

  return dst_offset;
}

void buffer_add_chars(buffer_t* buffer, const char* data) {
  if (!data) return;
  size_t len = strlen(data);
  buffer_append(buffer, bytes((uint8_t*) data, len + 1));
  buffer->data.len -= 1;
}

void buffer_add_hex_chars(buffer_t* buffer, bytes_t data, char* prefix, char* suffix) {
  uint32_t len = data.len * 2 + (prefix ? strlen(prefix) : 0) + (suffix ? strlen(suffix) : 0);
  buffer_grow(buffer, buffer->data.len + len + 1);
  buffer_add_chars(buffer, prefix);
  char tmp[4];

  for (size_t i = 0; i < data.len; i++) {
    sprintf(tmp, "%02x", data.data[i]);
    buffer_add_chars(buffer, tmp);
  }
  buffer_add_chars(buffer, suffix);
}

bytes_t bytes_dup(bytes_t data) {
  bytes_t result = {.data = malloc(data.len), .len = data.len};
  memcpy(result.data, data.data, data.len);
  return result;
}