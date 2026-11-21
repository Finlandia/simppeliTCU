// Arduino String replacement backed by std::string.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>

class String {
public:
    String() = default;
    String(const char* s) : s_(s ? s : "") {}
    String(const std::string& s) : s_(s) {}
    explicit String(char c) : s_(1, c) {}
    explicit String(int v, unsigned char base = 10) : s_(fmtInt(v, base)) {}
    explicit String(unsigned int v, unsigned char base = 10) : s_(fmtUInt(v, base)) {}
    explicit String(long v, unsigned char base = 10) : s_(fmtInt(v, base)) {}
    explicit String(unsigned long v, unsigned char base = 10) : s_(fmtUInt(v, base)) {}
    explicit String(float v, unsigned int decimals = 2) : s_(fmtDouble(v, decimals)) {}
    explicit String(double v, unsigned int decimals = 2) : s_(fmtDouble(v, decimals)) {}

    const char* c_str() const { return s_.c_str(); }
    unsigned int length() const { return (unsigned int)s_.size(); }
    bool isEmpty() const { return s_.empty(); }
    float toFloat() const { return strtof(s_.c_str(), nullptr); }
    double toDouble() const { return strtod(s_.c_str(), nullptr); }
    long toInt() const { return strtol(s_.c_str(), nullptr, 10); }
    bool equals(const String& o) const { return s_ == o.s_; }
    bool startsWith(const String& p) const { return s_.compare(0, p.s_.size(), p.s_) == 0; }
    void trim();
    void toLowerCase();

    String& operator+=(const String& o) { s_ += o.s_; return *this; }
    String& operator+=(const char* o) { s_ += (o ? o : ""); return *this; }
    String& operator+=(char c) { s_ += c; return *this; }
    friend String operator+(const String& a, const String& b) { return String(a.s_ + b.s_); }
    bool operator==(const String& o) const { return s_ == o.s_; }
    bool operator==(const char* o) const { return s_ == (o ? o : ""); }
    bool operator!=(const String& o) const { return s_ != o.s_; }
    char operator[](unsigned int i) const { return i < s_.size() ? s_[i] : '\0'; }

    const std::string& str() const { return s_; }

private:
    static std::string fmtInt(long v, unsigned char base);
    static std::string fmtUInt(unsigned long v, unsigned char base);
    static std::string fmtDouble(double v, unsigned int decimals);
    std::string s_;
};
