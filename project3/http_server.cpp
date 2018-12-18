#include <array>
#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <regex>
#include <signal.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define MAX_SHELL_SESSION 5

using namespace boost::asio;
using namespace std;

io_service global_io_service;

void sigchld_handler(int signo) {
    waitpid(-1, NULL, 0);
    return;
}

class Session : public enable_shared_from_this<Session> {
    private:
        enum { max_length = 1024 };
        ip::tcp::socket _socket;
        array<char, max_length> _buffer;
        string server_addr, server_port, remote_addr, remote_port;
        /* REQUEST_METHOD. REQUEST_URI, SCRIPT_NAME, QUERY_STRING, SERVER_PROTOCOL, HTTP_HOST */
        string method, uri, script, query, protocol, host;

    public:
        Session(ip::tcp::socket socket)
            : _socket(move(socket)) {}

        void start() { do_read(); }

    private:
        void do_read() {
            auto self(shared_from_this());
            _socket.async_read_some(
                buffer(_buffer, max_length),
                [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
                    if (!ec) {
                        server_addr = _socket.local_endpoint().address().to_string();
                        server_port = std::to_string(_socket.local_endpoint().port());
                        remote_addr = _socket.remote_endpoint().address().to_string();
                        remote_port = std::to_string(_socket.remote_endpoint().port());

                        do_parse(_buffer.data());
                        do_response();
                    }
                });
        }

        void do_parse(string data) {
            stringstream ss_request_line(data);
            string keys;
            ss_request_line >> method;
            ss_request_line >> uri;
            ss_request_line >> protocol;
            ss_request_line >> keys;
            ss_request_line >> host;

            regex uri_regex("\\?");
            smatch uri_match;

            regex_search(uri, uri_match, uri_regex);
            if (uri_match.empty() == false) {
                script = uri_match.prefix().str();
                query = uri_match.suffix().str();
            }
            else {
                script = uri;
                query.clear();
            }
        }

        void do_response() {
            struct stat buffer;
            if (stat(script.substr(1).c_str(), &buffer) != 0) {
                string response = "HTTP/1.1 404 Not Found\n";
                write(_socket, boost::asio::buffer(response, response.length()));
                return;
            }

            pid_t pid;
            pid = fork();

            if (pid < 0) {
                perror("fork error");
                exit(1);
            }
            else if (pid == 0) {
                /* child process */
                /* set http environment variable */
                setenv("REQUEST_METHOD", method.c_str(), 1);
                setenv("REQUEST_URI", uri.c_str(), 1);
                setenv("QUERY_STRING", query.c_str(), 1);
                setenv("SERVER_PROTOCOL", protocol.c_str(), 1);
                setenv("HTTP_HOST", host.c_str(), 1);
                setenv("SERVER_ADDR", server_addr.c_str(), 1);
                setenv("SERVER_PORT", server_port.c_str(), 1);
                setenv("REMOTE_ADDR", remote_addr.c_str(), 1);
                setenv("REMOTE_PORT", remote_port.c_str(), 1);

                char *argv[] = {NULL};
                int sockfd = _socket.native_handle();
                dup2(sockfd, STDIN_FILENO);
                dup2(sockfd, STDOUT_FILENO);
                cout << "HTTP/1.1 200 OK" << endl;
                execv(script.substr(1).c_str(), argv);
            }
            else {
                /* parent process */
                waitpid(pid, NULL, 0);
            }
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

    signal(SIGCHLD, sigchld_handler);

    try {
        short port = atoi(argv[1]);
        Server s(port);
        global_io_service.run();
    } catch (exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }

    return 0;
}
