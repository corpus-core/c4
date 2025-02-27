#include "verify.h"
#include "../util/json.h"
#include "../util/ssz.h"
#include "sync_committee.h"
#include "types_verify.h"
#include <string.h>

void c4_verify_from_bytes(verify_ctx_t* ctx, bytes_t request, char* method, json_t args, chain_id_t chain_id) {
  ssz_ob_t req = ssz_ob(C4_REQUEST_CONTAINER, request);
  memset(ctx, 0, sizeof(verify_ctx_t));
  if (!ssz_is_valid(req, true, &ctx->state)) return;
  ctx->chain_id  = chain_id; // ssz_get_uint64(&req, "chainId");
  ctx->data      = ssz_get(&req, "data");
  ctx->proof     = ssz_get(&req, "proof");
  ctx->sync_data = ssz_get(&req, "sync_data");
  ctx->method    = method;
  ctx->args      = args;
  c4_verify(ctx);
}

void c4_verify(verify_ctx_t* ctx) {
  if (ctx->state.error) return;
  // check if there are sync_datat, we should use to update the state
  if (!c4_update_from_sync_data(ctx)) return;

  if (ssz_is_type(&ctx->proof, BLOCK_HASH_PROOF)) {
    ctx->type = PROOF_TYPE_BEACON_HEADER;
    verify_blockhash_proof(ctx);
  }
  else if (ssz_is_type(&ctx->proof, ETH_TRANSACTION_PROOF))
    verify_tx_proof(ctx);
  else if (ssz_is_type(&ctx->proof, ETH_RECEIPT_PROOF))
    verify_receipt_proof(ctx);
  else if (ssz_is_type(&ctx->proof, &ETH_LOGS_BLOCK_CONTAINER))
    verify_logs_proof(ctx);
  else if (ssz_is_type(&ctx->proof, ETH_ACCOUNT_PROOF))
    verify_account_proof(ctx);
  else if (ctx->proof.def->type == SSZ_TYPE_NONE && ctx->sync_data.def->type != SSZ_TYPE_NONE && ctx->data.def->type == SSZ_TYPE_NONE) {
    ctx->success = true;
  }
  else {
    ctx->state.error = strdup("proof is not a supported proof type");
    ctx->success     = false;
  }
}