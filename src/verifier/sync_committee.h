#ifndef sync_committee_h__
#define sync_committee_h__

#ifdef __cplusplus
extern "C" {
#endif

#include "../util/bytes.h"
#include "verify.h"
#include <stdint.h>

typedef struct {
  uint64_t last_period;
  uint64_t current_period;
  bytes_t  validators;
} c4_sync_state_t;

const c4_sync_state_t c4_get_validators(uint64_t period);
bool                  c4_update_from_sync_data(verify_ctx_t* ctx);
#ifdef __cplusplus
}
#endif

#endif