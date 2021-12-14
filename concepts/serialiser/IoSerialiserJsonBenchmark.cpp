#include "IoSerialiserBenchmark.hpp"
#include <IoSerialiserJson.hpp>

using namespace opencmw;
using namespace opencmw::utils; // for operator<< and fmt::format overloading

/**
 * for comparison (C++/POCO variant ~ 90% feature-complete: YaS header missing, ...):
 * Java:
 * IO Serializer (custom) throughput = 4.3 GB/s for 29.7 kB per test run (took 698.0 ms)
 * IO Serializer (POJO) throughput = 2.5 GB/s for 29.1 kB per test run (took 1172.0 ms)
 * C++:
 * IO Serializer (POCO) throughput = 6.6 GB/s for 29.5 kB per test run (took 447.2 ms)
 *
 */
int main() {
    using namespace opencmw;
    IoBuffer      buffer;
    TestDataClass data(10, 10, 0);
    TestDataClass data2;
    data2.byte1 = 30;

    fmt::print("IoSerialiserJsonBenchmark - check identity - nBytes = {}\n", checkSerialiserIdentity<Json>(buffer, data, data2));

    TestDataClass testData(1000, 0);  // numeric heavy data <-> equivalent to Java benchmark
    const int     nIterations = 1000; // 100000
    fmt::print("Json performance with strong checks (exceptions if necessary):\n");
    for (int i = 0; i < 10; i++) {
        testPerformancePoco<Json, ALWAYS>(buffer, testData, data2, nIterations);
    }
    fmt::print("Json performance with lenient checks (collect exceptions);\n");
    for (int i = 0; i < 10; i++) {
        testPerformancePoco<Json, LENIENT>(buffer, testData, data2, nIterations);
    }
    fmt::print("Json performance without checks:\n");
    for (int i = 0; i < 10; i++) {
        testPerformancePoco<Json, IGNORE>(buffer, testData, data2, nIterations);
    }
}