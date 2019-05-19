#include "memory.h"

#include <iomanip>
#include <iostream>

int main(int argc, char **argv)
{
    try {
        RPC::Connection conn;
        conn.connect("localhost");

        conn.load_module("memory", "memory v1.0.1");

        auto mem = conn.call<Memory::Read>(0x6640000c, 1);
        std::cout << std::hex;
        for (std::uint32_t word : mem) {
            std::cout << "CTP7 Virtex-7 firmware release : " << word;
        }
        std::cout << std::endl;
    } catch (const wisc::RPCSvc::RPCException &e) {
        std::cout << e.message << std::endl;
    } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
    }

    return 0;
}
