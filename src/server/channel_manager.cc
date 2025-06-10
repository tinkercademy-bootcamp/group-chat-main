#include "channel_manager.h"

namespace tt::chat::server {

    void ChannelManager::create_channel(const std::string &name) {
        channels_[name];  // default-initialize set
    }

    bool ChannelManager::has_channel(const std::string &name) const {
        return channels_.find(name) != channels_.end();
    }

    std::unordered_set<int>& ChannelManager::get_members(const std::string &name) {
        return channels_[name];
    }

    std::vector<std::string> ChannelManager::list_channels() const {
        std::vector<std::string> result;
        for (const auto &[name, _] : channels_) result.push_back(name);
        return result;
    }

    void ChannelManager::join_channel(const std::string &name, const std::string &prev_channel, int client_fd) {
        if(prev_channel != "") {
            channels_[prev_channel].erase(client_fd);
        }
        create_channel(name);
        channels_[name].insert(client_fd);
    }

} // namespace tt::chat::server