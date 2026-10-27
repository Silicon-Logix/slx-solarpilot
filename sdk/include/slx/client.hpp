// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
#pragma once
#include "solarpilot.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <vector>
namespace slx {
using json = nlohmann::json;
/** Public convenience wrapper; the binary boundary remains the documented C ABI. */
class Client {
  slx_sp_context *context_ = slx_sp_create(SLX_SP_ABI_VERSION);
  std::vector<char> buffer_ = std::vector<char>(SLX_SP_RESPONSE_CAPACITY);

public:
  Client() {
    if (!context_)
      throw std::runtime_error("SLX core ABI 1 unavailable");
  }
  ~Client() {
    slx_sp_destroy(context_);
  }
  Client(const Client &) = delete;
  Client &operator=(const Client &) = delete;
  json call(const json &request) {
    const auto text = request.dump();
    const auto result =
        slx_sp_dispatch(context_, text.data(), text.size(), buffer_.data(), buffer_.size());
    const auto response = json::parse(buffer_.data());
    if (result != SLX_SP_OK)
      throw std::runtime_error(response.value("error", "SLX dispatch failed"));
    return response;
  }
  void policy(slx_sp_policy_fn fn, void *data = nullptr) {
    slx_sp_set_policy(context_, fn, data);
  }
};
} // namespace slx
