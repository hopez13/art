// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <stdint.h>

namespace dt_fds {

// Packet types that the server will send to the client.
enum class ServerPacketType : uint32_t {
  kData = 0,
  kConnected = 1,
  kDisconnect = 2,
};

struct ServerPacket {
  ServerPacketType type_;
};

struct ServerDataPacket : public ServerPacket {
  uint32_t data_len_;
  uint8_t data_[0];
};

struct ServerConnectedPacket : public ServerPacket {
  uint32_t sequence_number_;
};

struct ServerDisconnectPacket : public ServerPacket {
};

// Types of packets the client will send to the server.
enum class ClientPacketType : uint32_t {
  kData = 0,
  kConnect = 1,
  kDisconnect = 2,
};

struct ClientPacket {
  ClientPacketType type_;
};

struct ClientDataPacket : public ClientPacket {
  uint32_t data_len_;
  uint8_t data_[0];
};

struct ClientConnectPacket : public ClientPacket {
  uint32_t sequence_number_;
};

struct ClientDisconnectPacket : public ClientPacket {
};

}  // namespace dt_fds
