// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace olb {

struct HttpRequest {
    std::string method;
    std::string path;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int statusCode = 200;
    std::string contentType = "application/json";
    std::string body;
};

class LocalApiServer {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;

    LocalApiServer(std::string host, std::uint16_t port, Handler handler);

    bool Run(std::string* error);
    void Stop();

private:
    std::string host_;
    std::uint16_t port_;
    Handler handler_;
    std::atomic_bool stopRequested_{false};
};

} // namespace olb

