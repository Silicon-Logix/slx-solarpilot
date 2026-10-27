// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
/* Silicon LogiX / SLX SolarPilot
 * Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
 * License: Silicon LogiX Evaluation License 1.0. See LICENSE.
 * Public C ABI 1. No STL objects or allocator ownership cross this boundary.
 */
#ifndef SLX_SOLARPILOT_H
#define SLX_SOLARPILOT_H
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32)
#if defined(SLX_SP_BUILD_CORE)
#define SLX_SP_API __declspec(dllexport)
#else
#define SLX_SP_API __declspec(dllimport)
#endif
#define SLX_SP_CALL __cdecl
#else
#define SLX_SP_API __attribute__((visibility("default")))
#define SLX_SP_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define SLX_SP_ABI_VERSION 1u
#define SLX_SP_RESPONSE_CAPACITY (2u * 1024u * 1024u)
typedef struct slx_sp_context slx_sp_context;
typedef enum slx_sp_result {
  SLX_SP_OK = 0,
  SLX_SP_INVALID = 1,
  SLX_SP_BUFFER = 2,
  SLX_SP_ERROR = 3
} slx_sp_result;
/** All powers are AC watts. Battery positive = discharge; negative = charge.
 * Policy callbacks run on the calling thread in EEBUS mode or on the EMS
 * worker in accelerated laboratory mode. They must not throw, call dispatch
 * recursively, block, or retain the supplied pointers. */
typedef struct slx_sp_policy_input {
  uint32_t abi_version, struct_size;
  double pv_w, household_w, battery_soc, battery_reserve;
  double battery_charge_limit_w, battery_discharge_limit_w;
  double grid_limit_w, ev_limit_w;
  uint32_t ev_connected, battery_available;
} slx_sp_policy_input;
typedef struct slx_sp_policy_output {
  uint32_t abi_version, struct_size;
  double battery_w, ev_w;
} slx_sp_policy_output;
typedef void(SLX_SP_CALL *slx_sp_policy_fn)(const slx_sp_policy_input *, slx_sp_policy_output *,
                                            void *);
SLX_SP_API uint32_t SLX_SP_CALL slx_sp_abi_version(void);
/** Returns NULL on ABI mismatch or allocation failure. Destroy with this DLL. */
SLX_SP_API slx_sp_context *SLX_SP_CALL slx_sp_create(uint32_t abi_version);
SLX_SP_API void SLX_SP_CALL slx_sp_destroy(slx_sp_context *context);
/** Caller serializes access to a context. Request is UTF-8 JSON, at most 64 KiB.
 * Response is NUL-terminated UTF-8 JSON in caller-owned storage. Capacity must
 * be >= SLX_SP_RESPONSE_CAPACITY; smaller buffers fail BEFORE any side effect.
 * See docs/API.md for operations, units and compatibility rules. */
SLX_SP_API slx_sp_result SLX_SP_CALL slx_sp_dispatch(slx_sp_context *, const char *request,
                                                     size_t request_size, char *response,
                                                     size_t response_capacity);
/** NULL restores built-in policy. User data remains caller-owned until destroy
 * or the next registration. Callback output always passes physical limits. */
SLX_SP_API slx_sp_result SLX_SP_CALL slx_sp_set_policy(slx_sp_context *, slx_sp_policy_fn, void *);
#ifdef __cplusplus
}
#endif
#endif
