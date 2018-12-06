#include <array>
#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <regex>
#include <signal.h>
#include <string>
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
        string method, url, version;

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
                        do_parse(_buffer.data());
                        do_response();
                    }
                });
        }

        void do_parse(string data) {
            stringstream ss_request_line(data);
            ss_request_line >> method;
            ss_request_line >> url;
            ss_request_line >> version;

            // cout << method << " " << url << " " << version << endl;
        }

        void do_response() {
            string path, query;
            regex url_regex("\\?");
            smatch url_match;

            regex_search(url, url_match, url_regex);
            if (url_match.empty() == false) {
                /* console.cgi?<query> */
                // cout << url_match.prefix().str().substr(1) 
                //      << " " << url_match.suffix().str() << endl;

                path = url_match.prefix().str().substr(1);

                size_t n, pos;
                string query = url_match.suffix().str();
                regex query_regex("h[0-4]=((?:nplinux|npbsd)[0-9].cs.nctu.edu.tw)&p[0-4]=([0-9]+)&f[0-4]=([^&]+)"); 
                smatch query_match;
                string host[MAX_SHELL_SESSION], file[MAX_SHELL_SESSION];
                int port[MAX_SHELL_SESSION];

                pos = 0;
                for (n = 0; n < MAX_SHELL_SESSION; n++) {
                    query = query.substr(pos);
                    regex_search(query, query_match, query_regex);
                    if (query_match.empty() == false) {
                        host[n] = query_match[1];
                        port[n] = atoi(query_match.str(2).c_str());
                        file[n] = query_match[3];
                        pos = query_match[0].length() + 1;

                        // cout << n << ": ";
                        // cout << host[n] << " ";
                        // cout << port[n] << " ";
                        // cout << file[n] << endl;
                    }
                    else {
                        for (size_t i = n; i < MAX_SHELL_SESSION; i++) {
                            host[n].clear();
                            port[n] = -1;
                            file[n].clear();
                        }
                        break;
                    }
                }

                pid_t pid;
                pid = fork();

                if (pid < 0) {
                    perror("fork error");
                    exit(1);
                }
                else if (pid == 0) {
                    /* child process */
                    char *argv[] = {NULL};
                    int sockfd = _socket.native_handle();

                    /* set environment variables */
                    for (size_t i = 0; i < MAX_SHELL_SESSION; i++) {
                        char name[3];
                        if (host[i].empty() == false) {
                            sprintf(name, "h%d", (int)i);
                            setenv(name, host[i].c_str(), 1);
                            sprintf(name, "p%d", (int)i);
                            setenv(name, to_string(port[i]).c_str(), 1);
                            sprintf(name, "f%d", (int)i);
                            setenv(name, file[i].c_str(), 1);
                        }
                    }

                    dup2(sockfd, STDIN_FILENO);
                    dup2(sockfd, STDOUT_FILENO);
                    cout << "HTTP/1.1 200 OK" << endl;
                    execv(path.c_str() /*console.cgi*/, argv);
                }
                else {
                    /* parent process */
                }
            }
            else {
                /* panel.cgi */
                // cout << url.substr(1) << endl;

                pid_t pid;
                pid = fork();

                if (pid < 0) {
                    perror("fork error");
                    exit(1);
                }
                else if (pid == 0) {
                    /* child process */
                    char *argv[] = {NULL};
                    int sockfd = _socket.native_handle();
                    dup2(sockfd, STDIN_FILENO);
                    dup2(sockfd, STDOUT_FILENO);
                    cout << "HTTP/1.1 200 OK" << endl;
                    execv(url.substr(1).c_str(), argv);
                }
                else {
                    /* parent process */
                    waitpid(pid, NULL, 0);
                }
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
