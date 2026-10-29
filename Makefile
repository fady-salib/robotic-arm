CC = gcc
CXX = g++
CPPFLAGS = -Ilibraries/ArmCommon/src
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror -g
CXXFLAGS = -std=c++11 -Wall -Wextra -Wpedantic -Werror -g
SANITIZERS ?= -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie
LOGIC = ArmProtocol Calibration HeartMonitor Kinematics EspIpd
OBJECTS = $(addprefix build/host/,$(addsuffix .o,$(LOGIC)))

.PHONY: test clean
test: build/host/test_logic build/host/test_receiver build/host/test_transport build/host/test_transmitter
	./build/host/test_logic
	./build/host/test_receiver
	./build/host/test_transport
	./build/host/test_transport ack-timeout
	./build/host/test_transport error
	./build/host/test_transmitter

build/host:
	mkdir -p $@

build/host/%.o: libraries/ArmCommon/src/%.c $(wildcard libraries/ArmCommon/src/*.h) | build/host
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZERS) -c $< -o $@

build/host/test_logic: tests/test_logic.c $(OBJECTS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZERS) $^ -lm -o $@

build/host/test_%: tests/test_%.cpp tests/mock_runtime.cpp libraries/ArmCommon/src/Esp01.cpp $(OBJECTS) $(wildcard tests/mocks/*.h) $(wildcard firmware/*/*.ino)
	$(CXX) $(CPPFLAGS) -Itests/mocks $(CXXFLAGS) $(SANITIZERS) $(filter %.cpp %.o,$^) -lm -o $@

clean:
	rm -rf build/host
