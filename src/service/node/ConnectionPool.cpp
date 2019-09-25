/*
 * Copyright 2017-present Shawn Cao
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ConnectionPool.h"
#include <glog/logging.h>

namespace nebula {
namespace service {
namespace node {

std::shared_ptr<ConnectionPool> ConnectionPool::init() noexcept {
  static const auto inst = std::shared_ptr<ConnectionPool>(new ConnectionPool());
  return inst;
}

// TODO(cao): we don't have maintainance yet,
// ideally to have health check peridically and recreate channel when necessary.
// api to get maintained channel
std::shared_ptr<grpc::Channel> ConnectionPool::connection(const std::string& addr) {
  // lock here for new connection creation?
  auto located = connections_.find(addr);
  if (located != connections_.end()) {
    // do a little maintainance here
    auto channel = located->second;
    auto state = channel->GetState(true);
    if (state != grpc_connectivity_state::GRPC_CHANNEL_SHUTDOWN) {
      return channel;
    }

    // will be replaced by channel recreation below
    LOG(INFO) << "Seeing a dead channel to " << addr;
  }

  // all client configurations come to here
  grpc::ChannelArguments chArgs;
  chArgs.SetMaxReceiveMessageSize(-1);
  LOG(INFO) << "Creating a channel to " << addr;

  auto channel = grpc::CreateCustomChannel(addr, grpc::InsecureChannelCredentials(), chArgs);
  connections_[addr] = channel;

  return channel;
}

} // namespace node
} // namespace service
} // namespace nebula