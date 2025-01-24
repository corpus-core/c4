#include "proofer.h"
#include "../util/json.h"
#include "proof_account.h"
#include <stdlib.h>
#include <string.h>

proofer_ctx_t* c4_proofer_create(char* method, char* params) {
  json_t params_json = json_parse(params);
  if (params_json.type != JSON_TYPE_ARRAY) return NULL;
  char* params_str = malloc(params_json.len + 1);
  memcpy(params_str, params_json.start, params_json.len);
  params_str[params_json.len] = 0;
  params_json.start           = params_str;
  proofer_ctx_t* ctx          = calloc(1, sizeof(proofer_ctx_t));
  ctx->method                 = strdup(method);
  ctx->params                 = params_json;
  return ctx;
}

void c4_proofer_free(proofer_ctx_t* ctx) {
  data_request_t* data_request = ctx->data_requests;
  while (data_request) {
    data_request_t* next = data_request->next;
    if (data_request->url) free(data_request->url);
    if (data_request->error) free(data_request->error);
    if (data_request->payload.data) free(data_request->payload.data);
    if (data_request->response.data) free(data_request->response.data);
    free(data_request);
    data_request = next;
  }

  if (ctx->method) free(ctx->method);
  if (ctx->params.start) free(ctx->params.start);
  if (ctx->error) free(ctx->error);
  if (ctx->proof.data) free(ctx->proof.data);
  free(ctx);
}

c4_proofer_status_t c4_proofer_execute(proofer_ctx_t* ctx) {
  if (c4_proofer_get_pending_data_request(ctx)) return C4_PROOFER_WAITING;
  if (ctx->error) return C4_PROOFER_ERROR;

  if (strcmp(ctx->method, "eth_getBalance") == 0)
    c4_proof_account(ctx);
  else
    ctx->error = strdup("Unsupported method");

  return c4_proofer_status(ctx);
}

data_request_t* c4_proofer_get_pending_data_request(proofer_ctx_t* ctx) {
  data_request_t* data_request = ctx->data_requests;
  while (data_request) {
    if (data_request->response.data == NULL && data_request->error == NULL) return data_request;
    data_request = data_request->next;
  }
  return NULL;
}

data_request_t* c4_proofer_get_data_request_by_id(proofer_ctx_t* ctx, bytes32_t id) {
  data_request_t* data_request = ctx->data_requests;
  while (data_request) {
    if (memcmp(data_request->id, id, 32) == 0) return data_request;
    data_request = data_request->next;
  }
  return NULL;
}

void c4_proofer_add_data_request(proofer_ctx_t* ctx, data_request_t* data_request) {
  data_request->next = ctx->data_requests;
  ctx->data_requests = data_request;
}

c4_proofer_status_t c4_proofer_status(proofer_ctx_t* ctx) {
  if (ctx->error) return C4_PROOFER_ERROR;
  if (ctx->proof.data) return C4_PROOFER_SUCCESS;
  if (c4_proofer_get_pending_data_request(ctx)) return C4_PROOFER_WAITING;
  return C4_PROOFER_PENDING;
}

c4_status_t c4u_send_eth_rpc(proofer_ctx_t* ctx, char* method, char* params, json_t* result) {
  bytes32_t id     = {0};
  buffer_t  buffer = {0};
  buffer_add_chars(&buffer, "{\"jsonrpc\":\"2.0\",\"method\":\"");
  buffer_add_chars(&buffer, method);
  buffer_add_chars(&buffer, "\",\"params\":");
  buffer_add_chars(&buffer, params);
  buffer_add_chars(&buffer, ",\"id\":1}");
  sha256(buffer.data, id);
  data_request_t* data_request = c4_proofer_get_data_request_by_id(ctx, id);
  if (data_request) {
    buffer_free(&buffer);
    if (!data_request->error && data_request->response.data) {
      json_t response = json_parse((char*) data_request->response.data);
      if (response.type != JSON_TYPE_OBJECT) {
        ctx->error = strdup("Invalid JSON response");
        return C4_ERROR;
      }

      json_t error = json_get(response, "error");
      if (error.type == JSON_TYPE_OBJECT) {
        error      = json_get(error, "message");
        ctx->error = strndup(error.start, error.len);
        return C4_ERROR;
      }
      else if (error.type == JSON_TYPE_STRING) {
        ctx->error = strndup(error.start, error.len);
        return C4_ERROR;
      }

      json_t res = json_get(response, "result");
      if (res.type == JSON_TYPE_NOT_FOUND || res.type == JSON_TYPE_INVALID) {
        ctx->error = strdup("Invalid JSON response");
        return C4_ERROR;
      }

      *result = res;
      return C4_SUCCESS;
    }
    else
      ctx->error = strdup(data_request->error ? data_request->error : "Data request already exists");
  }
  else {
    data_request = calloc(1, sizeof(data_request_t));
    memcpy(data_request->id, id, 32);
    data_request->payload  = buffer.data;
    data_request->encoding = C4_DATA_ENCODING_JSON;
    data_request->method   = C4_DATA_METHOD_POST;
    data_request->type     = C4_DATA_TYPE_ETH_RPC;
    c4_proofer_add_data_request(ctx, data_request);
    return C4_PENDING;
  }

  return C4_SUCCESS;
}
