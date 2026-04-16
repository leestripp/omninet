// OmniNet Utils Tests
#include "utils/Utils.h"
#include <iostream>

int main() {
    // Basic test
    auto parts = OmniNet::Utils::split("a,b,c", ',');
    std::cout << "Split test: " << parts.size() << " parts" << std::endl;
    
    return 0;
}