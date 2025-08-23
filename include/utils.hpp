#pragma once
#include <systemc>
#include <string>
#include <random>

class Randomizer {
public:
    Randomizer(int minVal, int maxVal) : distrib(minVal, maxVal), gen(std::random_device{}()) {}

    int nextInt() {
        return distrib(gen);
    }

    int nextInt(int minVal, int maxVal) {
        std::uniform_int_distribution<> temp_distrib(minVal, maxVal);
        return temp_distrib(gen);
    }

private:
    std::uniform_int_distribution<> distrib;
    std::mt19937 gen;
};