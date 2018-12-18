#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <signal.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#define MAX_SHELL_SESSION 5
#define MAX_LINE_LENGTH 1024
#define MAX_REPLY_LENGTH 8192
#define SLEEP_TIME 500

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std;

static size_t nserv;
static string host[MAX_SHELL_SESSION], port[MAX_SHELL_SESSION], file[MAX_SHELL_SESSION];

void parse_env(void) {
    size_t pos;
    string query = getenv("QUERY_STRING");
    regex query_regex("h[0-4]=((?:nplinux|npbsd)[0-9].cs.nctu.edu.tw)&p[0-4]=([0-9]+)&f[0-4]=([^&]+)");
    smatch query_match;

    pos = 0;
    for (nserv = 0; nserv < MAX_SHELL_SESSION; nserv++) {
        query = query.substr(pos);
        regex_search(query, query_match, query_regex);
        if (query_match.empty() == false) {
            host[nserv] = query_match[1];
            port[nserv] = query_match[2];
            file[nserv] = query_match[3];

            pos = query_match[0].length() + 1;
        }
        else
            break;
    }
    return;
}

void output_html(void) {
    cout << "Content-type: text/html\r\n\r\n";
    cout << "<!DOCTYPE html>\n\
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
        cout << "          <th scope=\"col\">" << host[i] << ":"
             << port[i] << "</th>\n";

    cout << "        </tr>\n\
      </thead>\n\
      <tbody>\n\
        <tr>\n";

    for (size_t i = 0; i < nserv; i++)
        cout << "          <td><pre id=\"s" << i
             << "\" class=\"mb-0\"></pre></td>\n";

    cout << "        </tr>\n\
      </tbody>\n\
    </table>\n\
  </body>\n\
</html>";
    flush(cout);
    return;
}

void sigchld_handler(int signo) {
    waitpid(-1, NULL, 0);
    return;
}

void encode(std::string *data) {
    /* escape XML/HTML */
    /* Refer to http://www.interfacebus.com/html_escape_codes.html */
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
                // data->replace(pos, 1, "\\r");
                data->replace(pos, 1, "&#13;");
                break;
            case '\n':
                // data->replace(pos, 1, "\\n");
                data->replace(pos, 1, "&#10;");
                break;
            case '\\':
                data->replace(pos, 1, "&#92;");
                break;
            default:
                break;
        }
    }
}

void output_shell(string session, string content) {
    encode(&content);
    cout << "<script>document.getElementById('" << session 
         << "').innerHTML += '" << content << "';</script>";
    flush(cout);
    return;
}

void output_command(string session, string content) {
    encode(&content);
    cout << "<script>document.getElementById('" << session 
         << "').innerHTML += '<b>" << content << "</b>';</script>";
    flush(cout);
    return;
}

int main(void) {
    parse_env();
    output_html();

    pid_t pid;

    signal(SIGCHLD, sigchld_handler);

    for (size_t i = 0; i < nserv; i++) {
        pid = fork();

        if (pid < 0) {
            perror("fork error");
            exit(1);
        }
        else if (pid == 0) {
            /* child process */
            io_service _io_service;
            tcp::socket _socket(_io_service);
            tcp::resolver _resolver(_io_service);
            boost::system::error_code error;
            connect(_socket, _resolver.resolve({host[i], port[i]}));
            // connect(_socket, _resolver.resolve({"127.0.0.1", port[i]}));
            boost::this_thread::sleep(boost::posix_time::milliseconds(SLEEP_TIME));

            string session = "s" + to_string(i);
            ifstream _ifstream("test_case/" + string(file[i]));
            // ifstream _ifstream("test_case/t1.txt");
            string line;
            char request[MAX_LINE_LENGTH], reply[MAX_REPLY_LENGTH];
            
            memset(reply, 0, MAX_REPLY_LENGTH);
            _socket.read_some(buffer(reply, MAX_REPLY_LENGTH));
            output_shell(session, string(reply));
            boost::this_thread::sleep(boost::posix_time::milliseconds(SLEEP_TIME));

            size_t conn_close = 0;
            while (getline(_ifstream, line)) {
                line = line + "\n";
                strcpy(request, line.c_str());
                write(_socket, buffer(request, strlen(request)));
                output_command(session, string(request));
                boost::this_thread::sleep(boost::posix_time::milliseconds(SLEEP_TIME));

                while (1) {
                    memset(reply, 0, MAX_REPLY_LENGTH);
                    _socket.read_some(buffer(reply, MAX_REPLY_LENGTH), error);
                    if (error == error::eof) {
                        /* connection closed */
                        conn_close = 1;
                        break;
                    }
                    output_shell(session, string(reply));
                    boost::this_thread::sleep(boost::posix_time::milliseconds(SLEEP_TIME));

                    if (strcmp(reply + (strlen(reply) - 2), "% ") == 0)
                        break;
                    if (line == "exit\n") {
                        /* tricky way to determine if connection closed */
                        conn_close = 1;
                        break;
                    }
                }
                if (conn_close == 1)
                    break;
            }
            exit(0);
        }
        else {
            /* parent process */
        }
    }

    for (size_t i = 0; i < nserv; i++)
        waitpid(-1, NULL, 0);
    return 0;
}
