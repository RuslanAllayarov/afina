#ifndef AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H

#include <cstring>

#include <sys/epoll.h>

#include <afina/Storage.h>
#include <afina/logging/Service.h>
#include <afina/execute/Command.h>
#include <protocol/Parser.h>
namespace Afina {
namespace Network {
namespace MTnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<spdlog::logger> logger, std::shared_ptr<Afina::Storage> stor)
    : _socket(s), _logger(logger), pStorage(stor){
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
    }

    inline bool isAlive() const { return _is_alive; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class Worker;
    friend class ServerImpl;

    int _socket;
    struct epoll_event _event;

    bool _is_alive;
    std::mutex mutex;
    std::shared_ptr<spdlog::logger> _logger;
    std::shared_ptr<Afina::Storage> pStorage;
    std::unique_ptr<Afina::Execute::Command> command_to_execute;
    std::string _results;
    Protocol::Parser parser;
    std::size_t arg_remains;
    std::string argument_for_command;

    int _written_bytes;
    int _read_bytes;
    int _bytes_for_read;
    char client_buffer[4096];
};

} // namespace MTnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
