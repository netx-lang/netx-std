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

    std::invoke(std::forward<Func>(stimuli), circuit, inputs, outputs);
    std::cout << "Test " << name << " Passed." << std::endl;
}

#define CASE(component) void component(hxon::circuit circuit, const hxon::id_set &inputs, const hxon::id_set &outputs)

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

#define TEST(package, component) test(#package "." #component, package::component)

int main() {
    TEST(selector_test, mux_5);
    TEST(selector_test, priority_encoder_3);
    return 0;
}
