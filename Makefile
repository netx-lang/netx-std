BUILD_DIR := build
TARGET := $(BUILD_DIR)/std_test

MODE ?= release

ifeq ($(MODE),debug)
    CXXFLAGS := -g -O0 -std=c++20
else
    CXXFLAGS := -O3 -Ofast -std=c++20
endif

.PHONY: all run clean debug release

all: $(TARGET)

debug:
	$(MAKE) MODE=debug

release:
	$(MAKE) MODE=release

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): main.cpp | $(BUILD_DIR)
	g++ $(CXXFLAGS) -o $@ $< -lnxsim

run: all
	$(TARGET)

clean:
	rm -f $(TARGET)