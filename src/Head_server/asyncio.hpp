#pragma once

#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

namespace async_tcp_echo
{

using boost::asio::ip::tcp;
namespace asio = boost::asio;

// Represents a single client connection
class Session : public std::enable_shared_from_this<Session>
{
      public:
        explicit Session(tcp::socket socket) : socket_(std::move(socket)) {}

        void start() { read(); }

      private:
        void read()
        {
                auto self(shared_from_this());
                socket_.async_read_some(
                    asio::buffer(data_, max_length),
                    [this, self](boost::system::error_code ec, std::size_t length)
                    {
                            if (!ec)
                            {
                                    std::cout << "Received from " << socket_.remote_endpoint()
                                              << ": " << std::string(data_, length) << "\n";
                                    write(length);
                            }
                    });
        }

        void write(std::size_t length)
        {
                auto self(shared_from_this());
                asio::async_write(socket_, asio::buffer(data_, length),
                                  [this, self](boost::system::error_code ec, std::size_t)
                                  {
                                          if (!ec)
                                          {
                                                  read(); // Continue reading
                                          }
                                  });
        }

        tcp::socket socket_;
        enum
        {
                max_length = 1024
        };
        char data_[max_length];
};

// Accepts incoming connections and launches sessions
class Server
{
      public:
        Server(asio::io_context &io_context, unsigned short port)
            : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
        {
                accept();
        }

      private:
        void accept()
        {
                acceptor_.async_accept(
                    [this](boost::system::error_code ec, tcp::socket socket)
                    {
                            if (!ec)
                            {
                                    std::make_shared<Session>(std::move(socket))->start();
                            }
                            accept(); // Accept next
                    });
        }

        tcp::acceptor acceptor_;
};

// Runs the async TCP echo server on multiple threads
inline void run_echo_server(unsigned short port,
                            std::size_t thread_count = std::thread::hardware_concurrency())
{
        try
        {
                asio::io_context io_context;

                Server server(io_context, port);

                std::vector<std::thread> threads;
                for (std::size_t i = 0; i < thread_count; ++i)
                {
                        threads.emplace_back([&io_context]() { io_context.run(); });
                }

                std::cout << "Async TCP Echo Server running on port " << port << " with "
                          << thread_count << " threads.\n";

                for (auto &thread : threads)
                {
                        thread.join();
                }
        }
        catch (const std::exception &e)
        {
                std::cerr << "Server Exception: " << e.what() << "\n";
        }
}

} // namespace async_tcp_echo
