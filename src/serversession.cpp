/*
 * This file is part of the trojan project.
 * Trojan is an unidentifiable mechanism that helps you bypass GFW.
 * Copyright (C) 2017  GreaterFire
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "serversession.h"
#include <string>
#include <memory>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "trojanrequest.h"
#include "log.h"
using namespace std;
using namespace boost::asio::ip;
using namespace boost::asio::ssl;

ServerSession::ServerSession(const Config &config, boost::asio::io_service &io_service, context &ssl_context) :
    Session(config, io_service),
    in_socket(io_service, ssl_context),
    out_socket(io_service),
    status(HANDSHAKE) {}

boost::asio::basic_socket<tcp, boost::asio::stream_socket_service<tcp> >& ServerSession::accept_socket() {
    return in_socket.lowest_layer();
}

void ServerSession::start() {
    in_endpoint = in_socket.lowest_layer().remote_endpoint();
    auto self = shared_from_this();
    in_socket.async_handshake(stream_base::server, [this, self](const boost::system::error_code error) {
        if (!error) {
            in_async_read();
        } else {
            Log::log_with_endpoint(in_endpoint, "SSL handshake failed", Log::ERROR);
            destroy();
        }
    });
}

void ServerSession::in_async_read() {
    auto self = shared_from_this();
    in_socket.async_read_some(boost::asio::buffer(in_read_buf, MAX_LENGTH), [this, self](const boost::system::error_code error, size_t length) {
        if (!error) {
            in_recv(string((const char*)in_read_buf, length));
        } else {
            destroy();
        }
    });
}

void ServerSession::in_async_write(const string &data) {
    auto self = shared_from_this();
    boost::asio::async_write(in_socket, boost::asio::buffer(data), [this, self](const boost::system::error_code error, size_t) {
        if (!error) {
            in_sent();
        } else {
            destroy();
        }
    });
}

void ServerSession::in_recv(const string &data) {
    switch (status) {
        case HANDSHAKE: {
            auto self = shared_from_this();
            size_t first = data.find("\r\n");
            if (first != string::npos) {
                if (config.password == data.substr(0, first)) {
                    size_t second = data.find("\r\n", first + 2);
                    if (second != string::npos) {
                        string req_str = data.substr(first + 2, second - first - 2);
                        TrojanRequest req;
                        if (!req.parse(req_str)) {
                            Log::log_with_endpoint(in_endpoint, "bad request", Log::ERROR);
                            destroy();
                            return;
                        };
                        Log::log_with_endpoint(in_endpoint, "requested connection to " + req.address + ':' + to_string(req.port), Log::INFO);
                        status = CONNECTING_REMOTE;
                        out_write_buf = data.substr(second + 2);
                        tcp::resolver::query query(req.address, to_string(req.port));
                        resolver.async_resolve(query, [this, self](const boost::system::error_code error, tcp::resolver::iterator iterator) {
                            if (!error) {
                                out_socket.async_connect(*iterator, [this, self](const boost::system::error_code error) {
                                    if (!error) {
                                        Log::log_with_endpoint(in_endpoint, "tunnel established");
                                        status = FORWARDING;
                                        out_async_read();
                                        if (out_write_buf != "") {
                                            out_async_write(out_write_buf);
                                        } else {
                                            in_async_read();
                                        }
                                    } else {
                                        Log::log_with_endpoint(in_endpoint, "cannot establish connection to remote server", Log::ERROR);
                                        destroy();
                                    }
                                });
                            } else {
                                Log::log_with_endpoint(in_endpoint, "cannot resolve remote server hostname", Log::ERROR);
                                destroy();
                            }
                        });
                        return;
                    }
                }
            }
            Log::log_with_endpoint(in_endpoint, "not trojan request, connecting to " + config.remote_addr + ':' + to_string(config.remote_port), Log::WARN);
            status = CONNECTING_REMOTE;
            out_write_buf = data;
            tcp::resolver::query query(config.remote_addr, to_string(config.remote_port));
            resolver.async_resolve(query, [this, self](const boost::system::error_code error, tcp::resolver::iterator iterator) {
                if (!error) {
                    out_socket.async_connect(*iterator, [this, self](const boost::system::error_code error) {
                        if (!error) {
                            Log::log_with_endpoint(in_endpoint, "tunnel established");
                            status = FORWARDING;
                            out_async_read();
                            out_async_write(out_write_buf);
                        } else {
                            Log::log_with_endpoint(in_endpoint, "cannot establish connection to remote server " + config.remote_addr + ':' + to_string(config.remote_port), Log::ERROR);
                            destroy();
                        }
                    });
                } else {
                    Log::log_with_endpoint(in_endpoint, "cannot resolve remote server hostname " + config.remote_addr, Log::ERROR);
                    destroy();
                }
            });
            break;
        }
        case CONNECTING_REMOTE: {
            break;
        }
        case FORWARDING: {
            out_async_write(data);
            break;
        }
        case DESTROYING: {
            break;
        }
    }
}

void ServerSession::in_sent() {
    switch (status) {
        case HANDSHAKE: {
            break;
        }
        case CONNECTING_REMOTE: {
            break;
        }
        case FORWARDING: {
            out_async_read();
            break;
        }
        case DESTROYING: {
            break;
        }
    }
}

void ServerSession::out_async_read() {
    auto self = shared_from_this();
    out_socket.async_read_some(boost::asio::buffer(out_read_buf, MAX_LENGTH), [this, self](const boost::system::error_code error, size_t length) {
        if (!error) {
            out_recv(string((const char*)out_read_buf, length));
        } else {
            destroy();
        }
    });
}

void ServerSession::out_async_write(const string &data) {
    auto self = shared_from_this();
    boost::asio::async_write(out_socket, boost::asio::buffer(data), [this, self](const boost::system::error_code error, size_t) {
        if (!error) {
            out_sent();
        } else {
            destroy();
        }
    });
}

void ServerSession::out_recv(const string &data) {
    switch (status) {
        case HANDSHAKE: {
            break;
        }
        case CONNECTING_REMOTE: {
            break;
        }
        case FORWARDING: {
            in_async_write(data);
            break;
        }
        case DESTROYING: {
            break;
        }
    }
}

void ServerSession::out_sent() {
    switch (status) {
        case HANDSHAKE: {
            break;
        }
        case CONNECTING_REMOTE: {
            break;
        }
        case FORWARDING: {
            in_async_read();
            break;
        }
        case DESTROYING: {
            break;
        }
    }
}

void ServerSession::destroy() {
    if (status == DESTROYING) {
        return;
    }
    Log::log_with_endpoint(in_endpoint, "disconnected");
    status = DESTROYING;
    resolver.cancel();
    out_socket.close();
    auto self = shared_from_this();
    in_socket.async_shutdown([this, self](const boost::system::error_code){});
}
