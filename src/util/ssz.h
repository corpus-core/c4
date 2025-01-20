#ifndef ssz_h__
#define ssz_h__

#ifdef __cplusplus
extern "C" {
#endif

#include "bytes.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// Forward declarations
typedef struct ssz_def       ssz_def_t;
typedef struct ssz_list      ssz_list_t;
typedef struct ssz_container ssz_container_t;

/** the available SSZ Types */
typedef enum {
  SSZ_TYPE_UINT       = 0, /**< Basic uint type */
  SSZ_TYPE_BOOLEAN    = 1, /**< Basic boolean type (true or false) */
  SSZ_TYPE_CONTAINER  = 2, /**< Container type */
  SSZ_TYPE_VECTOR     = 3, /**< Vector type wih a fixed length*/
  SSZ_TYPE_LIST       = 4, /**< List type with a variable length*/
  SSZ_TYPE_BIT_VECTOR = 5, /**< Bit vector type  with a fixed length*/
  SSZ_TYPE_BIT_LIST   = 6, /**< Bit list type with a variable length*/
  SSZ_TYPE_UNION      = 7, /**< Union type with a variable length*/
  SSZ_TYPE_NONE       = 8, /**< a NONE-Type (only used in unions) */
} ssz_type_t;

/** a SSZ Type Definition */
struct ssz_def {
  const char* name; /**< name of the property or SSZ Def*/
  ssz_type_t  type; /**< General SSZ type  */
  union {
    struct {
      uint32_t len;
    } uint; /**< basic uint definitions */
    struct ssz_container {
      const ssz_def_t* elements; /**< the elements in the container */
      uint32_t         len;      /**< the number of elements in the container or un*/
    } container;                 /**< container or union definitions */
    struct ssz_list {
      const ssz_def_t* type; /**< the type of the elements in the vector or list */
      uint32_t         len;  /**< either the fixed length of the vector or max length of the list.*/
    } vector;                /**< vector or list defintions */
  } def;
};

/** a SSZ Object which holds a reference to the definition of the object and the bytes of the object */
typedef struct {
  bytes_t          bytes; /**< the bytes of the object */
  const ssz_def_t* def;   /**< the definition of the object */
} ssz_ob_t;

/** creates a new ssz_ob_t object */
#define ssz_ob(typename, data) \
  (ssz_ob_t) { .bytes = data, .def = &typename }

/** gets the uint64 value of the object */
static inline uint64_t ssz_uint64(ssz_ob_t ob) {
  return ob.bytes.len == 8 ? uint64_from_le(ob.bytes.data) : 0;
}

/** gets the uint32 value of the object */
static inline uint32_t ssz_uint32(ssz_ob_t ob) {
  return ob.bytes.len == 4 ? uint32_from_le(ob.bytes.data) : 0;
}

/** gets the length of the object */
uint32_t ssz_len(ssz_ob_t ob);

/** gets the bytes of the object */
static inline bytes_t ssz_bytes(ssz_ob_t ob) {
  return ob.bytes;
}

static inline bool ssz_is_error(ssz_ob_t ob) {
  return !ob.def || !ob.bytes.data;
}

/** gets the object of a list or vector at the index */
ssz_ob_t ssz_at(ssz_ob_t ob, uint32_t index);

/** gets the value of a field with the given name. If the data is not a container or union or if the field is not found, it will return an empty object */
ssz_ob_t ssz_get(ssz_ob_t* ob, char* name);

static inline uint64_t ssz_get_uint64(ssz_ob_t* ob, char* name) {
  return ssz_uint64(ssz_get(ob, name));
}

/** gets the value of a union. If the object is not a union, it will return an empty object. A Object with the type SSZ_TYPE_NONE will be returned if the union is empty */
ssz_ob_t ssz_union(ssz_ob_t ob);

/** dumps the object to a file */
void ssz_dump(FILE* f, ssz_ob_t ob, bool include_name, int intend);

/** hashes the object */
void ssz_hash_tree_root(ssz_ob_t ob, uint8_t* out);

extern const ssz_def_t ssz_uint8;
extern const ssz_def_t ssz_bytes32;
extern const ssz_def_t ssz_bls_pubky;

#define SSZ_BOOLEAN(property)       \
  {                                 \
      .name     = property,         \
      .type     = SSZ_TYPE_BOOLEAN, \
      .def.uint = {.len = 1}}

#define SSZ_UINT(property, length)                                        \
  {                                                                       \
    .name = property, .type = SSZ_TYPE_UINT, .def.uint = {.len = length } \
  }
#define SSZ_LIST(property, typePtr, max_len)                                   \
  {                                                                            \
    .name = property, .type = SSZ_TYPE_LIST, .def.vector = {.len  = max_len,   \
                                                            .type = &typePtr } \
  }
#define SSZ_VECTOR(property, typePtr, length)                                    \
  {                                                                              \
    .name = property, .type = SSZ_TYPE_VECTOR, .def.vector = {.len  = length,    \
                                                              .type = &typePtr } \
  }
#define SSZ_BIT_LIST(property, max_length)                                          \
  {                                                                                 \
    .name = property, .type = SSZ_TYPE_BIT_LIST, .def.vector = {.len  = max_length, \
                                                                .type = NULL }      \
  }
#define SSZ_BIT_VECTOR(property, length)                                          \
  {                                                                               \
    .name = property, .type = SSZ_TYPE_BIT_VECTOR, .def.vector = {.len  = length, \
                                                                  .type = NULL }  \
  }
#define SSZ_CONTAINER(propname, children)                           \
  {                                                                 \
    .name          = propname,                                      \
    .type          = SSZ_TYPE_CONTAINER,                            \
    .def.container = {.elements = children,                         \
                      .len      = sizeof(children) / sizeof(ssz_def_t) } \
  }
#define SSZ_UNION(propname, children)                               \
  {                                                                 \
    .name          = propname,                                      \
    .type          = SSZ_TYPE_UNION,                                \
    .def.container = {.elements = children,                         \
                      .len      = sizeof(children) / sizeof(ssz_def_t) } \
  }
#define SSZ_BYTE                   ssz_uint8
#define SSZ_BYTES(name, limit)     SSZ_LIST(name, ssz_uint8, limit)
#define SSZ_BYTE_VECTOR(name, len) SSZ_VECTOR(name, ssz_uint8, len)
#define SSZ_BYTES32(name)          SSZ_BYTE_VECTOR(name, 32)
#define SSZ_ADDRESS(name)          SSZ_BYTE_VECTOR(name, 20)
#define SSZ_UINT64(name)           SSZ_UINT(name, 8)
#define SSZ_UINT32(name)           SSZ_UINT(name, 4)
#define SSZ_UINT16(name)           SSZ_UINT(name, 2)
#define SSZ_UINT8(name)            SSZ_UINT(name, 1)
#define SSZ_NONE                   {.name = "NONE", .type = SSZ_TYPE_NONE}

typedef struct {
  ssz_def_t*     def;
  bytes_buffer_t fixed;
  bytes_buffer_t dynamic;
} ssz_builder_t;

void ssz_add_bytes(ssz_builder_t* buffer, char* name, bytes_t data);
void ssz_add_uint64(ssz_builder_t* buffer, uint64_t value);
void ssz_add_uint32(ssz_builder_t* buffer, uint32_t value);
void ssz_add_uint16(ssz_builder_t* buffer, uint16_t value);
void ssz_add_uint8(ssz_builder_t* buffer, uint8_t value);
void ssz_buffer_free(ssz_builder_t* buffer);
// converts the ssz_buffer to bytes and the frees up the buffer
// make sure to free the returned after using
ssz_ob_t ssz_builder_to_bytes(ssz_builder_t* buffer);
#ifdef __cplusplus
}
#endif

#endif