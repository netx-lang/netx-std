BUILD_DIR := build
TARGET := $(BUILD_DIR)/std_test

MODE ?= release

ifeq ($(MODE),debug)
    CXXFLAGS := -g -O0 -std=c++23
else
    CXXFLAGS := -O3 -ffast-math -std=c++23
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
	c++ $(CXXFLAGS) -flto -o $@ $<
run: all
	$(TARGET)

clean:
	rm -f $(TARGET)
