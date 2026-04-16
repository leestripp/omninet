// OmniNet Exception Tests
#include "Exception.h"
#include <iostream>

int main() {
    try {
        throw OmniNet::SocketException("Test error", 42);
    } catch (const OmniNet::Exception& e) {
        std::cout << "Caught exception: " << e.what() << std::endl;
    }
    return 0;
}