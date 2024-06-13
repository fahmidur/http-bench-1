//------------------------------------------------------------------------------
//
// Example: HTTP server, small
//
//------------------------------------------------------------------------------

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>

namespace asio = boost::asio;
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace my_program_state {
    std::size_t

    _req_count() {
        static std::size_t count = 0;
        return ++count;
    }

    std::time_t
    now() {
        return std::time(0);
    }
}

class http_connection : public std::enable_shared_from_this<http_connection> {
public:
    http_connection(tcp::socket socket) : 
        _sock_(std::move(socket))
    {
        // this is ugly    
    }

    // Initiate the asynchronous operations associated with the connection.
    void
    start() {
        read_request();
    }

private:
    // The socket for the currently connected client.
    tcp::socket _sock_;

    // The buffer for performing reads.
    beast::flat_buffer buffer_{8192};

    // The request message.
    http::request<http::dynamic_body> _req_;

    // The response message.
    http::response<http::dynamic_body> _res_;

    // The timer for putting a deadline on connection processing.
    // asio::steady_timer _deadline_{
    //     _sock_.get_executor(), std::chrono::seconds(60)
    // };

    // Asynchronously receive a complete request message.
    void read_request() {
        auto self = shared_from_this();

        http::async_read(
            _sock_,
            buffer_,
            _req_,
            [self](beast::error_code ec, std::size_t bytes_transferred) {
                boost::ignore_unused(bytes_transferred);
                if(!ec) {
                    self->process_request();
                }
            });
    }

    // Determine what needs to be done with the request message.
    void process_request() {
        _res_.version(_req_.version());
        _res_.keep_alive(true);

        switch(_req_.method()) {
        case http::verb::get:
            _res_.result(http::status::ok);
            _res_.set(http::field::server, "Beast");
            create_response();
            break;
        default:
            // We return responses indicating an error if
            // we do not recognize the request method.
            _res_.result(http::status::bad_request);
            _res_.set(http::field::content_type, "text/plain");
            beast::ostream(_res_.body())
                << "Invalid request-method '"
                << std::string(_req_.method_string())
                << "'";
            break;
        }

        write_response();
    }

    // Construct a response message based on the program state.
    void create_response() {
        if(_req_.target() == "/time") {
            _res_.set(http::field::content_type, "text/html");
            beast::ostream(_res_.body())
                <<  "<html>\n"
                <<  "<head><title>Current time</title></head>\n"
                <<  "<body>\n"
                <<  "<h1>Current time</h1>\n"
                <<  "<p>The current time is "
                <<  my_program_state::now()
                <<  " seconds since the epoch.</p>\n"
                <<  "</body>\n"
                <<  "</html>\n";
        }
        else {
            _res_.result(http::status::not_found);
            _res_.set(http::field::content_type, "text/plain");
            beast::ostream(_res_.body()) << "File not found\r\n";
        }
    }

    // Asynchronously transmit the response message.
    void write_response() {
        auto self = shared_from_this();

        _res_.content_length(_res_.body().size());

        http::async_write(
            _sock_,
            _res_,
            [self](beast::error_code ec, std::size_t) {
                self->_sock_.shutdown(tcp::socket::shutdown_send, ec);
            }
        );
    }
};

// "Loop" forever accepting new connections.
void http_server(tcp::acceptor& acceptor, tcp::socket& socket) {
  acceptor.async_accept(
    socket,
    [&](beast::error_code ec) {
        if(!ec) {
            std::make_shared<http_connection>(std::move(socket))->start();
        }
        http_server(acceptor, socket);
    });
}

void print_usage(const char* argv0) {
    std::cout << "Usage of " << argv0 << ":" << std::endl;
    std::cout << "  -addr string" << std::endl;
    std::cout << "    The listening address (default \"127.0.0.1\")" << std::endl;
    std::cout << "  -port int" << std::endl;
    std::cout << "    The listening port (default 3535)" << std::endl;
}

int main(int argc, char* argv[]) {
    try {
        const char* arg_address = NULL;
        const char* arg_port = NULL;
        bool expecting_address = false;
        bool expecting_port = false;
        for(int i = 1; i < argc; i++) {
            if(
                strcmp(argv[i], "-addr") == 0 || 
                strcmp(argv[i], "--addr") == 0 ||
                strcmp(argv[i], "-address") == 0 ||
                strcmp(argv[i], "--address") == 0
            ) {
                expecting_address = true;
            }
            else
            if(expecting_address) {
                expecting_address = false;
                // std::cout << "debug. setting arg_address to " << argv[i] << std::endl;
                arg_address = argv[i];
            }
            else
            if(
                strcmp(argv[i], "-port") == 0 ||
                strcmp(argv[i], "--port") == 0
            ) {
                expecting_port = true;
            }
            else
            if(expecting_port) {
                expecting_port = false;
                // std::cout << "debug. setting arg_port to " << argv[i] << std::endl;
                arg_port = argv[i];
            }
            else
            if(
                strcmp(argv[i], "-help") == 0 ||
                strcmp(argv[i], "--help") == 0
            ) {
                print_usage(argv[0]);
                exit(0);
            }
            else {

            }
        }
        if(arg_address == NULL) {
            arg_address = "127.0.0.1";
        }
        if(arg_port == NULL) {
            arg_port = "3535";
        }


        auto const address = asio::ip::make_address(arg_address);
        unsigned short port = static_cast<unsigned short>(std::atoi(arg_port));

        asio::io_context ioc{1};

        tcp::acceptor acceptor{ioc, {address, port}};
        tcp::socket socket{ioc};
        http_server(acceptor, socket);

        std::cout << "Starting server on http://" << address << ":" << port << std::endl;
        ioc.run();
    }
    catch(std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
