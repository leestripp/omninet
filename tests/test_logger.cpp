// OmniNet Logger Tests
#include "Logger.h"
#include <iostream>

int main() {
    OmniNet::Logger::instance().info("Logger test");
    return 0;
}