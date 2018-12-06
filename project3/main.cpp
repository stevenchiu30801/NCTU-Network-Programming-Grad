#include <array>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
// #include <signal.h>
#include <sstream>
#include <string>
// #include <sys/types.h>
// #include <sys/wait.h>
// #include <unistd.h>
#define MAX_SHELL_SESSION 5
#define MAX_LINE_LENGTH 1024
#define MAX_REPLY_LENGTH 8192
#define SLEEP_TIME 500

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std;

io_service global_io_service;

extern void panel(tcp::socket socket);

// void sigchld_handler(int signo) {
//     waitpid(-1, NULL, 0);
//     return;
// }

void panel(tcp::socket socket) {
    const string test_case_dir = "test_case";
    const string domain = "cs.nctu.edu.tw";
    const size_t n_test_cases = 10, n_hosts = 5;
    string test_case_menu, host_menu;
    stringstream ss;
    string str;
    const char *cstr;

    for(size_t i = 0; i < n_test_cases; i++) {
        test_case_menu += "<option value=\"t" + to_string(i + 1) +
                          ".txt\">t" + to_string(i + 1) + ".txt</option>";
    }
    for(size_t i = 0; i < n_hosts; i++) {
        string host = "nplinux" + to_string(i + 1);
        host_menu += "<option value=\"" + host + "." + domain + "\">" + host + "</option>";
    }
    for(size_t i = 0; i < n_hosts; i++) {
        string host = "npbsd" + to_string(i + 1);
        host_menu += "<option value=\"" + host + "." + domain + "\">" + host + "</option>";
    }

    ss << "Content-type: text/html\r\n\r\n";
    ss << "<!DOCTYPE html>\n\
<html lang=\"en\">\n\
  <head>\n\
    <title>NP Project 3 Panel</title>\n\
    <link\n\
      rel=\"stylesheet\"\n\
      href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\"\n\
      integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\"\n\
      crossorigin=\"anonymous\"\n\
    />\n\
    <link\n\
      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n\
      rel=\"stylesheet\"\n\
    />\n\
    <link\n\
      rel=\"icon\"\n\
      type=\"image/png\"\n\
      href=\"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png\"\n\
    />\n\
    <style>\n\
      * {\n\
        font-family: 'Source Code Pro', monospace;\n\
      }\n\
    </style>\n\
  </head>\n\
  <body class=\"bg-secondary pt-5\">\n";

    ss << "<form action=\"console.cgi\" method=\"GET\">\n\
      <table class=\"table mx-auto bg-light\" style=\"width: inherit\">\n\
        <thead class=\"thead-dark\">\n\
          <tr>\n\
            <th scope=\"col\">#</th>\n\
            <th scope=\"col\">Host</th>\n\
            <th scope=\"col\">Port</th>\n\
            <th scope=\"col\">Input File</th>\n\
          </tr>\n\
        </thead>\n\
        <tbody>\n";

    for(size_t i = 0; i < n_hosts; i ++) {
        ss << "<tr>\n\
            <th scope=\"row\" class=\"align-middle\">Session " << to_string(i + 1) << "</th>\n\
            <td>\n\
              <div class=\"input-group\">\n\
                <select name=\"h" << to_string(i) << "\" class=\"custom-select\">\n\
                  <option></option>" << host_menu << "\n\
                </select>\n\
                <div class=\"input-group-append\">\n\
                  <span class=\"input-group-text\">.cs.nctu.edu.tw</span>\n\
                </div>\n\
              </div>\n\
            </td>\n\
            <td>\n\
              <input name=\"p" << to_string(i) << "\" type=\"text\" class=\"form-control\" size=\"5\" />\n\
            </td>\n\
            <td>\n\
              <select name=\"f" << to_string(i) << "\" class=\"custom-select\">\n\
                <option></option>\n\
                " << test_case_menu << "\n\
              </select>\n\
            </td>\n\
          </tr>\n";
    }

    ss << "<tr>\n\
            <td colspan=\"3\"></td>\n\
            <td>\n\
              <button type=\"submit\" class=\"btn btn-info btn-block\">Run</button>\n\
            </td>\n\
          </tr>\n\
        </tbody>\n\
      </table>\n\
    </form>\n\
  </body>\n\
</html>";

    str = ss.str();
    cstr = str.c_str();
    write(socket, buffer(cstr, strlen(cstr)));
}

class console {
    private:
        tcp::socket socket;
        size_t nserv;
        string hosts[MAX_SHELL_SESSION], ports[MAX_SHELL_SESSION], files[MAX_SHELL_SESSION];
    public:
        console(tcp::socket _socket, size_t _nserv, string *_hosts, string *_ports, string *_files)
            : socket(move(_socket)),
              nserv(_nserv) {
                for (size_t i = 0; i < nserv; i++) {
                    hosts[i] = _hosts[i];
                    ports[i] = _ports[i];
                    files[i] = _files[i];
                }
            }

        void encode(std::string *data) {
            /* escape XML/HTML */
            for(size_t pos = 0; pos != data->size(); pos++) {
                switch(data->at(pos)) {
                    case '&':
                        data->replace(pos, 1, "&amp;");
                        break;
                    case '\"':
                        data->replace(pos, 1, "&quot;");
                        break;
                    case '\'':
                        data->replace(pos, 1, "&apos;");
                        break;
                    case '<':
                        data->replace(pos, 1, "&lt;");
                        break;
                    case '>':
                        data->replace(pos, 1, "&gt;");
                        break;
                    case '\r':
                        data->replace(pos, 1, "\\r");
                        break;
                    case '\n':
                        data->replace(pos, 1, "\\n");
                        break;
                    default:
                        break;
                }
            }
        }

        void output_shell(string session, string content) {
            stringstream ss;
            string str;
            const char *cstr;
            encode(&content);
            ss << "<script>document.getElementById('" << session 
                 << "').innerHTML += '" << content << "';</script>";
            str = ss.str();
            cstr = str.c_str();
            write(socket, buffer(cstr, strlen(cstr)));
        }

        void output_command(string session, string content) {
            stringstream ss;
            string str;
            const char* cstr;
            encode(&content);
            ss << "<script>document.getElementById('" << session 
                 << "').innerHTML += '<b>" << content << "</b>';</script>";
            str = ss.str();
            cstr = str.c_str();
            write(socket, buffer(cstr, strlen(cstr)));
        }

        void output_html() {
            stringstream ss;
            string str;
            const char *cstr;
            ss << "Content-type: text/html\r\n\r\n";
            ss << "<!DOCTYPE html>\n\
        <html lang=\"en\">\n\
          <head>\n\
            <meta charset=\"UTF-8\" />\n\
            <title>NP Project 3 Console</title>\n\
            <link\n\
              rel=\"stylesheet\"\n\
              href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\"\n\
              integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\"\n\
              crossorigin=\"anonymous\"\n\
            />\n\
            <link\n\
              href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n\
              rel=\"stylesheet\"\n\
            />\n\
            <link\n\
              rel=\"icon\"\n\
              type=\"image/png\"\n\
              href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n\
            />\n\
            <style>\n\
              * {\n\
                font-family: 'Source Code Pro', monospace;\n\
                font-size: 1rem !important;\n\
              }\n\
              body {\n\
                background-color: #212529;\n\
              }\n\
              pre {\n\
                color: #cccccc;\n\
              }\n\
              b {\n\
                color: #ffffff;\n\
              }\n\
            </style>\n\
          </head>\n\
          <body>\n\
            <table class=\"table table-dark table-bordered\">\n\
              <thead>\n\
                <tr>\n";

            for (size_t i = 0; i < nserv; i++)
                ss << "          <th scope=\"col\">" << hosts[i] << ":"
                     << ports[i] << "</th>\n";

            ss << "        </tr>\n\
              </thead>\n\
              <tbody>\n\
                <tr>\n";

            for (size_t i = 0; i < nserv; i++)
                ss << "          <td><pre id=\"s" << i
                     << "\" class=\"mb-0\"></pre></td>\n";

            ss << "        </tr>\n\
              </tbody>\n\
            </table>\n\
          </body>\n\
        </html>";

            str = ss.str();
            cstr = str.c_str();
            write(socket, buffer(cstr, strlen(cstr)));
        }

        void session(size_t i) {
            io_service _io_service;
            tcp::socket _socket(_io_service);
            tcp::resolver _resolver(_io_service);
            boost::system::error_code error;
            connect(_socket, _resolver.resolve({hosts[i], ports[i]}));
            // connect(_socket, _resolver.resolve({"127.0.0.1", ports[i]}));
            boost::this_thread::sleep(boost::posix_time::milliseconds(SLEEP_TIME));

            string session = "s" + to_string(i);
            ifstream _ifstream("test_case/" + string(files[i]));
            // ifstream _ifstream("test_case/t1.txt");
            string line;
            char request[MAX_LINE_LENGTH], reply[MAX_REPLY_LENGTH];
            
            memset(reply, 0, MAX_REPLY_LENGTH);
            _socket.read_some(buffer(reply, MAX_REPLY_LENGTH));
            output_shell(session, string(reply));
            boost::this_thread::sleep(boost::posix_time::milliseconds(SLEEP_TIME));

            while (getline(_ifstream, line)) {
                line = line + "\n";
                // strcpy(request, line.c_str());
                size_t len = line.copy(request, line.length(), 0);
                request[len] = '\0';
                write(_socket, buffer(request, strlen(request)));
                output_command(session, string(request));
                boost::this_thread::sleep(boost::posix_time::milliseconds(SLEEP_TIME));

                memset(reply, 0, MAX_REPLY_LENGTH);
                _socket.read_some(buffer(reply, MAX_REPLY_LENGTH), error);
                if (error == error::eof) {
                    /* connection closed */
                    break;
                }
                output_shell(session, string(reply));
                boost::this_thread::sleep(boost::posix_time::milliseconds(SLEEP_TIME));

                if (line == "exit\n") {
                    /* tricky way to determine if connection closed */
                    break;
                }
            }
        }

        void start() {
            output_html();

            boost::thread *sessions[MAX_SHELL_SESSION];

            for (size_t i = 0; i < nserv; i++) 
                sessions[i] = new boost::thread([this, i]{session(i);});
            for (size_t i = 0; i < nserv; i++) {
                sessions[i]->join();
                delete sessions[i];
            }

            // signal(SIGCHLD, sigchld_handler);

            // pid_t pid;
            // for (size_t i = 0; i < nserv; i++) {
            //     pid = fork();
            //     if (pid < 0) {
            //         perror("fork error");
            //         exit(1);
            //     }
            //     else if (pid == 0) {
            //         session(i);
            //     }
            //     else {

            //     }
            // }
        }
};

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

                size_t nserv;
                string hosts[MAX_SHELL_SESSION], ports[MAX_SHELL_SESSION], files[MAX_SHELL_SESSION];
                size_t n, pos;
                string query = url_match.suffix().str();
                regex query_regex("h[0-4]=((?:nplinux|npbsd)[0-9].cs.nctu.edu.tw)&p[0-4]=([0-9]+)&f[0-4]=([^&]+)"); 
                smatch query_match;
                
                pos = 0;
                nserv = 0;
                for (n = 0; n < MAX_SHELL_SESSION; n++) {
                    query = query.substr(pos);
                    regex_search(query, query_match, query_regex);
                    if (query_match.empty() == false) {
                        hosts[n] = query_match[1];
                        ports[n] = query_match[2];
                        files[n] = query_match[3];
                        pos = query_match[0].length() + 1;
                        nserv++;

                        // cout << n << ": ";
                        // cout << hosts[n] << " ";
                        // cout << ports[n] << " ";
                        // cout << files[n] << endl;
                    }
                    else {
                        for (size_t i = n; i < MAX_SHELL_SESSION; i++) {
                            hosts[n].clear();
                            ports[n] = -1;
                            files[n].clear();
                        }
                        break;
                    }
                }

                stringstream ss;
                string str;
                const char* cstr;
                ss << "HTTP/1.1 200 OK" << endl;
                str = ss.str();
                cstr = str.c_str();
                write(_socket, buffer(cstr, strlen(cstr)));
                console c(move(_socket), nserv, hosts, ports, files);
                c.start();
            }
            else {
                /* panel.cgi */
                // cout << url.substr(1) << endl;

                stringstream ss;
                string str;
                const char *cstr;
                ss << "HTTP/1.1 200 OK" << endl;
                str = ss.str();
                cstr = str.c_str();
                write(_socket, buffer(cstr, strlen(cstr)));
                panel(move(_socket));
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

    // signal(SIGCHLD, sigchld_handler);

    try {
        short port = atoi(argv[1]);
        Server s(port);
        global_io_service.run();
    } catch (exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }

    return 0;
}
