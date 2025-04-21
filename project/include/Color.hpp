#ifndef COLOR_HPP
#define COLOR_HPP

#include <algorithm>
#include "utils.hpp"
#include "ColorComponent.hpp"

namespace Colors
{

    // specified as an unsigned int 0-255 or floating point value 0.0f-1.0f
    class Color
    {

    private:
        float r, g, b, a;

    public:
        Color(float r, float g, float b, float a);
        Color();

        Color &SetColor(float r, float g, float b, float a);
        Color &SetColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

        unsigned char GetComponentValueAsUByte(ColorComponent component) const;
        float GetComponentValueAsFloat(ColorComponent component) const;

        qword_t GetAsQword() const;
    };
}
#endif