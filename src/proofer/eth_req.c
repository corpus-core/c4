#include "eth_req.h"
#include "../util/json.h"
#include "../util/logger.h"
#include "../util/rlp.h"
#include "../util/ssz.h"
#include "../verifier/types_beacon.h"
#include "../verifier/types_verify.h"
#include "beacon.h"
#include "ssz_types.h"
#include <inttypes.h> // Include this header for PRIu64 and PRIx64
#include <stdlib.h>
#include <string.h>

#define JSON_TX_FIELDS       "{transactionIndex:hexuint,blockNumber:hexuint,hash:bytes32,blockHash:bytes32,from:address,gas:hexuint,gasPrice:hexuint,input:bytes,nonce:hexuint,to:address,value:hexuint,type:hexuint,v:hexuint,r:bytes32,s:bytes32}"
#define JSON_LOG_FIELDS      "{address:address,topics:[bytes32],data:bytes,blockNumber:hexuint,transactionHash:bytes32,transactionIndex:hexuint,blockHash:bytes32,logIndex:hexuint,removed:bool}"
#define JSON_RECEIPTS_FIELDS "{type:hexuint,status:hexuint,cumulativeGasUsed:hexuint,logs:[" JSON_LOG_FIELDS "],logsBloom:bytes,transactionHash:bytes32,transactionIndex:hexuint,blockHash:bytes32,gasUsed:hexuint,effectiveGasPrice:hexuint,from:address,to?:address,contractAddress?:address}"

c4_status_t get_eth_tx(proofer_ctx_t* ctx, json_t txhash, json_t* tx_data) {
  uint8_t  tmp[200];
  buffer_t buf = stack_buffer(tmp);
  TRY_ASYNC(c4_send_eth_rpc(ctx, "eth_getTransactionByHash", bprintf(&buf, "[%J]", txhash), tx_data));
  CHECK_JSON(*tx_data, JSON_TX_FIELDS, "Invalid results for Tx: ");
  return C4_SUCCESS;
}

c4_status_t eth_getBlockReceipts(proofer_ctx_t* ctx, json_t block, json_t* receipts_array) {
  uint8_t  tmp[200];
  buffer_t buf = stack_buffer(tmp);
  TRY_ASYNC(c4_send_eth_rpc(ctx, "eth_getBlockReceipts", bprintf(&buf, "[%J]", block), receipts_array));
  CHECK_JSON(*receipts_array, "[" JSON_RECEIPTS_FIELDS "]", "Invalid results for Block Receipts: ");
  return C4_SUCCESS;
}

c4_status_t eth_get_logs(proofer_ctx_t* ctx, json_t params, json_t* logs) {
  uint8_t  tmp[1000];
  buffer_t buf = stack_buffer(tmp);
  TRY_ASYNC(c4_send_eth_rpc(ctx, "eth_getLogs", json_as_string(params, &buf), logs));
  CHECK_JSON(*logs, "[" JSON_LOG_FIELDS "]", "Invalid results for Logs: ");
  return C4_SUCCESS;
}

bytes_t c4_serialize_receipt(json_t r, buffer_t* buf) {
  uint8_t  tmp[300]   = {0};
  buffer_t tmp_buf    = stack_buffer(tmp);
  uint8_t  tmp2[32]   = {0};
  buffer_t short_buf  = stack_buffer(tmp2);
  buffer_t topics_buf = {0};
  buffer_t logs_buf   = {0};
  buffer_t log_buf    = {0};
  uint8_t  type       = json_get_uint8(r, "type");
  uint8_t  status     = json_get_uint8(r, "status");
  bytes_t  state_root = json_get_bytes(r, "stateRoot", &tmp_buf);
  buffer_reset(buf);

  if (state_root.len == 32)
    rlp_add_item(buf, state_root);
  else
    rlp_add_uint64(buf, (uint64_t) status);
  rlp_add_uint64(buf, json_get_uint64(r, "cumulativeGasUsed"));
  rlp_add_item(buf, json_get_bytes(r, "logsBloom", &tmp_buf));

  // encode logs
  json_for_each_value(json_get(r, "logs"), log) {
    buffer_reset(&log_buf);
    rlp_add_item(&log_buf, json_get_bytes(log, "address", &tmp_buf));

    buffer_reset(&topics_buf);
    json_for_each_value(json_get(log, "topics"), topic)
        rlp_add_item(&topics_buf, json_as_bytes(topic, &short_buf));

    rlp_add_list(&log_buf, topics_buf.data);
    rlp_add_item(&log_buf, json_get_bytes(log, "data", &topics_buf));
    rlp_add_list(&logs_buf, log_buf.data);
  }
  rlp_add_list(buf, logs_buf.data);
  rlp_to_list(buf);
  if (type) buffer_splice(buf, 0, 0, bytes(&type, 1));
  buffer_free(&logs_buf);
  buffer_free(&log_buf);
  buffer_free(&topics_buf);
  return buf->data;
}

// sends a request to the eth rpc and returns the result or returns with status C4_PENDING
c4_status_t c4_send_eth_rpc(proofer_ctx_t* ctx, char* method, char* params, json_t* result) {
  bytes32_t id     = {0};
  buffer_t  buffer = {0};
  bprintf(&buffer, "{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s,\"id\":1}", method, params);
  sha256(buffer.data, id);
  data_request_t* data_request = c4_state_get_data_request_by_id(&ctx->state, id);
  if (data_request) {
    buffer_free(&buffer);
    if (c4_state_is_pending(data_request)) return C4_PENDING;
    if (!data_request->error && data_request->response.data) {
      json_t response = json_parse((char*) data_request->response.data);
      if (response.type != JSON_TYPE_OBJECT) {
        ctx->state.error = strdup("Invalid JSON response");
        return C4_ERROR;
      }

      json_t error = json_get(response, "error");
      if (error.type == JSON_TYPE_OBJECT) {
        json_t code = json_get(error, "code");
        if (code.len == 6 && strncmp(code.start, "-32602", 6) == 0)
          RETRY_REQUEST(data_request);
        else
          THROW_ERROR("Error when calling eth-rpc for %s (params: %s) : %j", method, params, json_get(error, "message"));
      }
      else if (error.type == JSON_TYPE_STRING)
        THROW_ERROR("Error when calling eth-rpc for %s (params: %s) : %j", method, params, error);

      json_t res = json_get(response, "result");
      if (res.type == JSON_TYPE_NOT_FOUND || res.type == JSON_TYPE_INVALID) THROW_ERROR("Error when calling eth-rpc for %s (params: %s): Invalid JSON response (no result)", method, params);

      *result = res;
      return C4_SUCCESS;
    }
    else
      THROW_ERROR(data_request->error ? data_request->error : "Data request failed");
  }
  else {
    data_request = calloc(1, sizeof(data_request_t));
    memcpy(data_request->id, id, 32);
    data_request->payload  = buffer.data;
    data_request->encoding = C4_DATA_ENCODING_JSON;
    data_request->method   = C4_DATA_METHOD_POST;
    data_request->type     = C4_DATA_TYPE_ETH_RPC;
    c4_state_add_request(&ctx->state, data_request);
    return C4_PENDING;
  }

  return C4_SUCCESS;
}
