#ifndef ASYNC_WEBSERVER_HPP
#define ASYNC_WEBSERVER_HPP

#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>
#include <iostream>
#include <istream>
#include <memory>
#include <sstream> // Required for std::ostringstream
#include <string>
// need multiple threads to send and recieve change it to mulit threaded
#include <thread>
#include <vector>

// Since it's only used for heartbeat signals better to run on single
//'cause we neither use much of the cpu nor the bandwidth with just hearbeat signals

using asio::ip::tcp;

class HttpSession;

class HttpSession : public std::enable_shared_from_this<HttpSession>
{
      public:
        HttpSession(asio::io_context &io_context) : socket_(io_context), request_buffer_() {}

        tcp::socket &socket() { return socket_; }

        void start()
        {
                asio::async_read_until(socket_, request_buffer_, "\r\n\r\n",
                                       std::bind(&HttpSession::handle_read_request,
                                                 shared_from_this(), std::placeholders::_1,
                                                 std::placeholders::_2));
        }

      private:
        void handle_read_request(const asio::error_code &error, size_t bytes_transferred)
        {
                if (!error)
                {
                        std::istream request_stream(&request_buffer_);
                        std::string request_data;
                        std::string line;
                        std::ostringstream oss;
                        while (std::getline(request_stream, line) && line != "\r")
                        {
                                oss << line << "\n";
                        }
                        request_data = oss.str();
                        if (!request_data.empty() && request_data.back() == '\n')
                        {
                                request_data.pop_back();
                        }

                        std::cout << "Received request:\n---START---\n"
                                  << request_data << "---END---\n"
                                  << std::endl;

                        std::string response = "HTTP/1.1 200 OK\r\n"
                                               "Content-Type: text/plain\r\n"
                                               "Content-Length: 20\r\n"
                                               "\r\n"
                                               "Hello from ASIO!";

                        asio::async_write(socket_, asio::buffer(response),
                                          std::bind(&HttpSession::handle_write_response,
                                                    shared_from_this(), std::placeholders::_1,
                                                    std::placeholders::_2));
                }
                else if (error == asio::error::eof || error == asio::error::connection_reset)
                {
                        std::cout << "Client disconnected: " << error.message() << std::endl;
                }
                else
                {
                        std::cerr << "Error reading request: " << error.message() << std::endl;
                }
        }

        void handle_write_response(const asio::error_code &error, size_t /*bytes_transferred*/)
        {
                if (!error)
                {
                        std::cout << "Response sent successfully." << std::endl;
                }
                else
                {
                        std::cerr << "Error writing response: " << error.message() << std::endl;
                }

                asio::error_code ec;
                socket_.shutdown(tcp::socket::shutdown_both, ec);
                socket_.close(ec);
                if (ec)
                {
                        std::cerr << "Error closing socket: " << ec.message() << std::endl;
                }
        }

        tcp::socket socket_;
        asio::streambuf request_buffer_;
};

class TcpServer
{
      public:
        TcpServer(asio::io_context &io_context, short port)
            : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
        {
                start_accept();
        }

      private:
        void start_accept()
        {
                std::shared_ptr<HttpSession> new_session =
                    std::make_shared<HttpSession>(io_context_);

                acceptor_.async_accept(
                    new_session->socket(),
                    std::bind(&TcpServer::handle_accept, this, new_session, std::placeholders::_1));
        }

        void handle_accept(std::shared_ptr<HttpSession> new_session, const asio::error_code &error)
        {
                if (!error)
                {
                        std::cout << "Accepted connection from: "
                                  << new_session->socket().remote_endpoint() << std::endl;
                        new_session->start();
                }
                else
                {
                        std::cerr << "Error accepting connection: " << error.message() << std::endl;
                }

                start_accept();
        }

        asio::io_context &io_context_;
        tcp::acceptor acceptor_;
};

#endif // ASYNC_WEBSERVER_HPP
