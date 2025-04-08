#include <format>
#include <fstream>
#include <iostream>

#include <boost/process.hpp>
#include "circuit/circuit_parser.h"

std::tuple<hxon::circuit, hxon::id_set, hxon::id_set> setup_simulator(const std::string &command) {
    boost::process::ipstream json;
    boost::process::child process(command,
        boost::process::std_out > json,
        boost::process::std_err > boost::process::null);
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

#define CASE(component) void component(hxon::circuit circuit, [[maybe_unused]] const hxon::id_set &inputs, [[maybe_unused]] const hxon::id_set &outputs)

static auto high = hxon::value_t(1, 1);
static auto low = hxon::value_t(1, 0);

namespace selector_test {
    CASE(mux_5) {
        for (int i = 0; i != 32; ++i) {
            circuit.update(*inputs.begin(), hxon::value_t::parse(std::format("5'd{}", i)));
            assert(circuit.get_value(*outputs.begin()) == hxon::value_t(32, i));
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
        circuit.flip_clocks(); circuit.flip_clocks();
        const auto values = new std::set<hxon::value_t>();
        circuit.update(1, low);
        for (int i = 0; i != 65536; ++i) {
            values->insert(circuit.get_value(2));
            circuit.flip_clocks(); circuit.flip_clocks();
        }
        assert(values->size() == 65535);
        delete values;
    }

    CASE(galois_lfsr_16) {
        circuit.update(1, high);
        circuit.flip_clocks(); circuit.flip_clocks();
        const auto values = new std::set<hxon::value_t>();
        circuit.update(1, low);
        for (int i = 0; i != 65536; ++i) {
            values->insert(circuit.get_value(2));
            circuit.flip_clocks(); circuit.flip_clocks();
        }
        assert(values->size() == 65535);
        delete values;
    }
}

namespace memory {
    CASE(sync_fifo_16) {

    }
}

#define TEST(package, component) test(#package "." #component, package::component)

int main() {
    TEST(selector_test, mux_5);
    TEST(selector_test, priority_encoder_3);
    TEST(random_test, fib_lfsr_16);
    TEST(random_test, galois_lfsr_16);
    return 0;
}
