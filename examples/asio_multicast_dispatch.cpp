#include <vitality/vitality.hpp>

// Assume standalone ASIO single-header is available in your include path.
#include <asio.hpp>

#include <array>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

class MulticastReceiver {
public:
    MulticastReceiver(asio::io_context& io,
                      const asio::ip::address& listen_address,
                      const asio::ip::address& multicast_address,
                      unsigned short port)
        : socket_(io) {
        asio::ip::udp::endpoint listen_endpoint(listen_address, port);

        socket_.open(listen_endpoint.protocol());
        socket_.set_option(asio::ip::udp::socket::reuse_address(true));
        socket_.bind(listen_endpoint);

        socket_.set_option(asio::ip::multicast::join_group(multicast_address));
        start_receive();
    }

private:
    void start_receive() {
        socket_.async_receive_from(
            asio::buffer(buffer_),
            sender_endpoint_,
            [this](const std::error_code& ec, std::size_t bytes_received) {
                if (!ec) {
                    auto packet_bytes = vita::bytes_view(
                        reinterpret_cast<const vita::byte*>(buffer_.data()),
                        bytes_received);

                    vita::packet::dispatch(
                        packet_bytes,
                        [this](const vita::view::signal& signal_view) {
                            on_signal(signal_view);
                        },
                        [this](const vita::view::context& context_view) {
                            on_context(context_view);
                        });
                } else {
                    std::cerr << "receive error: " << ec.message() << "\n";
                }

                start_receive();
            });
    }

    static void on_signal(const vita::view::signal& view) {
        std::cout << "signal packet"
                  << " stream_id=0x" << std::hex << view.stream_id().value_or(0u) << std::dec
                  << " payload_bytes=" << view.payload().size() << "\n";
    }

    static void on_context(const vita::view::context& view) {
        std::cout << "context packet"
                  << " stream_id=0x" << std::hex << view.stream_id().value_or(0u) << std::dec
                  << " sample_rate=" << (view.has_sample_rate_sps() ? "present" : "missing")
                  << " temperature=" << (view.has_temperature_celsius() ? "present" : "missing")
                  << "\n";
    }

    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint sender_endpoint_;
    std::array<std::byte, 2048> buffer_{};
};

int main(int argc, char** argv) {
    const std::string listen_ip = (argc > 1) ? argv[1] : "0.0.0.0";
    const std::string multicast_ip = (argc > 2) ? argv[2] : "239.255.0.1";
    const unsigned short port = static_cast<unsigned short>((argc > 3) ? std::stoi(argv[3]) : 50000);

    asio::io_context io;
    MulticastReceiver receiver(
        io,
        asio::ip::make_address(listen_ip),
        asio::ip::make_address(multicast_ip),
        port);

    std::cout << "Listening for UDP multicast on " << multicast_ip << ":" << port
              << " via " << listen_ip << "\n";
    io.run();
    return 0;
}
