#include <format>
#include <random>
#include <fstream>
#include <iostream>
#include <thread>
#include <stdexcept>

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#define NX_BACKEND uint64_t
#include <nxsim/simulation.h>

nxon::parse_context setup_simulator(const std::string &command) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        throw std::runtime_error("pipe() failed");
    }
    
    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(open("/dev/null", O_WRONLY), STDERR_FILENO);
        close(pipefd[1]);
        
        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
        _exit(1);
    } else if (pid > 0) {
        close(pipefd[1]);
        
        std::string json;
        char buffer[4096];
        ssize_t bytes;
        
        while ((bytes = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            json.append(buffer, bytes);
        }
        
        close(pipefd[0]);
        int status;
        waitpid(pid, &status, 0);
        
        if (WEXITSTATUS(status) != 0) {
            throw std::runtime_error("NetX compilation failed");
        }
        
        if (!json.empty() && json.back() == '\n') {
            json.pop_back();
        }
        
        return nxon::parse_circuit(json);
    } else {
        close(pipefd[0]);
        close(pipefd[1]);
        throw std::runtime_error("fork() failed");
    }
}

template <typename Func>
decltype(auto) test(const std::string &name, Func &&stimuli) {
    const auto command = std::format(
        "nx compile _netx.toml --minimal --top {}", name);
    auto ctx = setup_simulator(command);

    using namespace std::chrono;
    const auto begin = steady_clock::now();
    std::invoke(std::forward<Func>(stimuli), ctx);
    const auto end = steady_clock::now();
    const duration<float> time = end - begin;
    std::cout << "Test passed in " << std::fixed << std::setprecision(3) << time.count() << " seconds: ";
    std::cout << name << std::endl;
}

#define CASE(component) void component(nxon::parse_context ctx)

using namespace nxon;
static auto high = value_t(1, 1);
static auto low = value_t(1, 0);

static void time_advance(parse_context &ctx, const std::string &name = "clk") {
    ctx.flip_by_name(name);
    ctx.flip_by_name(name);
}

namespace selector_test {
    CASE(mux_1_test) {
        ctx.update_by_name("t", {32, 114});
        ctx.update_by_name("b", {32, 514});
        ctx.update_by_name("cond", high);
        assert(ctx.get_by_name("out") == value_t(32, 114));
        ctx.update_by_name("cond", low);
        assert(ctx.get_by_name("out") == value_t(32, 514));
    }

    CASE(mux_5_test) {
        for (unsigned i = 0; i != 32; ++i) {
            ctx.update_by_name("cond", {5, i});
            assert(ctx.get_by_name("out") == value_t(32, i));
        }
    }

    CASE(mux_enum_test) {
        ctx.update_by_name("cond", {2, 0});
        assert(ctx.get_by_name("out") == value_t(2, 1));
        ctx.update_by_name("cond", {2, 1});
        assert(ctx.get_by_name("out") == value_t(2, 2));
        ctx.update_by_name("cond", {2, 2});
        assert(ctx.get_by_name("out") == value_t(2, 0));
    }

    CASE(demux_5_test) {
        ctx.update_by_name("input", {32, 114514});
        for (unsigned i = 0; i != 32; ++i) {
            ctx.update_by_name("cond", {5, i});
            assert(ctx.get_by_name(std::format("out[{}]", i)) == value_t(32, 114514));
        }
    }

    CASE(decoder_5_test) {
        for (unsigned i = 0; i != 32; ++i) {
            ctx.update_by_name("input", {5, i});
            assert(ctx.get_by_name("output") == value_t(32, 1 << i));
        }
    }

    CASE(encoder_5_test) {
        for (unsigned i = 0; i != 32; ++i) {
            ctx.update_by_name("input", value_t(32, 1 << i));
            assert(ctx.get_by_name("output") == value_t(5, i));
        }
    }

    CASE(priority_encoder_3_test) {
        for (unsigned i = 1; i != 256; ++i) {
            ctx.update_by_name("input", {8, i});
            const auto bit = 8 - (__builtin_clz(i) - 24 + 1);
            assert(ctx.get_by_name("output") == value_t(3, bit));
        }
    }

    CASE(arbiter_4_test) {
        for (unsigned i = 0; i != 16; ++i) {
            ctx.update_by_name("request", {4, i});
            // const auto bit = i == 0 ? 0 : __builtin_ctz(i);
            assert(ctx.get_by_name("grant") == value_t(4, i & ~(i - 1)));
        }
    }
}


namespace random_test {
    CASE(fib_lfsr_16_test) {
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

    CASE(galois_lfsr_16_test) {
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
    CASE(shift_reg_32_test) {
        unsigned value = 0x12345678;
        std::random_device rd;

        ctx.update_by_name("rst", high);
        ctx.update_by_name("rst_value", {32, value});
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

    CASE(reg_file_32_test) {
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

    CASE(sync_fifo_16_test) {
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
    CASE(barrel_shifter_32_test) {
        unsigned value = 0x12345678;
        std::random_device rd;

        for (int i = 0; i != 32768; ++i) {
            bool dir   = std::uniform_int_distribution<unsigned>(0, 1)(rd);
            bool arith = std::uniform_int_distribution<unsigned>(0, 1)(rd);
            unsigned shamt = std::uniform_int_distribution<unsigned>(0, 32)(rd);
            unsigned input = std::uniform_int_distribution<unsigned>(0, 0xFFFFFFFF)(rd);

            ctx.update_by_name("din", {32, input});
            ctx.update_by_name("shamt", {5, shamt});
            ctx.update_by_name("dir", {1, dir});
            ctx.update_by_name("arith", {1, arith});
            
            unsigned expected;
            if (dir) {
                if (arith) {
                    expected = ((signed) input) >> shamt;
                } else {
                    expected = input >> shamt;
                }
            } else {
                expected = (input << shamt);
            }
            assert (ctx.get_by_name("dout") == value_t(32, expected));
        }
    }

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

    CASE(ripple_carry_adder_32_test) {
        std::random_device rd;
        for (unsigned i = 0; i != 65536; ++i) {
            uint64_t a = std::uniform_int_distribution<uint64_t>(0, 0xFFFFFFFF)(rd);
            uint64_t b = std::uniform_int_distribution<uint64_t>(0, 0xFFFFFFFF)(rd);

            ctx.update_by_name("A", {32, a});
            ctx.update_by_name("B", {32, b});
            ctx.update_by_name("cin", low);
            assert (ctx.get_by_name("S") == value_t(32, a + b));
            assert (ctx.get_by_name("cout") == value_t(1, (a + b) >> 32));
        }
    }

    CASE(carry_lookahead_adder_32_test) {
        std::random_device rd;
        for (unsigned i = 0; i != 65536; ++i) {
            uint64_t a = std::uniform_int_distribution<uint64_t>(0, 0xFFFFFFFF)(rd);
            uint64_t b = std::uniform_int_distribution<uint64_t>(0, 0xFFFFFFFF)(rd);

            ctx.update_by_name("A", {32, a});
            ctx.update_by_name("B", {32, b});
            ctx.update_by_name("cin", low);
            assert (ctx.get_by_name("S") == value_t(32, a + b));
            assert (ctx.get_by_name("cout") == value_t(1, (a + b) >> 32));
        }
    }
}

#define TEST(package, component) test(#package "." #component, package::component)

int main() {
    TEST(selector_test, mux_1_test);
    TEST(selector_test, mux_5_test);
    TEST(selector_test, mux_enum_test);
    TEST(selector_test, demux_5_test);
    TEST(selector_test, decoder_5_test);
    TEST(selector_test, encoder_5_test);
    TEST(selector_test, priority_encoder_3_test);
    TEST(selector_test, arbiter_4_test);
    TEST(random_test, fib_lfsr_16_test);
    TEST(random_test, galois_lfsr_16_test);
    TEST(memory_test, shift_reg_32_test);
    TEST(memory_test, reg_file_32_test);
    TEST(memory_test, sync_fifo_16_test);
    TEST(arith_test, barrel_shifter_32_test);
    TEST(arith_test, sqrt32_test);
    TEST(arith_test, ripple_carry_adder_32_test);
    TEST(arith_test, carry_lookahead_adder_32_test);
    return 0;
}
