#include <array>
#include <boost/asio.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <signal.h>
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stdio.h>

#define SOCK_CONFIG "socks.conf"

using namespace boost::asio;
using namespace std;

io_service global_io_service;
enum Command { CONNECT = 1, BIND, UNKNOWN };
enum Reply { ACCEPT = 90, REJECT, CONN_REJECT };
enum Firewall { PERMIT = 0, DENY };

ostream& operator<<(ostream& os, const Command c) {
    string str;
    switch (c) {
        case 1:
            str = "CONNECT";
            break;
        case 2:
            str = "BIND";
            break;
        case 3:
            str = "UNKNOWN";
            break;
    }
    return os << str;
}

ostream& operator<<(ostream& os, const Reply r) {
    string str;
    switch (r) {
        case 90:
            str = "ACCEPT";
            break;
        case 91:
            str = "REJECT";
            break;
        case 92:
            str = "REJECT";
            break;
    }
    return os << str;
}

class Session : public enable_shared_from_this<Session> {
    private:
        enum { max_length = 1024 };
        ip::tcp::socket _socket;
        array<unsigned char, max_length> _buffer, local_buffer, remote_buffer;
        string s_ip, s_port, d_ip, d_port;
        unsigned char dstport[2], dstip[4];
        Command command;
        Reply reply;
        Firewall firewall;
        ip::tcp::socket conn_socket;
        int bind_port;
        ip::tcp::acceptor bind_acceptor;

    public:
        Session(ip::tcp::socket socket)
            : _socket(move(socket)),
              conn_socket(global_io_service),
              bind_acceptor(global_io_service) {}

        ~Session() {}

        void start() { do_read(); }

    private:
        void do_read() {
            auto self(shared_from_this());
            _socket.async_read_some(
                buffer(_buffer, max_length),
                [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
                    if (!ec) {
                        s_ip = _socket.remote_endpoint().address().to_string();
                        s_port = std::to_string(_socket.remote_endpoint().port());

                        do_parse(_buffer);
                    }
                });
        }

        void do_parse(array<unsigned char, max_length> data) {
            int vn = data[0], cd = data[1];
            for (size_t i = 0; i < 2; i++)
                dstport[i] = data[i + 2];
            for (size_t i = 0; i < 4; i++)
                dstip[i] = data[i + 4];

            d_port = to_string(int(dstport[0]) * 256 + int(dstport[1]));
            d_ip = to_string(dstip[0]) + "." +
                   to_string(dstip[1]) + "." +
                   to_string(dstip[2]) + "." +
                   to_string(dstip[3]);

            if (int(vn) == 4 && int(cd) == 1) {
                /* CONNECT requests */
                command = CONNECT;
            }
            else if (int(vn) == 4 && int(cd) == 2) {
                /* BIND requests */
                command = BIND;
            }
            else {
                command = UNKNOWN;
            }

            ifstream config_file(SOCK_CONFIG);
            string line;
            firewall = DENY;    /* default action of firewall */
            if (config_file.is_open()) {
                while (getline(config_file, line)) {
                    stringstream ss;
                    string action, mode, ip;
                    size_t found = line.find("#");
                    line = line.substr(0, found);
                    ss << line;
                    ss >> action >> mode >> ip;

                    if (command == CONNECT && mode == "c") {
                        size_t found = ip.find("*");
                        found = d_ip.find(ip.substr(0, found));
                        if (action == "permit" && found != std::string::npos) {
                            firewall = PERMIT;
                            break;
                        }
                    }
                    else if (command == BIND && mode == "b") {
                        size_t found = ip.find("*");
                        found = d_ip.find(ip.substr(0, found));
                        if (action == "permit" && found != std::string::npos) {
                            firewall = PERMIT;
                            break;
                        }
                    }
                }
                config_file.close();
            }

            do_handle();
        }

        void do_handle() {
            if (firewall == DENY) {
                reply = REJECT;
            }
            else if (command == CONNECT) {
                do_connect();
                do_reply(dstport, dstip);
                if (reply == ACCEPT) {
                    do_read_local();
                    do_read_remote();
            }
            }
            else if (command == BIND) {
                do_bind();
                unsigned char _dstport[2] = {(unsigned char)(bind_port / 256), (unsigned char)(bind_port % 256)};
                unsigned char _dstip[4] = {0, 0, 0, 0};
                do_reply(_dstport, _dstip);
            }
            else {
                reply = REJECT;
            }

            cout << "<S_IP>: " << s_ip << endl;
            cout << "<S_PORT>: " << s_port << endl;
            cout << "<D_IP>: " << d_ip << endl;
            cout << "<D_PORT>: " << d_port << endl;
            cout << "<Command>: " << command << endl;
            cout << "<Reply>: " << reply << endl;
            cout << endl;
        }

        void do_reply(unsigned char* _dstport, unsigned char* _dstip) {
            /* SOCKS4_REPLY */
            stringstream ss;
            string socks4_reply;
            unsigned char vn = 0, cd = reply;

            ss << vn << cd;
            for (size_t i = 0; i < 2; i++)
                ss << _dstport[i];
            for (size_t i = 0; i < 4; i++)
                ss << _dstip[i];

            ss >> socks4_reply;

            // for (size_t i = 0; i < socks4_reply.length(); i++)
            //     cout << int(socks4_reply[i]) << " ";
            // cout << endl;

            write(_socket, boost::asio::buffer(socks4_reply, socks4_reply.length()));
        }

        void do_connect() {
            ip::tcp::resolver _resolver(global_io_service);
            try {
                connect(conn_socket, _resolver.resolve({d_ip, d_port}));
                reply = ACCEPT;
            } catch (exception& e) {
                reply = CONN_REJECT;
            }
        }

        void do_bind() {
            auto self(shared_from_this());
            /* randomly choose a port from 5000 to 65535 */
            bind_port = rand() % 60536 + 5000;
            ip::tcp::endpoint bind_endpoint(ip::tcp::v4(), bind_port);
            bind_acceptor.open(bind_endpoint.protocol());
            bind_acceptor.set_option(ip::tcp::acceptor::reuse_address(true));
            bind_acceptor.bind(bind_endpoint);
            bind_acceptor.listen();
            // bind_acceptor(global_io_service, ip::tcp::endpoint(ip::tcp::v4(), bind_port));

            bind_acceptor.async_accept(conn_socket, [this, self](boost::system::error_code ec) {
                if (!ec) {
                    do_read_local();
                    do_read_remote();
                    do_reply(dstport, dstip);
                }
            });

            reply = ACCEPT;
        }

        void do_read_local() {
            auto self(shared_from_this());
            _socket.async_read_some(
                boost::asio::buffer(local_buffer, max_length),
                [this, self](boost::system::error_code ec, size_t length) {
                    if (!ec) do_write_remote(length);
                    else do_close();
                });
        }

        void do_read_remote() {
            auto self(shared_from_this());
            conn_socket.async_read_some(
                boost::asio::buffer(remote_buffer, max_length),
                [this, self](boost::system::error_code ec, size_t length) {
                    if (!ec) do_write_local(length);
                    else do_close();
                });
        }

        void do_write_local(size_t length) {
            auto self(shared_from_this());
            _socket.async_send(
                boost::asio::buffer(remote_buffer, length),
                [this, self](boost::system::error_code ec, size_t) {
                    if (!ec) do_read_remote();
                    else do_close();
                });
        }

        void do_write_remote(size_t length) {
            auto self(shared_from_this());
            conn_socket.async_send(
                boost::asio::buffer(local_buffer, length),
                [this, self](boost::system::error_code ec, size_t) {
                    if (!ec) do_read_local();
                    else do_close();
                });
        }

        void do_close() {
            _socket.close();
            conn_socket.close();
        }
};

class Server {
    private:
        ip::tcp::acceptor _acceptor;
        ip::tcp::socket _socket;

    public:
        Server(short port)
            : _acceptor(global_io_service, ip::tcp::endpoint(ip::tcp::v4(), port)),
              _socket(global_io_service)
        {
            do_accept();
        }

    private:
        void do_accept() {
            _acceptor.async_accept(_socket, [this](boost::system::error_code ec) {
                if (!ec)
                    make_shared<Session>(move(_socket))->start();
                do_accept();
            });
        }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    try {
        int port = atoi(argv[1]);
        Server s(port);
        global_io_service.run();
    } catch (exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }

    return 0;
}
