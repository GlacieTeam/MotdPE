// Copyright © 2025 GlacieTeam. All rights reserved.
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#include "motdpe/MotdPE.hpp"
#include <array>
#include <charconv>
#include <cstddef>
#include <cstring>
#include <expected>
#include <format>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <WS2tcpip.h>
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace motdpe {

namespace detail {

constexpr std::byte operator""_b(unsigned long long value) noexcept { return static_cast<std::byte>(value); }

class MotdException : public std::runtime_error {
public:
    using runtime_error::runtime_error;
};

#ifdef _WIN32
using SocketType                          = SOCKET;
constexpr SocketType INVALID_SOCKET_VALUE = INVALID_SOCKET;
constexpr int        SOCKET_ERROR_VALUE   = SOCKET_ERROR;
#else
using SocketType                          = int;
constexpr SocketType INVALID_SOCKET_VALUE = -1;
constexpr int        SOCKET_ERROR_VALUE   = -1;
#endif

#ifdef _WIN32
class SocketInitializer {
public:
    SocketInitializer() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw MotdException{std::format("WSAStartup failed: {}", WSAGetLastError())};
        }
    }

    ~SocketInitializer() noexcept { WSACleanup(); }

    SocketInitializer(const SocketInitializer&)            = delete;
    SocketInitializer& operator=(const SocketInitializer&) = delete;
};
#endif

class SocketHandle {
public:
    explicit SocketHandle(SocketType sock) noexcept : mSocket(sock) {}

    ~SocketHandle() noexcept { close(); }

    SocketHandle(const SocketHandle&)            = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;

    SocketHandle(SocketHandle&& other) noexcept : mSocket(std::exchange(other.mSocket, INVALID_SOCKET_VALUE)) {}

    SocketHandle& operator=(SocketHandle&& other) noexcept {
        if (this != &other) {
            close();
            mSocket = std::exchange(other.mSocket, INVALID_SOCKET_VALUE);
        }
        return *this;
    }

    operator SocketType() const noexcept { return mSocket; }
    explicit operator bool() const noexcept { return mSocket != INVALID_SOCKET_VALUE; }

    void close() noexcept {
        if (mSocket != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
            closesocket(mSocket);
#else
            ::close(mSocket);
#endif
            mSocket = INVALID_SOCKET_VALUE;
        }
    }

private:
    SocketType mSocket = INVALID_SOCKET_VALUE;
};

class AddrInfoPtr {
public:
    explicit AddrInfoPtr(addrinfo* ai) : mAddrInfo(ai) {}
    ~AddrInfoPtr() {
        if (mAddrInfo) freeaddrinfo(mAddrInfo);
    }

    AddrInfoPtr(const AddrInfoPtr&)            = delete;
    AddrInfoPtr& operator=(const AddrInfoPtr&) = delete;

    AddrInfoPtr(AddrInfoPtr&& other) noexcept : mAddrInfo(std::exchange(other.mAddrInfo, nullptr)) {}

    AddrInfoPtr& operator=(AddrInfoPtr&& other) noexcept {
        if (this != &other) {
            reset();
            mAddrInfo = std::exchange(other.mAddrInfo, nullptr);
        }
        return *this;
    }

    void reset() noexcept {
        if (mAddrInfo) {
            freeaddrinfo(mAddrInfo);
            mAddrInfo = nullptr;
        }
    }

    addrinfo* get() const noexcept { return mAddrInfo; }
    addrinfo* operator->() const noexcept { return mAddrInfo; }
    explicit  operator bool() const noexcept { return mAddrInfo != nullptr; }

private:
    addrinfo* mAddrInfo = nullptr;
};

std::string QueryMotdImpl(std::string_view host, uint16_t port, std::chrono::milliseconds timeout) {
#ifdef _WIN32
    [[maybe_unused]] static const SocketInitializer initializer;
#endif

    static constexpr std::array<std::byte, 33> queryBuf = {0x01_b, 0x00_b, 0x00_b, 0x00_b, 0x00_b, 0xFF_b, 0xFF_b,
                                                           0xC1_b, 0x1D_b, 0x00_b, 0xFF_b, 0xFF_b, 0x00_b, 0xFE_b,
                                                           0xFE_b, 0xFE_b, 0xFE_b, 0xFD_b, 0xFD_b, 0xFD_b, 0xFD_b,
                                                           0x12_b, 0x34_b, 0x56_b, 0x78_b, 0x9C_b, 0x18_b, 0x28_b,
                                                           0x7F_b, 0xE1_b, 0x64_b, 0x89_b, 0x8D_b};

    addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo*         res     = nullptr;
    const std::string portStr = std::to_string(port);

    if (const int status = getaddrinfo(host.data(), portStr.c_str(), &hints, &res); status != 0) {
#ifdef _WIN32
        const wchar_t* errorMsg = gai_strerror(status);
        int            size     = WideCharToMultiByte(CP_UTF8, 0, errorMsg, -1, nullptr, 0, nullptr, nullptr);
        std::string    utf8Msg(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, errorMsg, -1, &utf8Msg[0], size, nullptr, nullptr);
        throw MotdException{std::format("DNS resolution failed: {}", utf8Msg)};
#else
        throw MotdException{std::format("DNS resolution failed: {}", gai_strerror(status))};
#endif
    }

    AddrInfoPtr                 resPtr(res);
    std::array<std::byte, 1024> recvBuf;
    bool                        success = false;
    std::string                 result;

    const auto timeoutMs = timeout.count();

    for (addrinfo* addr = res; addr != nullptr; addr = addr->ai_next) {
        SocketHandle sock{socket(addr->ai_family, SOCK_DGRAM, IPPROTO_UDP)};
        if (!sock) continue;
#ifdef _WIN32
        DWORD winTimeout = static_cast<DWORD>(timeoutMs);
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&winTimeout), sizeof(winTimeout))
            == SOCKET_ERROR_VALUE) {
            continue;
        }
#else
        timeval tv{};
        tv.tv_sec  = static_cast<long>(timeoutMs / 1000);
        tv.tv_usec = static_cast<long>((timeoutMs % 1000) * 1000);

        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == SOCKET_ERROR_VALUE) {
            continue;
        }
#endif
        if (sendto(
                sock,
                reinterpret_cast<const char*>(queryBuf.data()),
                static_cast<int>(queryBuf.size()),
                0,
                addr->ai_addr,
                static_cast<socklen_t>(addr->ai_addrlen)
            )
            == SOCKET_ERROR_VALUE) {
            continue;
        }

        sockaddr_storage fromAddr{};
        socklen_t        fromLen = sizeof(fromAddr);
        const int        recvLen = recvfrom(
            sock,
            reinterpret_cast<char*>(recvBuf.data()),
            static_cast<int>(recvBuf.size()),
            0,
            reinterpret_cast<sockaddr*>(&fromAddr),
            &fromLen
        );

        if (recvLen > 35) {
            result.assign(reinterpret_cast<const char*>(recvBuf.data() + 35), static_cast<size_t>(recvLen) - 35);
            success = true;
            break;
        }
    }

    if (!success) {
        throw MotdException{std::format("All connection attempts failed for {}:{}", host, port)};
    }

    return result;
}

} // namespace detail

std::string queryMotd(std::string_view host, uint16_t port, std::chrono::milliseconds timeout) {
    return detail::QueryMotdImpl(host, port, timeout);
}

std::future<std::string> queryMotdAsync(std::string_view host, uint16_t port, std::chrono::milliseconds timeout) {
    return std::async(std::launch::async, [host = std::string(host), port, timeout] {
        return detail::QueryMotdImpl(host, port, timeout);
    });
}

void queryMotdAsync(
    std::string_view                           host,
    uint16_t                                   port,
    std::function<void(std::string)>           onSuccess,
    std::function<void(const std::exception&)> onError,
    std::chrono::milliseconds                  timeout
) {
    std::thread([host = std::string(host),
                 port,
                 timeout,
                 onSuccess = std::move(onSuccess),
                 onError   = std::move(onError)]() mutable {
        try {
            auto result = detail::QueryMotdImpl(host, port, timeout);
            if (onSuccess) {
                onSuccess(std::move(result));
            }
        } catch (const std::exception& e) {
            if (onError) {
                onError(e);
            }
        } catch (...) {
            if (onError) {
                onError(std::runtime_error("Unknown exception"));
            }
        }
    }).detach();
}

} // namespace motdpe