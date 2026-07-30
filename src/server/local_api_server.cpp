// SPDX-License-Identifier: GPL-2.0-or-later

#include "open_live_bridge/server/local_api_server.h"

#include "open_live_bridge/util/logger.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#error "LocalApiServer currently supports Windows only."
#endif

namespace olb {

namespace {

std::string Trim(std::string value)
{
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ' || value.back() == '\t')) {
        value.pop_back();
    }

    std::size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t')) {
        ++start;
    }

    if (start > 0) {
        value.erase(0, start);
    }

    return value;
}

std::string ToLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string ReasonPhrase(int statusCode)
{
    switch (statusCode) {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 404:
        return "Not Found";
    case 500:
        return "Internal Server Error";
    default:
        return "OK";
    }
}

bool SendAll(SOCKET socketHandle, const std::string& data)
{
    const char* cursor = data.data();
    int remaining = static_cast<int>(data.size());

    while (remaining > 0) {
        const int sent = send(socketHandle, cursor, remaining, 0);
        if (sent == SOCKET_ERROR) {
            return false;
        }

        cursor += sent;
        remaining -= sent;
    }

    return true;
}

std::size_t FindContentLength(const std::string& headerBlock)
{
    std::istringstream stream(headerBlock);
    std::string line;

    while (std::getline(stream, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        const auto key = ToLower(Trim(line.substr(0, colon)));
        if (key != "content-length") {
            continue;
        }

        const auto value = Trim(line.substr(colon + 1));
        try {
            return static_cast<std::size_t>(std::stoul(value));
        } catch (const std::exception&) {
            return 0;
        }
    }

    return 0;
}

bool ReadRequest(SOCKET client, HttpRequest* request)
{
    std::string buffer;
    std::vector<char> chunk(4096);

    while (true) {
        const int received = recv(client, chunk.data(), static_cast<int>(chunk.size()), 0);
        if (received <= 0) {
            return false;
        }

        buffer.append(chunk.data(), static_cast<std::size_t>(received));

        const auto headerEnd = buffer.find("\r\n\r\n");
        if (headerEnd == std::string::npos) {
            continue;
        }

        const auto headerBlock = buffer.substr(0, headerEnd);
        const auto contentLength = FindContentLength(headerBlock);
        const auto expectedSize = headerEnd + 4 + contentLength;
        if (buffer.size() < expectedSize) {
            continue;
        }

        std::istringstream stream(headerBlock);
        std::string requestLine;
        if (!std::getline(stream, requestLine)) {
            return false;
        }

        std::istringstream requestLineStream(requestLine);
        requestLineStream >> request->method >> request->path;
        if (request->method.empty() || request->path.empty()) {
            return false;
        }

        const auto queryPos = request->path.find('?');
        if (queryPos != std::string::npos) {
            request->path = request->path.substr(0, queryPos);
        }

        std::string line;
        while (std::getline(stream, line)) {
            const auto colon = line.find(':');
            if (colon == std::string::npos) {
                continue;
            }

            auto key = ToLower(Trim(line.substr(0, colon)));
            auto value = Trim(line.substr(colon + 1));
            request->headers[std::move(key)] = std::move(value);
        }

        request->body = buffer.substr(headerEnd + 4, contentLength);
        return true;
    }
}

std::string BuildResponse(const HttpResponse& response)
{
    std::ostringstream out;
    out << "HTTP/1.1 " << response.statusCode << " " << ReasonPhrase(response.statusCode) << "\r\n";
    out << "Content-Type: " << response.contentType << "\r\n";
    out << "Content-Length: " << response.body.size() << "\r\n";
    out << "Connection: close\r\n";
    out << "\r\n";
    out << response.body;
    return out.str();
}

} // namespace

LocalApiServer::LocalApiServer(std::string host, std::uint16_t port, Handler handler)
    : host_(std::move(host))
    , port_(port)
    , handler_(std::move(handler))
{
}

bool LocalApiServer::Run(std::string* error)
{
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        if (error) {
            *error = "WSAStartup failed";
        }
        return false;
    }

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        if (error) {
            *error = "failed to create server socket";
        }
        WSACleanup();
        return false;
    }

    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);

    if (InetPtonA(AF_INET, host_.c_str(), &address.sin_addr) != 1) {
        if (error) {
            *error = "invalid listen address: " + host_;
        }
        closesocket(server);
        WSACleanup();
        return false;
    }

    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        if (error) {
            *error = "failed to bind local api server";
        }
        closesocket(server);
        WSACleanup();
        return false;
    }

    if (listen(server, SOMAXCONN) == SOCKET_ERROR) {
        if (error) {
            *error = "failed to listen on local api server socket";
        }
        closesocket(server);
        WSACleanup();
        return false;
    }

    Log(LogLevel::Info, "local api listening on " + host_ + ":" + std::to_string(port_));

    while (!stopRequested_.load()) {
        SOCKET client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            continue;
        }

        HttpRequest request;
        HttpResponse response;

        if (!ReadRequest(client, &request)) {
            response.statusCode = 400;
            response.body = "{\"ok\":false,\"error\":\"invalid http request\"}";
        } else {
            response = handler_(request);
        }

        const auto wireResponse = BuildResponse(response);
        SendAll(client, wireResponse);
        closesocket(client);
    }

    closesocket(server);
    WSACleanup();
    return true;
}

void LocalApiServer::Stop()
{
    stopRequested_.store(true);
}

} // namespace olb
