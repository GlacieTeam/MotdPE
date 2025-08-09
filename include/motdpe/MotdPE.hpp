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