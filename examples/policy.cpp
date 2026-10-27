// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
#include <algorithm>
#include <slx/solarpilot.h>
/** Editable example: reserve PV surplus for the vehicle before charging the
 * home battery. Never discharge the home battery to charge the EV. Select
 * "Policy pubblica" in the application to compare with the built-in strategy. */
extern "C" void SLX_SP_CALL slx_demo_policy(const slx_sp_policy_input *in,
                                            slx_sp_policy_output *out, void *) {
  out->abi_version = SLX_SP_ABI_VERSION;
  out->struct_size = sizeof(*out);
  double surplus = in->pv_w - in->household_w;
  out->ev_w = in->ev_connected && surplus >= 1380 ? std::min(surplus, in->ev_limit_w) : 0;
  surplus -= out->ev_w;
  out->battery_w = 0;
  if (in->battery_available) {
    if (surplus > 0)
      out->battery_w = -std::min(surplus, in->battery_charge_limit_w);
    else if (in->battery_soc > in->battery_reserve)
      out->battery_w = std::min(-surplus, in->battery_discharge_limit_w);
  }
}
