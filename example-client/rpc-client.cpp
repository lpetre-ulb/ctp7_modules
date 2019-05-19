#include "memory.h"

#include <iomanip>
#include <iostream>

int main(int argc, char **argv)
{
    RPC::Connection conn;
    conn.connect("localhost");

    conn.load_module("memory", "memory v1.0.1");

    auto mem = conn.call<Memory::Read>(0, 10);
    std::cout << std::hex;
    for (std::uint32_t word : mem) {
        std::cout << " " << word;
    }
    std::cout << std::endl;

    return 0;
}
