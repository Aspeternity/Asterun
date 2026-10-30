#include <winsock2.h>
#include <ws2tcpip.h>
#include "platform/EverythingHttpRequest.hpp"
#include <cassert>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <thread>

using namespace std::chrono_literals;
namespace {
struct Internet {
    HINTERNET value{};
    ~Internet() { if (value) WinHttpCloseHandle(value); }
};
// Local HTTP is sufficient to exercise actual WinHTTP callback/close behavior.
// Production still requires HTTPS and retains normal certificate verification.
struct Peer {
    SOCKET listener{INVALID_SOCKET};
    unsigned short port{};
    std::promise<void> received;
    std::jthread thread;
    explicit Peer(int mode) {
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        assert(listener != INVALID_SOCKET);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        assert(bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
        int size = sizeof(address);
        assert(getsockname(listener, reinterpret_cast<sockaddr*>(&address), &size) == 0);
        port = ntohs(address.sin_port);
        assert(listen(listener, 1) == 0);
        thread = std::jthread([this, mode](std::stop_token stop) {
            while (!stop.stop_requested()) {
                fd_set readable; FD_ZERO(&readable); FD_SET(listener, &readable);
                timeval timeout{0, 20000};
                if (select(0, &readable, nullptr, nullptr, &timeout) > 0) break;
            }
            if (stop.stop_requested()) return;
            SOCKET client = accept(listener, nullptr, nullptr);
            assert(client != INVALID_SOCKET);
            DWORD timeout = 1000;
            setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
                reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            std::string request;
            char buffer[1024];
            while (request.find("\r\n\r\n") == std::string::npos) {
                const int bytes = recv(client, buffer, sizeof(buffer), 0);
                if (bytes <= 0) break;
                request.append(buffer, bytes);
            }
            assert(request.find("\r\n\r\n") != std::string::npos);
            received.set_value();
            if (mode != 1) {
                const std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nConnection: close\r\n\r\n" +
                    std::string(mode == 0 ? "data" : "d");
                assert(send(client, response.data(), static_cast<int>(response.size()), 0) == static_cast<int>(response.size()));
            }
            // Deliberately stall headers/body until the client cancels. No
            // public internet, DNS, TLS exceptions, or dependency on weak WiFi.
            std::mutex mutex;
            std::condition_variable_any changed;
            std::unique_lock lock(mutex);
            changed.wait(lock, stop, [] { return false; });
            closesocket(client);
        });
    }
    ~Peer() {
        thread.request_stop(); thread.join(); closesocket(listener);
    }
};
}
int main() {
    WSADATA data{}; assert(WSAStartup(MAKEWORD(2, 2), &data) == 0);
    for (int cycle = 0; cycle < 12; ++cycle) for (int mode : {0, 1, 2}) {
        std::cout << "Loopback cycle " << cycle << ", mode " << mode << std::endl;
        Peer peer(mode);
        auto received = peer.received.get_future();
        std::atomic<int> stage{};
        std::promise<void> waiting, completed;
        auto ready = waiting.get_future(); auto done = completed.get_future();
        auto worker = std::make_unique<std::jthread>([&](std::stop_token token) {
            Internet session{WinHttpOpen(L"Asterun transport regression", WINHTTP_ACCESS_TYPE_NO_PROXY,
                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC)};
            assert(session.value); stage = 1;
            assert(WinHttpSetTimeouts(session.value, 5000, 5000, 10000, 10000));
            Internet connection{WinHttpConnect(session.value, L"127.0.0.1", peer.port, 0)};
            assert(connection.value);
            {
                altrun::win::everything_http::Request request;
                std::uint32_t error{};
                assert(request.Attach(WinHttpOpenRequest(connection.value, L"GET", L"/", nullptr,
                    WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0), error));
                // A silent peer may stall SendRequest itself. Do not require
                // its completion before allowing the test to request stop.
                if (mode == 1) waiting.set_value();
                bool ok = request.Send(token, error); stage = 2;
                if (ok) ok = request.Receive(token, error);
                if (mode != 1) {
                    assert(ok); stage = 3;
                    DWORD available{};
                    if (mode == 2) {
                        // Permit ReceiveResponse's read-ahead, then consume the
                        // prefix before waiting on a genuinely stalled body.
                        assert(request.Available(available, token, error) && available == 1);
                        DWORD read{};
                        assert(request.Read(available, read, token, error) && read == 1);
                        assert(request.Data()[0] == 'd');
                    }
                    waiting.set_value(); stage = 4;
                    ok = request.Available(available, token, error);
                    if (mode == 0) {
                        assert(ok);
                        std::string body;
                        while (available != 0) {
                            DWORD read{};
                            assert(request.Read(available, read, token, error));
                            body.append(request.Data(), read);
                            assert(request.Available(available, token, error));
                        }
                        assert(body == "data");
                    }
                }
                if (mode != 0) assert(!ok && error == ERROR_CANCELLED);
                request.Close(); request.Close(); // destruction is also idempotent
            }
            completed.set_value();
        });
        const auto readyStatus = ready.wait_for(10s);
        if (readyStatus != std::future_status::ready)
            std::cerr << "Request did not reach cancellation phase; stage=" << stage.load() << std::endl;
        assert(readyStatus == std::future_status::ready);
        if (mode == 0) {
            assert(done.wait_for(3s) == std::future_status::ready);
            worker->request_stop(); // no live request/callback after completion
        } else {
            // Prove that real network I/O reached the peer before cancellation.
            assert(received.wait_for(3s) == std::future_status::ready);
            assert(done.wait_for(40ms) == std::future_status::timeout);
        }
        const auto start = std::chrono::steady_clock::now();
        worker.reset(); // real exit pattern: request_stop + join
        assert(std::chrono::steady_clock::now() - start < 3s);
    }
    WSACleanup();
    std::cout << "36 real loopback WinHTTP requests: success, stalled headers/body cancellation and exit passed\n";
}
