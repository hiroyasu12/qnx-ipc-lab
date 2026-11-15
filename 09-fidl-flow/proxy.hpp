#pragma once
#include <cstdint>

class CalculatorProxy {
public:
    explicit CalculatorProxy(int coid) : coid_(coid) {}
    bool add(std::int32_t left, std::int32_t right, std::int64_t& result) const;
private:
    int coid_;
};
