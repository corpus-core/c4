#ifndef __C4_CURL_H__
#define __C4_CURL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "../../src/proofer/proofer.h"

void curl_fetch(data_request_t* req);
void curl_set_config(json_t config);

#ifdef __cplusplus
}
#endif

#endif