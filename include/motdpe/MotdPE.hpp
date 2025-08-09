// Copyright © 2025 GlacieTeam. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#pragma once
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <vector>

namespace motdpe {

std::string
queryMotd(std::string_view host, uint16_t port, std::chrono::milliseconds timeout = std::chrono::seconds(5));

std::future<std::string>
queryMotdAsync(std::string_view host, uint16_t port, std::chrono::milliseconds timeout = std::chrono::seconds(5));

void queryMotdAsync(
    std::string_view                           host,
    uint16_t                                   port,
    std::function<void(std::string)>           onSuccess,
    std::function<void(const std::exception&)> onError,
    std::chrono::milliseconds                  timeout = std::chrono::seconds(5)
);

} // namespace motdpe