#include <gtest/gtest.h>

#include <Vex.h>

namespace vex
{

static constexpr int fact(int n)
{
    if (n == 2)
    {
        return 2;
    }
    return n * fact(n - 1);
}

TEST(GraphicsTests, FactSampleTest)
{
    EXPECT_EQ(fact(2), 2);
    EXPECT_EQ(fact(3), 6);
}

TEST(GraphicsTests, GraphicsCreationTest)
{
    GfxBackend backend{ BackendDescription{ .useSwapChain = false } };
}

} // namespace vex