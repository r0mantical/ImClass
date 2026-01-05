#pragma once

// Force WinSock2 FIRST
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <thread>
#include <mutex>
#include <queue>
#include <string>
#include <memory>
#include <Windows.h>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <atomic>
#include "logging.h"
#include <nlohmann/json.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using json = nlohmann::json;

class WebSocketServer {
private:
    struct PendingRequest {
        std::string id;
        std::string type;
        std::chrono::steady_clock::time_point timestamp;
        std::function<void(const std::string&)> callback;
    };

    net::io_context ioc;
    tcp::acceptor acceptor;
    std::thread server_thread;
    std::mutex queue_mutex;
    std::mutex conn_mutex;
    std::mutex request_mutex;

    std::queue<std::string> incoming_messages;
    std::shared_ptr<websocket::stream<tcp::socket>> ws_stream;
    std::unordered_map<std::string, PendingRequest> pending_requests;
    std::atomic<uint64_t> request_counter{ 0 };

    bool has_connection = false;
    bool running = false;

    void do_accept() {
        acceptor.async_accept([this](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                logger::addLog("[WS] Client connected");

                ws_stream = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket));

                ws_stream->async_accept([this](beast::error_code ec) {
                    if (!ec) {
                        std::lock_guard<std::mutex> lock(conn_mutex);
                        has_connection = true;
                        do_read();
                    }
                    });
            }

            if (running) {
                do_accept();
            }
            });
    }

    void do_read() {
        auto buffer = std::make_shared<beast::flat_buffer>();

        ws_stream->async_read(*buffer, [this, buffer](beast::error_code ec, std::size_t bytes) {
            if (ec) {
                logger::addLog("[WS] Client disconnected");
                std::lock_guard<std::mutex> lock(conn_mutex);
                has_connection = false;
                return;
            }

            std::string message = beast::buffers_to_string(buffer->data());

            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                incoming_messages.push(message);
                // REMOVE THIS LOG
                // logger::addLog("[WS] Received: " + message);
            }

            // Try to process as response
            process_response(message);

            if (running) {
                do_read();
            }
            });
    }

    void process_response(const std::string& response_json) {
        try {
            auto j = json::parse(response_json);

            if (!j.contains("request_id")) {
                // REMOVE THIS LOG
                // logger::addLog("[WS] Received message without request_id (not a response)");
                return;
            }

            std::string request_id = j["request_id"];

            std::lock_guard<std::mutex> lock(request_mutex);
            auto it = pending_requests.find(request_id);

            if (it != pending_requests.end()) {
                // REMOVE THIS LOG
                // logger::addLog("[WS] Processing response for request: " + it->second.type + " (ID: " + request_id + ")");
                it->second.callback(response_json);
                pending_requests.erase(it);
            }
            else {
                logger::addLog("[WS] Received response for unknown request ID: " + request_id);
            }
        }
        catch (const std::exception& e) {
            logger::addLog("[WS] Error processing response: " + std::string(e.what()));
        }
    }

public:
    WebSocketServer()
        : acceptor(ioc, tcp::endpoint(tcp::v4(), 9001))
    {
        acceptor.set_option(net::socket_base::reuse_address(true));
    }

    void start() {
        logger::addLog("[WS] Starting WebSocket server on port 9001");

        running = true;
        do_accept();

        server_thread = std::thread([this]() {
            try {
                ioc.run();
                logger::addLog("[WS] Server started successfully");
            }
            catch (const std::exception& e) {
                logger::addLog("[WS] Server error: " + std::string(e.what()));
            }
            });
    }

    void stop() {
        if (running) {
            logger::addLog("[WS] Stopping WebSocket server");
            running = false;

            if (ws_stream) {
                beast::error_code ec;
                ws_stream->close(websocket::close_code::normal, ec);
            }

            ioc.stop();

            if (server_thread.joinable()) {
                server_thread.join();
            }

            logger::addLog("[WS] Server stopped");
        }
    }

    bool is_connected() {
        std::lock_guard<std::mutex> lock(conn_mutex);
        return has_connection;
    }

    void send(const std::string& message) {
        if (ws_stream && has_connection) {
            try {
                ws_stream->write(net::buffer(message));
                // REMOVE THIS LOG
                // logger::addLog("[WS] Sent message (" + std::to_string(message.size()) + " bytes)");
            }
            catch (const std::exception& e) {
                logger::addLog("[WS] Send error: " + std::string(e.what()));
            }
        }
        else {
            logger::addLog("[WS] Cannot send - not connected");
        }
    }

    std::string send_request(const std::string& type, const json& data,
        std::function<void(const std::string&)> callback) {

        std::string request_id = std::to_string(++request_counter);

        json msg = data;
        msg["type"] = type;
        msg["request_id"] = request_id;

        {
            std::lock_guard<std::mutex> lock(request_mutex);
            pending_requests[request_id] = {
                request_id,
                type,
                std::chrono::steady_clock::now(),
                callback
            };
        }

        send(msg.dump());
        // REMOVE THIS LOG (except for important requests like ref_process)
        // logger::addLog("[WS] Sent request: " + type + " (ID: " + request_id + ")");
        return request_id;
    }

    void cleanup_stale_requests() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(request_mutex);

        for (auto it = pending_requests.begin(); it != pending_requests.end();) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.timestamp);
            if (age.count() > 5) {
                logger::addLog("[WS] Request timeout: " + it->second.type + " (ID: " + it->first + ")");
                it = pending_requests.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    bool has_message() {
        std::lock_guard<std::mutex> lock(queue_mutex);
        return !incoming_messages.empty();
    }

    std::string receive() {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (incoming_messages.empty()) return "";

        std::string msg = incoming_messages.front();
        incoming_messages.pop();
        return msg;
    }

    ~WebSocketServer() {
        stop();
    }
};

inline WebSocketServer g_WebSocketServer;