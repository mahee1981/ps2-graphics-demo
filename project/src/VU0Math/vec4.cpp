#include "VU0Math/vec4.hpp"

ps2math::Vec4::Vec4() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}

ps2math::Vec4::Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

ps2math::Vec4::Vec4(const std::initializer_list<float> &values)
{
    if(values.size() != 4)
        throw std::range_error("Initalizer list is not of length 4");

    std::copy(values.begin(), values.end(), components.begin());
}

ps2math::Vec4::Vec4(std::array<float, 4> &&components) noexcept : components(std::move(components)) {}

ps2math::Vec4::Vec4(const std::array<float, 4> &components) : components(components) {}

ps2math::Vec4::Vec4(const float *vertex)
{
    std::copy(vertex, vertex+4, components.begin());
}

ps2math::Vec4 ps2math::operator+(const Vec4 &lhs, const Vec4 &rhs)
{
    Vec4 result;


    asm __volatile__(
        "lqc2 $vf1, 0x00(%0)      \n"
        "lqc2 $vf2, 0x00(%1)      \n"
        "1:                       \n"
        "vadd  $vf3, $vf1, $vf2   \n"
        "sqc2   $vf3, 0x00(%2)    \n"
        : : "r" (&lhs), "r" (&rhs), "r"(&result)
        : "memory"
    );

    return result;
}

ps2math::Vec4 ps2math::operator-(const Vec4 &lhs, const Vec4 &rhs)
{
    Vec4 result;

    asm __volatile__(
        "lqc2 $vf1, 0x00(%0)      \n"
        "lqc2 $vf2, 0x00(%1)      \n"
        "1:                       \n"
        "vsub  $vf3, $vf1, $vf2   \n"
        "sqc2   $vf3, 0x00(%2)    \n"
        : : "r" (&lhs), "r" (&rhs), "r"(&result)
        : "memory"
    );

    return result;
}

ps2math::Vec4 &ps2math::Vec4::operator+=(const Vec4 &rhs)
{
     asm __volatile__(
        "lqc2 $vf1, 0x00(%0)      \n"
        "lqc2 $vf2, 0x00(%1)      \n"
        "1:                       \n"
        "vadd  $vf1, $vf1, $vf2   \n"
        "sqc2   $vf1, 0x00(%0)    \n"
        : : "r" (this), "r" (&rhs)
        : "memory"
    );

    return *this;

}

ps2math::Vec4 &ps2math::Vec4::operator-=(const Vec4 &rhs)
{
     asm __volatile__(
        "lqc2 $vf1, 0x00(%0)      \n"
        "lqc2 $vf2, 0x00(%1)      \n"
        "1:                       \n"
        "vsub  $vf1, $vf1, $vf2   \n"
        "sqc2   $vf1, 0x00(%0)    \n"
        : : "r" (this), "r" (&rhs)
        : "memory"
    );

    return *this;

}