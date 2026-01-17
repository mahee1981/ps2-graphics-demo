#include "common/Color.hpp"
#include <algorithm>

namespace Colors
{

Color::Color() : r(0), g(0), b(0), a(0)
{
}

Color::Color(u8 r, u8 g, u8 b, u8 a) : r(r), g(g), b(b), a(a)
{
}

} // namespace Colors
