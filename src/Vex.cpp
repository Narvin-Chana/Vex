#include <Vex.h>

namespace vex
{

// Just some trash to test out cmake/clang-tidy...

// Function with unused parameter
void unusedParam(int x, int y)
{
    std::cout << "Only using x: " << x << '\n';
}

// Non-const reference parameter that isn't modified
void nonConstRef(std::string& name)
{
    std::cout << "Hello " << name << '\n';
}

bool func()
{
    return true;
}

} // namespace vex