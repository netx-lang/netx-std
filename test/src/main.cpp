#include <format>
#include <random>
#include <fstream>
#include <iostream>

#include <boost/process.hpp>
#include "circuit/circuit_parser.h"

std::tuple<hxon::circuit, hxon::id_set, hxon::id_set> setup_simulator(const std::string &command) {
    std::string json;
    boost::process::ipstream out;
    boost::process::child process(command,
                                  boost::process::std_out > out,
                                  boost::process::std_err > boost::process::null);
    std::getline(out, json);
    process.wait();
    auto context = hxon::parse_circuit(nlohmann::json::parse(json));
    return {context.circuit, context.circuit_input, context.circuit_output};
}

template <typename Func>
decltype(auto) test(const std::string &name, Func &&stimuli) {
    const auto command = std::format(
        "nx compile /home/nairen/workspace/netx_std/_netx.toml --minimal --top {}", name);
    auto [circuit, inputs, outputs] = setup_simulator(command);

    using namespace std::chrono;
    const auto begin = steady_clock::now();
    std::invoke(std::forward<Func>(stimuli), circuit, inputs, outputs);
    const auto end = steady_clock::now();
    const duration<float> time = end - begin;
    std::cout << "Test passed in " << std::fixed << std::setprecision(3) << time.count() << " seconds: ";
    std::cout << name << std::endl;
}

#define CASE(component) void component(hxon::circuit &circuit, [[maybe_unused]] const hxon::id_set &inputs, [[maybe_unused]] const hxon::id_set &outputs)

static auto high = hxon::value_t(1, 1);
static auto low = hxon::value_t(1, 0);

static void time_advance(hxon::circuit &circuit) {
    circuit.flip(circuit.all_clocks());
    circuit.flip(circuit.all_clocks());
}


namespace selector_test {
    CASE(mux_5) {
        for (int i = 0; i != 32; ++i) {
            circuit.update(*inputs.begin(), hxon::value_t::parse(std::format("5'd{}", i)));
            assert(circuit.get_value(*outputs.begin()) == hxon::value_t(32, i));
        }
    }

    CASE(decoder_5) {
        for (int i = 0; i != 32; ++i) {
            circuit.update(*inputs.begin(), hxon::value_t::parse(std::format("5'd{}", i)));
            const int expected = 1 << i;
            assert(circuit.get_value(*outputs.begin()) == hxon::value_t(32, expected));
        }
    }

    CASE(priority_encoder_3) {
        for (int i = 1; i != 256; ++i) {
            circuit.update(*inputs.begin(), hxon::value_t::parse(std::format("8'd{}", i)));
            const auto bit = 8 - (__builtin_clz(i) - 24 + 1);
            assert(circuit.get_value(*outputs.begin()) == hxon::value_t(3, bit));
        }
    }
}


namespace random_test {
    CASE(fib_lfsr_16) {
        circuit.update(1, high);
        circuit.flip(circuit.all_clocks());
        circuit.flip(circuit.all_clocks());
        const auto values = new std::set<hxon::value_t>();
        circuit.update(1, low);
        for (int i = 0; i != 65536; ++i) {
            values->insert(circuit.get_value(2));
            circuit.flip(circuit.all_clocks());
            circuit.flip(circuit.all_clocks());
        }
        assert(values->size() == 65535);
        delete values;
    }

    CASE(galois_lfsr_16) {
        circuit.update(1, high);
        circuit.flip(circuit.all_clocks());
        circuit.flip(circuit.all_clocks());
        const auto values = new std::set<hxon::value_t>();
        circuit.update(1, low);
        for (int i = 0; i != 65536; ++i) {
            values->insert(circuit.get_value(2));
            circuit.flip(circuit.all_clocks());
            circuit.flip(circuit.all_clocks());
        }
        assert(values->size() == 65535);
        delete values;
    }
}

namespace memory_test {
    CASE(shift_reg_32) {
        int value = 0x12345678;
        std::random_device rd;

        circuit.update(1, high);
        circuit.update(2, hxon::value_t::parse(std::format("32'd{}", value)));
        time_advance(circuit);
        assert (circuit.get_value(4) == hxon::value_t::parse(std::format("32'd{}", value)));
        circuit.update(1, low);
        for (int i = 0; i != 128; ++i) {
            const bool isHigh = std::uniform_int_distribution(0, 1)(rd);
            circuit.update(3, isHigh ? high : low);
            value = value << 1 | isHigh;
            time_advance(circuit);
            assert (circuit.get_value(4) == hxon::value_t::parse(std::format("32'd{}", value)));
        }
    }

    CASE(reg_file_32) {
        circuit.update(1, high); time_advance(circuit); circuit.update(1, low); // reset
        circuit.update(4, high); // enable write
        for (int i = 0; i != 32; ++i) {
            circuit.update(5, hxon::value_t::parse(std::format("5'd{}", i))); // wr_addr
            circuit.update(6, hxon::value_t::parse(std::format("32'd{}", i))); // din
            time_advance(circuit);
        }
        circuit.update(4, low); // disable write
        for (int i = 0; i != 32; ++i) {
            circuit.update(2, hxon::value_t::parse(std::format("5'd{}", i)));
            circuit.update(3, hxon::value_t::parse(std::format("32'd{}", 32 - i - 1)));
            assert (circuit.get_value(7) == hxon::value_t::parse(std::format("32'd{}", i)));
            assert (circuit.get_value(8) == hxon::value_t::parse(std::format("32'd{}", 32 - i - 1)));
        }
    }

    CASE(sync_fifo_16) {
        circuit.update(1, high); time_advance(circuit); circuit.update(1, low); // reset
        circuit.update(2, high); // enable write
        circuit.update(3, low);  // disable read
        for (int i = 1; i <= 20; ++i) {
            circuit.update(4, hxon::value_t::parse(std::format("32'd{}", i)));
            time_advance(circuit);
            if (i >= 15) {
                assert(circuit.get_value(7) == high); // full
            }
        }
        circuit.update(2, low);  // disable write
        circuit.update(3, high); // enable read
        for (int i = 1; i <= 20; ++i) {
            time_advance(circuit);
            if (i >= 15) {
                assert(circuit.get_value(6) == high); // empty
            } else {
                assert(circuit.get_value(5) == hxon::value_t::parse(std::format("32'd{}", i)));
            }
        }

        circuit.update(1, high); time_advance(circuit); circuit.update(1, low); // reset
        circuit.update(2, high); // enable write
        circuit.update(3, high); // enable read
        for (int i = 1; i <= 30; ++i) {
            circuit.update(4, hxon::value_t::parse(std::format("32'd{}", i)));
            time_advance(circuit);
            assert(circuit.get_value(5) == hxon::value_t::parse(std::format("32'd{}", i - 1)));
        }
    }
}

namespace arith_test {
    CASE(sqrt32_test) {
        circuit.update(1, high);
        time_advance(circuit);
        circuit.update(1, low);

        for (unsigned int i = 0; i != 65536; ++i) {
            circuit.update(2, hxon::value_t::parse(std::format("32'd{}", i * i)));
            circuit.update(1, high); time_advance(circuit); circuit.update(1, low);
            for (int j = 0; j != 16; ++j) {
                time_advance(circuit);
            }
            assert (circuit.get_value(3) == hxon::value_t::parse(std::format("16'd{}", i)));
        }
    }
}

#define TEST(package, component) test(#package "." #component, package::component)

int main() {
    TEST(selector_test, mux_5);
    TEST(selector_test, decoder_5);
    TEST(selector_test, priority_encoder_3);
    TEST(random_test, fib_lfsr_16);
    TEST(random_test, galois_lfsr_16);
    TEST(memory_test, shift_reg_32);
    TEST(memory_test, reg_file_32);
    TEST(memory_test, sync_fifo_16);
    TEST(arith_test, sqrt32_test);
    return 0;
}
