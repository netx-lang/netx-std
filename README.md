# NetX Standard Library

A collection of reusable hardware components for digital design using NetX language.

## Overview

The NetX Standard Library provides a set of hardware components that can be used as building blocks in NetX designs. The library includes components for arithmetic operations, memory management, random number generation, and data selection/routing.

## Components

### Arithmetic (`src/arith.nx`)
- SQRT32: 32-bit square root calculator using non-restoring algorithm

### Memory (`src/memory.nx`)
- SHIFT_REG: Configurable-width shift register with reset
- REG_FILE: Dual-port register file with single write port
- SYNC_FIFO: Synchronous FIFO buffer with status flags

### Random Number Generation (`src/random.nx`)
- FIB_LFSR: Fibonacci Linear Feedback Shift Register
- GALOIS_LFSR: Galois Linear Feedback Shift Register

### Selector/Logic (`src/selector.nx`)
- MUX: Configurable multiplexer (2^n-to-1)
- DEMUX: Configurable demultiplexer (1-to-2^n)
- DECODER: Binary-to-one-hot decoder
- ENCODER: One-hot-to-binary encoder
- PRIORITY_ENCODER: Priority encoder with highest-index precedence

### Utilities (`src/utils.nx`)
- REG: Configurable register with synchronous reset
- HEX_TO_7SEG: Hexadecimal to 7-segment display decoder

## Project Structure

```
├── _netx.toml         # Project configuration
├── Makefile           # Build system
├── main.cpp           # Test runner
├── src/               # Library source files
│   ├── arith.nx       # Arithmetic components
│   ├── memory.nx      # Memory components
│   ├── random.nx      # Random number generators
│   ├── selector.nx    # Multiplexers and logic
│   └── utils.nx       # Utility components
└── test/              # Test files
    ├── arith_test.nx
    ├── memory_test.nx
    ├── random_test.nx
    └── selector_test.nx
```

## Building and Testing

### Prerequisites

- [NetX compiler and simulator](https://github.com/pascal-lab/NetX)
- C++23 compatible compiler (clang++ recommended)
- POSIX-compatible system (Linux, macOS, BSD)
- nxsim simulation library

### Build Commands

```bash
# Build in release mode (default)
make

# Build in debug mode
make debug

# Run tests
make run

# Clean build artifacts
make clean
```

### Build Modes

- Release Mode: Optimized build with `-O3 -ffast-math` flags
- Debug Mode: Debug build with `-g -O0` flags for development

## Usage

### Including the Library

Add the NetX Standard Library to your project's `_netx.toml`:

```toml
[dependencies]
std = { version = "0.1.0", git = "git@github.com:pascal-lab/NetX-std.git" }
```

### Importing Components

```netx
// Import specific components
import utils.REG;
import selector.MUX;
import memory.SYNC_FIFO;

// Import all components from a module
import arith.*;
```

### Example Usage

```netx
// Using a register with positive edge trigger and active high reset
component MyDesign : [clk, rst, data_in] -> [data_out] {
  wire clk of clock();
  wire rst of bit(1);
  wire data_in, data_out of bit(8);

  data_out <> REG(pos_edge=true, high_rst=true, rst_value=8'd0) <> [clk, rst, data_in];
}

// Using a 4-to-1 multiplexer
component Selector : [sel, in0, in1, in2, in3] -> [out] {
  wire sel of bit(2);
  auto in0, in1, in2, in3, out;
  
  out <> MUX(2) <> [sel, in0, in1, in2, in3];
}
```

## Testing

The library includes comprehensive test suites for all components:

- arith_test.nx: Tests for arithmetic components
- memory_test.nx: Tests for memory components  
- random_test.nx: Tests for random number generators
- selector_test.nx: Tests for selector/logic components

Tests are automatically run using the C++ test runner in `main.cpp`.