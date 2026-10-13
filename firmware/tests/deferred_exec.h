#pragma once
#include "qmk_mock.h"
typedef uint8_t deferred_token;
#define INVALID_DEFERRED_TOKEN 0
typedef uint32_t (*deferred_exec_callback)(uint32_t, void *);
deferred_token defer_exec(uint32_t delay, deferred_exec_callback cb, void *arg);
bool cancel_deferred_exec(deferred_token token);
