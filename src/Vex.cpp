#include <Vex.h>

namespace vex
{

// Just some trash to test out clang-tidy...

// Function with unused parameter
void unusedParam(int x, int y)
{
    std::cout << "Only using x: " << x << std::endl;
}

// Non-const reference parameter that isn't modified
void nonConstRef(std::string &name)
{
    std::cout << "Hello " << name << std::endl;
}

bool func()
{
    return false;
}

} // namespace vex