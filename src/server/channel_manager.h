#ifndef CHANNEL_MANAGER_H
#define CHANNEL_MANAGER_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

namespace tt::chat::server {

    class ChannelManager {
    public:
        void create_channel(const std::string &name);
        bool has_channel(const std::string &name) const;
        std::unordered_set<int>& get_members(const std::string &name);
        std::vector<std::string> list_channels() const;
        void join_channel(const std::string &name, const std::string &prev_channel, int client_fd);

    private:
        std::unordered_map<std::string, std::unordered_set<int>> channels_;
    };

} // namespace tt::chat::server

#endif // CHANNEL_MANAGER_H