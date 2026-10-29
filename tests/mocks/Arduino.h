#ifndef TEST_ARDUINO_H
#define TEST_ARDUINO_H
#include <cstdint>
#include <cstring>
#include <deque>
#include <string>
#define F(s) reinterpret_cast<const __FlashStringHelper *>(s)
struct __FlashStringHelper;
constexpr int HIGH = 1, LOW = 0, INPUT = 0, OUTPUT = 1;
constexpr uint8_t A0=14, A1=15, A2=16, A3=17, A4=18, A5=19, A6=20;
#define NUM_ANALOG_INPUTS 8
extern uint32_t fake_ms;
extern int analog_values[32], digital_values[32];
extern bool fake_interrupts_enabled;
inline void noInterrupts() { fake_interrupts_enabled = false; }
inline void interrupts() { fake_interrupts_enabled = true; }
inline uint32_t millis() { return fake_ms; }
inline void delay(uint32_t n) { fake_ms += n; }
inline int analogRead(uint8_t pin) { return analog_values[pin]; }
inline int digitalRead(uint8_t pin) { return digital_values[pin]; }
inline void digitalWrite(uint8_t pin, int value) { digital_values[pin] = value; }
inline void pinMode(uint8_t, int) {}
struct FakeSerial {
    std::deque<uint8_t> rx;
    std::string tx;
    int capacity = 64;
    void begin(uint32_t) {}
    int available() const { return static_cast<int>(rx.size()); }
    int availableForWrite() const { return capacity; }
    int read() { if (rx.empty()) return -1; int c=rx.front(); rx.pop_front(); return c; }
    void print(const __FlashStringHelper *s) { tx += reinterpret_cast<const char *>(s); }
    void print(const char *s) { tx += s; }
    void print(char c) { tx += c; }
    void print(unsigned int n) { tx += std::to_string(n); }
    void print(int n) { tx += std::to_string(n); }
    void print(uint8_t n) { tx += std::to_string(n); }
    void write(const uint8_t *p, size_t n) { tx.append(reinterpret_cast<const char *>(p), n); }
    void inject(const std::string &s) { for (unsigned char c:s) rx.push_back(c); }
};
extern FakeSerial Serial;
#endif
