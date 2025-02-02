#include "../util/json.h"
#include "../util/ssz.h"
#include "../verifier/types_beacon.h"
#include "../verifier/types_verify.h"
#include "beacon.h"
#include "proof_account.h"
#include "ssz_types.h"
#include <inttypes.h> // Include this header for PRIu64 and PRIx64
#include <stdlib.h>
#include <string.h>

static c4_status_t get_eth_tx(proofer_ctx_t* ctx, json_t txhash, json_t* tx_data) {
  char tmp[200];
  snprintf(tmp, sizeof(tmp), "[%.66s]", txhash.start);
  return c4_send_eth_rpc(ctx, "eth_getTransactionByHash", tmp, tx_data);
}

static bytes_t create_eth_data(json_t tx_data) {
  bytes_t data = {0};
  //  buffer_append(&data, json_as_bytes(json_get(tx_data, "input"), &tmp));
  return data;
}

static c4_status_t create_eth_tx_proof(proofer_ctx_t* ctx, json_t tx_data, beacon_block_t* block_data, bytes32_t body_root, bytes_t state_proof) {

  buffer_t      tmp          = {0};
  ssz_builder_t eth_tx_proof = {0};
  //  ssz_builder_t eth_state_proof   = {0};
  ssz_builder_t beacon_header = {0};
  ssz_builder_t c4_req        = {0};
  ssz_ob_t      block         = block_data->header;
  //  eth_account_proof.def           = (ssz_def_t*) &ETH_ACCOUNT_PROOF_CONTAINER;
  //  eth_state_proof.def             = (ssz_def_t*) (eth_account_proof.def->def.container.elements + 7); // TODO:  use the name to identify last element
  beacon_header.def      = (ssz_def_t*) &BEACON_BLOCKHEADER_CONTAINER;
  c4_req.def             = (ssz_def_t*) &C4_REQUEST_CONTAINER;
  uint8_t union_selector = 3; // TODO:  use the name to find the index based on the union definition

  // build the header
  ssz_add_bytes(&beacon_header, "slot", ssz_get(&block, "slot").bytes);
  ssz_add_bytes(&beacon_header, "proposerIndex", ssz_get(&block, "proposerIndex").bytes);
  ssz_add_bytes(&beacon_header, "parentRoot", ssz_get(&block, "parentRoot").bytes);
  ssz_add_bytes(&beacon_header, "stateRoot", ssz_get(&block, "stateRoot").bytes);
  ssz_add_bytes(&beacon_header, "bodyRoot", bytes(body_root, 32));

  buffer_grow(&eth_tx_proof.fixed, 256);
  buffer_append(&eth_tx_proof.fixed, bytes(&union_selector, 1)); // we add the union selector at the beginning
  ssz_add_bytes(&eth_tx_proof, "transaction", json_as_bytes(json_get(tx_data, "input"), &tmp));
  ssz_add_uint32(&eth_tx_proof, (uint32_t) json_as_uint64(json_get(tx_data, "transactionIndex")));
  ssz_add_uint64(&eth_tx_proof, json_as_uint64(json_get(tx_data, "blockNumber")));
  ssz_add_bytes(&eth_tx_proof, "blockHash", json_as_bytes(json_get(tx_data, "blockHash"), &tmp));
  //--------------
  if (union_selector == 1)
    buffer_splice(&tmp, 0, 0, bytes(NULL, 33 - tmp.data.len)); // we add zeros at the beginning so have a fixed length of 32+ selector
  else if (union_selector == 2)
    buffer_splice(&tmp, 0, 0, bytes(NULL, 1)); // make room for one byte
  tmp.data.data[0] = union_selector;           // union selector for bytes32 == index 1

  // build the request
  ssz_add_bytes(&c4_req, "data", tmp.data);
  ssz_add_builders(&c4_req, "proof", &eth_tx_proof);

  // empty sync_data
  union_selector = 0;
  ssz_add_bytes(&c4_req, "sync_data", bytes(&union_selector, 1));

  buffer_free(&tmp);
  ctx->proof = ssz_builder_to_bytes(&c4_req).bytes;
  return C4_SUCCESS;
}

c4_status_t c4_proof_transaction(proofer_ctx_t* ctx) {
  json_t txhash = json_at(ctx->params, 0);

  if (txhash.type != JSON_TYPE_STRING || txhash.len != 66 || txhash.start[1] != '0' || txhash.start[2] != 'x') {
    ctx->error = strdup("Invalid hash");
    return C4_ERROR;
  }

  json_t tx_data;
  TRY_ASYNC(get_eth_tx(ctx, txhash, &tx_data));

  uint32_t tx_index     = (uint32_t) json_as_uint64(json_get(tx_data, "transactionIndex"));
  json_t   block_number = json_get(tx_data, "blockNumber");
  if (block_number.type != JSON_TYPE_STRING || block_number.len < 5 || block_number.start[1] != '0' || block_number.start[2] != 'x') {
    ctx->error = strdup("Invalid block number");
    return C4_ERROR;
  }

  beacon_block_t block = {0};
  TRY_ASYNC(c4_beacon_get_block_for_eth(ctx, block_number, &block));

  bytes32_t body_root;
  ssz_hash_tree_root(block.body, body_root);

  bytes_t state_proof = ssz_create_multi_proof(block.body, 3,
                                               ssz_gindex(block.body.def, 2, "executionPayload", "blockNumber"),
                                               ssz_gindex(block.body.def, 2, "executionPayload", "blockHash"),
                                               ssz_gindex(block.body.def, 3, "executionPayload", "transactions", tx_index)

  );
  /*
  TRY_ASYNC_FINAL(
      create_eth_tx_proof(ctx, eth_proof, &block,
                          body_root, state_proof, address),
      free(state_proof.data));
  */
  return C4_SUCCESS;
}