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

#include "config.h"
#include <string>
#include <cstdint>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <openssl/sha.h>
#include "log.h"
using namespace std;
using namespace boost::property_tree;

void Config::load(const string &filename) {
    ptree tree;
    read_json(filename, tree);
    run_type = (tree.get("run_type", string("client")) == "server") ? SERVER : CLIENT;
    local_addr = tree.get("local_addr", string());
    local_port = tree.get("local_port", uint16_t(0));
    remote_addr = tree.get("remote_addr", string());
    remote_port = tree.get("remote_port", uint16_t(0));
    password = tree.get("password", string());
    password = Config::SHA224(password);
    keyfile = tree.get("keyfile", string());
    keyfile_password = tree.get("keyfile_password", string());
    certfile = tree.get("certfile", string());
    ssl_verify = tree.get("ssl_verify", true);
    ssl_verify_hostname = tree.get("ssl_verify_hostname", true);
    ca_certs = tree.get("ca_certs", string());
    log_level = static_cast<Log::Level>(tree.get("log_level", 1));
    Log::level = log_level;
}

string Config::SHA224(const string &message) {
    uint8_t digest[SHA224_DIGEST_LENGTH];
    SHA256_CTX ctx;
    SHA224_Init(&ctx);
    SHA224_Update(&ctx, message.c_str(), message.length());
    SHA224_Final(digest, &ctx);
    char mdString[(SHA224_DIGEST_LENGTH << 1) + 1];
    for (int i = 0; i < SHA224_DIGEST_LENGTH; ++i) {
        sprintf(mdString + (i << 1), "%02x", (unsigned int)digest[i]);
    }
    return string(mdString);
}
