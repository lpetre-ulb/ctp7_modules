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
        std::cerr << "Remote call failed: " << e.message << std::endl;
        return 1;
    } catch (const RPC::RemoteException &e) {
        std::cerr << "Remote call failed: " << e.what() << std::endl;
        if (e.has_backtrace()) {
            std::cerr << e.backtrace(); // The backtrace ends with a new line
        }
        return 1;
    }

    return 0;
}
