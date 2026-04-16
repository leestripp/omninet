// OmniNet Logger Tests
#include "utils/Logger.h"
#include <iostream>

int main() {
    OmniNet::Logger::instance().info(std::string("Logger test"));
    return 0;
}