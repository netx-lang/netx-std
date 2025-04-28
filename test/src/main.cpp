#include <format>
#include <random>
#include <fstream>
#include <iostream>

#include <boost/process.hpp>
#include "circuit/circuit_parser.h"

hxon::parse_context setup_simulator(const std::string &command) {
    std::string json;
    boost::process::ipstream out;
    boost::process::child process(command,
                                  boost::process::std_out > out,
                                  boost::process::std_err > boost::process::null);
    std::getline(out, json);
    process.wait();
    return hxon::parse_circuit(nlohmann::json::parse(json));
}

template <typename Func>
decltype(auto) test(const std::string &name, Func &&stimuli) {
    const auto command = std::format(
        "nx compile /home/nairen/workspace/netx_std/_netx.toml --minimal --top {}", name);
    auto ctx = setup_simulator(command);

    using namespace std::chrono;
    const auto begin = steady_clock::now();
    std::invoke(std::forward<Func>(stimuli), ctx);
    const auto end = steady_clock::now();
    const duration<float> time = end - begin;
    std::cout << "Test passed in " << std::fixed << std::setprecision(3) << time.count() << " seconds: ";
    std::cout << name << std::endl;
}

#define CASE(component) void component(hxon::parse_context ctx)

using namespace hxon;
static auto high = value_t(1, 1);
static auto low = value_t(1, 0);

static void time_advance(parse_context &ctx, const std::string &name = "clk") {
    ctx.flip_by_name(name);
    ctx.flip_by_name(name);
}

namespace selector_test {
    CASE(mux_1) {
        ctx.update_by_name("t", {32, 114});
        ctx.update_by_name("b", {32, 514});
        ctx.update_by_name("cond", high);
        assert(ctx.get_by_name("out") == value_t(32, 114));
        ctx.update_by_name("cond", low);
        assert(ctx.get_by_name("out") == value_t(32, 514));
    }

    CASE(mux_5) {
        for (unsigned i = 0; i != 32; ++i) {
            ctx.update_by_name("cond", {5, i});
            assert(ctx.get_by_name("out") == value_t(32, i));
        }
    }

    CASE(demux_5) {
        ctx.update_by_name("input", {32, 114514});
        for (unsigned i = 0; i != 32; ++i) {
            ctx.update_by_name("cond", {5, i});
            assert(ctx.get_by_name(std::format("out[{}]", i)) == value_t(32, 114514));
        }
    }

    CASE(decoder_5) {
        for (unsigned i = 0; i != 32; ++i) {
            ctx.update_by_name("input", {5, i});
            assert(ctx.get_by_name("output") == value_t(32, 1 << i));
        }
    }

    CASE(encoder_5) {
        for (unsigned i = 0; i != 32; ++i) {
            ctx.update_by_name("input", value_t(32, 1 << i));
            assert(ctx.get_by_name("output") == value_t(5, i));
        }
    }

    CASE(priority_encoder_3) {
        for (unsigned i = 1; i != 256; ++i) {
            ctx.update_by_name("input", {8, i});
            const auto bit = 8 - (__builtin_clz(i) - 24 + 1);
            assert(ctx.get_by_name("output") == value_t(3, bit));
        }
    }
}


namespace random_test {
    CASE(fib_lfsr_16) {
        ctx.update_by_name("rst", high);
        time_advance(ctx);
        const auto values = new std::set<value_t>();
        ctx.update_by_name("rst", low);
        for (int i = 0; i != 65536; ++i) {
            values->insert(ctx.get_by_name("output"));
            time_advance(ctx);
        }
        assert(values->size() == 65535);
        delete values;
    }

    CASE(galois_lfsr_16) {
        ctx.update_by_name("rst", high);
        time_advance(ctx);
        const auto values = new std::set<value_t>();
        ctx.update_by_name("rst", low);
        for (int i = 0; i != 65536; ++i) {
            values->insert(ctx.get_by_name("output"));
            time_advance(ctx);
        }
        assert(values->size() == 65535);
        delete values;
    }
}

namespace memory_test {
    CASE(shift_reg_32) {
        unsigned value = 0x12345678;
        std::random_device rd;

        ctx.update_by_name("rst", high);
        ctx.update_by_name("rst_in", {32, value});
        time_advance(ctx);
        assert (ctx.get_by_name("dout") == value_t(32, value));
        ctx.update_by_name("rst", low);
        for (int i = 0; i != 128; ++i) {
            const bool isHigh = std::uniform_int_distribution(0, 1)(rd);
            ctx.update_by_name("din", isHigh ? high : low);
            value = value << 1 | isHigh;
            time_advance(ctx);
            assert (ctx.get_by_name("dout") == value_t(32, value));
        }
    }

    CASE(reg_file_32) {
        ctx.update_by_name("rst", high); time_advance(ctx);
        ctx.update_by_name("rst", low);
        ctx.update_by_name("wr_en", high);
        for (unsigned i = 0; i != 32; ++i) {
            ctx.update_by_name("wr_addr", {5, i});
            ctx.update_by_name("din", {32, i});
            time_advance(ctx);
        }
        ctx.update_by_name("wr_en", low); // disable write
        for (unsigned i = 0; i != 32; ++i) {
            ctx.update_by_name("rd_addr_a", {5, i});
            ctx.update_by_name("rd_addr_b", {5, 32 - i - 1});
            assert (ctx.get_by_name("dout_a") == value_t(32, i));
            assert (ctx.get_by_name("dout_b") == value_t(32, 32 - i - 1));
        }
    }

    CASE(sync_fifo_16) {
        ctx.update_by_name("rst", high); time_advance(ctx);
        ctx.update_by_name("rst", low);
        ctx.update_by_name("wr_en", high);
        ctx.update_by_name("rd_en", low);
        for (unsigned i = 1; i <= 20; ++i) {
            ctx.update_by_name("din", {32, i});
            time_advance(ctx);
            if (i >= 15) {
                assert(ctx.get_by_name("full") == high);
            }
        }
        ctx.update_by_name("wr_en", low);
        ctx.update_by_name("rd_en", high);
        for (int i = 1; i <= 20; ++i) {
            time_advance(ctx);
            if (i >= 15) {
                assert(ctx.get_by_name("empty") == high);
            } else {
                assert(ctx.get_by_name("dout") == value_t(32, i));
            }
        }

        ctx.update_by_name("rst", high); time_advance(ctx);
        ctx.update_by_name("rst", low);
        ctx.update_by_name("wr_en", high);
        ctx.update_by_name("rd_en", high);
        for (unsigned i = 1; i <= 30; ++i) {
            ctx.update_by_name("din", {32, i});
            time_advance(ctx);
            assert(ctx.get_by_name("dout") == value_t(32, i - 1));
        }
    }
}

namespace arith_test {
    CASE(sqrt32_test) {
        for (unsigned i = 0; i != 65536; ++i) {
            ctx.update_by_name("D", {32, i * i});
            ctx.update_by_name("rst", high); time_advance(ctx);
            ctx.update_by_name("rst", low);
            for (int j = 0; j != 16; ++j) {
                time_advance(ctx);
            }
            assert (ctx.get_by_name("Q") == value_t(16, i));
        }
    }
}

#define TEST(package, component) test(#package "." #component, package::component)

int main() {
    TEST(selector_test, mux_1);
    TEST(selector_test, mux_5);
    TEST(selector_test, demux_5);
    TEST(selector_test, decoder_5);
    TEST(selector_test, encoder_5);
    TEST(selector_test, priority_encoder_3);
    TEST(random_test, fib_lfsr_16);
    TEST(random_test, galois_lfsr_16);
    TEST(memory_test, shift_reg_32);
    TEST(memory_test, reg_file_32);
    TEST(memory_test, sync_fifo_16);
    TEST(arith_test, sqrt32_test);
    return 0;
}
