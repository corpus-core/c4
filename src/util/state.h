#ifndef C4_STATE_H
#define C4_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bytes.h"
#include "chains.h"
#include "crypto.h"
#include "json.h"

typedef enum {
  C4_DATA_TYPE_BEACON_API = 0,
  C4_DATA_TYPE_ETH_RPC    = 1,
  C4_DATA_TYPE_REST_API   = 2
} data_request_type_t;

typedef enum {
  C4_DATA_ENCODING_JSON = 0,
  C4_DATA_ENCODING_SSZ  = 1
} data_request_encoding_t;

typedef enum {
  C4_DATA_METHOD_GET    = 0,
  C4_DATA_METHOD_POST   = 1,
  C4_DATA_METHOD_PUT    = 2,
  C4_DATA_METHOD_DELETE = 3
} data_request_method_t;

typedef enum {
  C4_SUCCESS = 0,
  C4_ERROR   = -1,
  C4_PENDING = 2
} c4_status_t;

typedef struct data_request {
  chain_id_t              chain_id;
  data_request_type_t     type;
  data_request_encoding_t encoding;
  char*                   url;
  data_request_method_t   method;
  bytes_t                 payload;
  bytes_t                 response;
  char*                   error;
  struct data_request*    next;
  bytes32_t               id;
} data_request_t;

typedef struct {
  data_request_t* requests;
  char*           error;
} c4_state_t;

void            c4_state_free(c4_state_t* state);
data_request_t* c4_state_get_data_request_by_id(c4_state_t* state, bytes32_t id);
data_request_t* c4_state_get_data_request_by_url(c4_state_t* state, char* url);
bool            c4_state_is_pending(data_request_t* req);
void            c4_state_add_request(c4_state_t* state, data_request_t* data_request);
data_request_t* c4_state_get_pending_request(c4_state_t* state);

#define TRY_ASYNC(fn)                      \
  do {                                     \
    c4_status_t state = fn;                \
    if (state != C4_SUCCESS) return state; \
  } while (0)

#define TRY_2_ASYNC(fn1, fn2)                \
  do {                                       \
    c4_status_t state1 = fn1;                \
    c4_status_t state2 = fn2;                \
    if (state1 != C4_SUCCESS) return state1; \
    if (state2 != C4_SUCCESS) return state2; \
  } while (0)

#define TRY_ASYNC_FINAL(fn, final)         \
  do {                                     \
    c4_status_t state = fn;                \
    final;                                 \
    if (state != C4_SUCCESS) return state; \
  } while (0)

#define TRY_ASYNC_CATCH(fn, cleanup) \
  do {                               \
    c4_status_t state = fn;          \
    if (state != C4_SUCCESS) {       \
      cleanup;                       \
      return state;                  \
    }                                \
  } while (0)

#define THROW_ERROR(msg)            \
  do {                              \
    ctx->state.error = strdup(msg); \
    return C4_ERROR;                \
  } while (0)

#ifdef TEST
char* c4_req_mockname(data_request_t* req);
#endif

#ifdef __cplusplus
}
#endif

#endif
