#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <iostream>
#include <thread>
#include <vector>

// Define the port number for the server
const short PORT = 9669;

// Define the maximum buffer size for receiving data
const int MAX_BUFFER_SIZE = 2048;

// Define a class to handle individual client connections
class Session : public std::enable_shared_from_this<Session>
{
      public:
        // Constructor: Initializes the socket for the session
        Session(boost::asio::io_context &io_context) : socket_(io_context) {}

        // Get the socket associated with this session
        boost::asio::ip::tcp::socket &socket() { return socket_; }

        // Start the session: Begin asynchronous reading from the client
        void start()
        {
                // Asynchronously read data from the client into the buffer
                // The read_handler will be called upon completion
                socket_.async_read_some(boost::asio::buffer(data_, MAX_BUFFER_SIZE),
                                        boost::bind(&Session::handle_read, shared_from_this(),
                                                    boost::asio::placeholders::error,
                                                    boost::asio::placeholders::bytes_transferred));
        }

      private:
        // Handler for asynchronous read operations
        void handle_read(const boost::system::error_code &error, size_t bytes_transferred)
        {
                if (!error)
                {
                        // If no error, print the received data
                        std::cout << "Received from client (" << socket_.remote_endpoint() << "): "
                                  << std::string(data_.begin(), data_.begin() + bytes_transferred)
                                  << std::endl;

                        // Asynchronously write the received data back to the client (echo server)
                        // The write_handler will be called upon completion
                        boost::asio::async_write(
                            socket_, boost::asio::buffer(data_, bytes_transferred),
                            boost::bind(&Session::handle_write, shared_from_this(),
                                        boost::asio::placeholders::error));
                }
                else if (error == boost::asio::error::eof ||
                         error == boost::asio::error::connection_reset)
                {
                        // Client disconnected gracefully or reset the connection
                        std::cout << "Client disconnected: " << socket_.remote_endpoint()
                                  << std::endl;
                }
                else
                {
                        // Handle other errors during read
                        std::cerr << "Error during read: " << error.message() << std::endl;
                }
        }

        // Handler for asynchronous write operations
        void handle_write(const boost::system::error_code &error)
        {
                if (!error)
                {
                        // If no error, continue reading from the client (keep the session alive)
                        socket_.async_read_some(
                            boost::asio::buffer(data_, MAX_BUFFER_SIZE),
                            boost::bind(&Session::handle_read, shared_from_this(),
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred));
                }
                else
                {
                        // Handle errors during write
                        std::cerr << "Error during write: " << error.message() << std::endl;
                }
        }

        boost::asio::ip::tcp::socket socket_;    // The socket for this session
        std::array<char, MAX_BUFFER_SIZE> data_; // Buffer to hold received data
};

// Define a class for the TCP server
class Server
{
      public:
        // Constructor: Initializes the server with an io_context and port number
        Server(boost::asio::io_context &io_context, short port)
            : io_context_(io_context),
              acceptor_(io_context,
                        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
        {
                start_accept();
        }

      private:
        void start_accept()
        {
                // Create a new session for the incoming connection
                // std::make_shared is used for proper memory management with shared_ptr
                std::shared_ptr<Session> new_session = std::make_shared<Session>(io_context_);

                // Asynchronously accept a new connection
                // The handle_accept handler will be called upon completion
                acceptor_.async_accept(new_session->socket(),
                                       boost::bind(&Server::handle_accept, this, new_session,
                                                   boost::asio::placeholders::error));
        }

        // Handler for asynchronous accept operations
        void handle_accept(std::shared_ptr<Session> new_session,
                           const boost::system::error_code &error)
        {
                if (!error)
                {
                        std::cout << "Client connected: " << new_session->socket().remote_endpoint()
                                  << std::endl;
                        // Start the session to begin communication with the new client
                        new_session->start();
                }
                else
                {
                        std::cerr << "Error during accept: " << error.message() << std::endl;
                }

                // Continue accepting new connections
                start_accept();
        }

        boost::asio::io_context &io_context_;     // The io_context for asynchronous operations
        boost::asio::ip::tcp::acceptor acceptor_; // The acceptor for incoming connections
};

int start_socket()
{
        try
        {
                // Create an io_context. This is the core of Boost.Asio's asynchronous
                // operations. It acts as a bridge between the operating system's I/O
                // services and your application.
                boost::asio::io_context io_context;

                // Create a Server object, which will listen for incoming connections.
                // It binds to the specified port and starts accepting clients.
                Server server(io_context, PORT);

                // Determine the number of threads to use.
                const unsigned int num_threads = 2; // increase the threads according to users count
                std::cout << "Starting server on port " << PORT << " with " << num_threads
                          << " threads..." << std::endl;

                // Create a vector to hold the worker threads.
                std::vector<std::thread> threads;

                // Launch multiple threads to run the io_context.
                // Each thread will call io_context.run(), which blocks until all work is
                // done. In a server, it continuously dispatches completion handlers for
                // asynchronous operations.
                for (unsigned int i = 0; i < num_threads; ++i)
                {
                        threads.emplace_back(
                            [&io_context]()
                            {
                                    // Each thread runs its own io_context loop.
                                    // This allows Boost.Asio to dispatch completion handlers
                                    // concurrently.
                                    io_context.run();
                            });
                }

                // Wait for all worker threads to finish.
                // In a typical server application, this loop would run indefinitely.
                // For this example, it will keep the main thread alive while worker threads
                // handle connections.
                for (std::thread &t : threads)
                {
                        t.join();
                }
        }
        catch (std::exception &e)
        {
                std::cerr << "Exception: " << e.what() << std::endl;
        }

        return 0;
}
