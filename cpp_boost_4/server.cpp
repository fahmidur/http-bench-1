//------------------------------------------------------------------------------
//
// Example: HTTP server, fast
//
//------------------------------------------------------------------------------

#include "fields_alloc.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <list>
#include <memory>
#include <string>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace asio = boost::asio;            // from <boost/asio.hpp>
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

// Return a reasonable mime type based on the extension of a file.
// beast::string_view
// mime_type(beast::string_view path) {
//     using beast::iequals;
//     auto const ext = [&path]{
//         auto const pos = path.rfind(".");
//         if(pos == beast::string_view::npos)
//             return beast::string_view{};
//         return path.substr(pos);
//     }();
//     if(iequals(ext, ".htm"))  return "text/html";
//     if(iequals(ext, ".html")) return "text/html";
//     if(iequals(ext, ".php"))  return "text/html";
//     if(iequals(ext, ".css"))  return "text/css";
//     if(iequals(ext, ".txt"))  return "text/plain";
//     if(iequals(ext, ".js"))   return "application/javascript";
//     if(iequals(ext, ".json")) return "application/json";
//     if(iequals(ext, ".xml"))  return "application/xml";
//     if(iequals(ext, ".swf"))  return "application/x-shockwave-flash";
//     if(iequals(ext, ".flv"))  return "video/x-flv";
//     if(iequals(ext, ".png"))  return "image/png";
//     if(iequals(ext, ".jpe"))  return "image/jpeg";
//     if(iequals(ext, ".jpeg")) return "image/jpeg";
//     if(iequals(ext, ".jpg"))  return "image/jpeg";
//     if(iequals(ext, ".gif"))  return "image/gif";
//     if(iequals(ext, ".bmp"))  return "image/bmp";
//     if(iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
//     if(iequals(ext, ".tiff")) return "image/tiff";
//     if(iequals(ext, ".tif"))  return "image/tiff";
//     if(iequals(ext, ".svg"))  return "image/svg+xml";
//     if(iequals(ext, ".svgz")) return "image/svg+xml";
//     return "application/text";
// }

class http_worker {
public:
    http_worker(http_worker const&) = delete;
    http_worker& operator=(http_worker const&) = delete;

    http_worker(tcp::acceptor& acceptor) :
        acceptor_(acceptor)
    {
    }

    void start() {
        accept();
        check_deadline();
    }

private:
    using alloc_t = fields_alloc<char>;
    //using request_body_t = http::basic_dynamic_body<beast::flat_static_buffer<1024 * 1024>>;
    using request_body_t = http::string_body;

    // The acceptor used to listen for incoming connections.
    tcp::acceptor& acceptor_;

    // The socket for the currently connected client.
    tcp::socket socket_{acceptor_.get_executor()};

    // The buffer for performing reads
    beast::flat_static_buffer<8192> buffer_;

    // The allocator used for the fields in the request and reply.
    alloc_t alloc_{8192};

    // The parser for reading the requests
    boost::optional<http::request_parser<request_body_t, alloc_t>> parser_;

    // The timer putting a time limit on requests.
    asio::steady_timer request_deadline_{
        acceptor_.get_executor(), (std::chrono::steady_clock::time_point::max)()};

    // The string-based response message.
    boost::optional<http::response<http::string_body, http::basic_fields<alloc_t>>> string_response_;

    // The string-based response serializer.
    boost::optional<http::response_serializer<http::string_body, http::basic_fields<alloc_t>>> string_serializer_;

    // The file-based response message.
    boost::optional<http::response<http::file_body, http::basic_fields<alloc_t>>> file_response_;

    // The file-based response serializer.
    boost::optional<http::response_serializer<http::file_body, http::basic_fields<alloc_t>>> file_serializer_;

    void accept() {
        // Clean up any previous connection.
        beast::error_code ec;
        socket_.close(ec);
        buffer_.consume(buffer_.size());

        acceptor_.async_accept(
            socket_,
            [this](beast::error_code ec) {
                if (ec) {
                    accept();
                } else {
                    // Request must be fully processed within 60 seconds.
                    request_deadline_.expires_after(
                        std::chrono::seconds(60));

                    read_request();
                }
            });
    }

    void read_request() {
        // On each read the parser needs to be destroyed and
        // recreated. We store it in a boost::optional to
        // achieve that.
        //
        // Arguments passed to the parser constructor are
        // forwarded to the message object. A single argument
        // is forwarded to the body constructor.
        //
        // We construct the dynamic body with a 1MB limit
        // to prevent vulnerability to buffer attacks.
        //
        parser_.emplace(
            std::piecewise_construct,
            std::make_tuple(),
            std::make_tuple(alloc_)
        );

        http::async_read(
            socket_,
            buffer_,
            *parser_,
            [this](beast::error_code ec, std::size_t) {
                if (ec) {
                    accept();
                } else {
                    process_request(parser_->get());
                }
            }
        );
    }

    void process_request(http::request<request_body_t, http::basic_fields<alloc_t>> const& req) {
        switch (req.method()) {
        case http::verb::get:
            // send_file(req.target());
            create_response(req);
            break;
        default:
            // We return responses indicating an error if
            // we do not recognize the request method.
            send_bad_response(
                http::status::bad_request,
                "Invalid request-method '" + std::string(req.method_string()) + "'\r\n"
            );
            break;
        }
    }

    void create_response(http::request<request_body_t, http::basic_fields<alloc_t>> const& req) {
        if(req.target() == "/time") {
            send_time_page();
        }
        else {
            send_bad_response(
                http::status::bad_request,
                "Invalid request-method '" + std::string(req.method_string()) + "'\r\n"
            );
        }
    }

    void send_time_page() {
        string_response_.emplace(
            std::piecewise_construct,
            std::make_tuple(),
            std::make_tuple(alloc_)
        );
        string_response_->result(http::status::ok);
        string_response_->keep_alive(false);
        string_response_->set(http::field::server, "Beast");
        string_response_->set(http::field::content_type, "text/html");

        std::stringstream body_stream;
        body_stream 
            <<  "<html>\n"
            <<  "<head><title>Current time</title></head>\n"
            <<  "<body>\n"
            <<  "<h1>Current time</h1>\n"
            <<  "<p>The current time is "
            <<  my_program_state::now()
            <<  " seconds since the epoch.</p>\n"
            <<  "</body>\n"
            <<  "</html>\n";
        string_response_->body() = body_stream.str();

        string_response_->prepare_payload();
        string_serializer_.emplace(*string_response_);
        http::async_write(
            socket_,
            *string_serializer_,
            [this](beast::error_code ec, std::size_t) {
                socket_.shutdown(tcp::socket::shutdown_send, ec);
                string_serializer_.reset();
                string_response_.reset();
                accept();
            }
        );
    }

    void send_bad_response(http::status status, std::string const& error) {
        string_response_.emplace(
            std::piecewise_construct,
            std::make_tuple(),
            std::make_tuple(alloc_)
        );

        string_response_->result(status);
        string_response_->keep_alive(false);
        string_response_->set(http::field::server, "Beast");
        string_response_->set(http::field::content_type, "text/plain");
        string_response_->body() = error;
        string_response_->prepare_payload();

        string_serializer_.emplace(*string_response_);

        http::async_write(
            socket_,
            *string_serializer_,
            [this](beast::error_code ec, std::size_t) {
                socket_.shutdown(tcp::socket::shutdown_send, ec);
                string_serializer_.reset();
                string_response_.reset();
                accept();
            }
        );
    }

    // void send_file(beast::string_view target) {
    //     // Request path must be absolute and not contain "..".
    //     if (target.empty() || target[0] != '/' || target.find("..") != std::string::npos) {
    //         send_bad_response(
    //             http::status::not_found,
    //             "File not found\r\n"
    //         );
    //         return;
    //     }

    //     std::string full_path = doc_root_;
    //     full_path.append(
    //         target.data(),
    //         target.size()
    //     );

    //     http::file_body::value_type file;
    //     beast::error_code ec;
    //     file.open(
    //         full_path.c_str(),
    //         beast::file_mode::read,
    //         ec
    //     );
    //     if(ec) {
    //         send_bad_response(
    //             http::status::not_found,
    //             "File not found\r\n"
    //         );
    //         return;
    //     }

    //     file_response_.emplace(
    //         std::piecewise_construct,
    //         std::make_tuple(),
    //         std::make_tuple(alloc_)
    //     );

    //     file_response_->result(http::status::ok);
    //     file_response_->keep_alive(false);
    //     file_response_->set(http::field::server, "Beast");
    //     file_response_->set(http::field::content_type, mime_type(std::string(target)));
    //     file_response_->body() = std::move(file);
    //     file_response_->prepare_payload();

    //     file_serializer_.emplace(*file_response_);

    //     http::async_write(
    //         socket_,
    //         *file_serializer_,
    //         [this](beast::error_code ec, std::size_t) {
    //             socket_.shutdown(tcp::socket::shutdown_send, ec);
    //             file_serializer_.reset();
    //             file_response_.reset();
    //             accept();
    //         }
    //     );
    // }

    void check_deadline() {
        // The deadline may have moved, so check it has really passed.
        if (request_deadline_.expiry() <= std::chrono::steady_clock::now()) {
            // Close socket to cancel any outstanding operation.
            socket_.close();

            // Sleep indefinitely until we're given a new deadline.
            request_deadline_.expires_at((std::chrono::steady_clock::time_point::max)());
        }

        request_deadline_.async_wait(
            [this](beast::error_code) {
                check_deadline();
            }
        );
    }
};

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

        int num_workers = std::thread::hardware_concurrency();

        asio::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {address, port}};

        std::list<http_worker> workers;
        for (int i = 0; i < num_workers; ++i) {
            workers.emplace_back(acceptor);
            workers.back().start();
        }

        std::cout << "Starting server on http://" << address << ":" << port << std::endl;
        ioc.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
