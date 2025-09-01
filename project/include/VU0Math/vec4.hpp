#ifndef VEC4_HPP
#define VEC4_HPP

#include <array>
#include <stdexcept>
#include <cmath>

namespace ps2math {

    // Row-major EE vector implementation
    class Vec4
    {

    public:
        Vec4();
        Vec4(float x, float y, float z, float w);
        Vec4(const std::initializer_list<float> &values);

        Vec4(std::array<float, 4> &&components) noexcept;
        Vec4(const std::array<float, 4> &components);

        Vec4(const float* vertex);

        union {
            std::array<float, 4> components alignas(sizeof(float) * 4);
            struct {float x,y,z,w;};        
            struct {float r,g,b,a;};
        };

        friend Vec4 operator+(const Vec4& lhs, const Vec4& rhs);
        friend Vec4 operator-(const Vec4& lhs, const Vec4& rhs);
        friend Vec4 CrossProduct(const Vec4& lhs, const Vec4 rhs);
        
        Vec4& operator+=(const Vec4& rhs);
        Vec4& operator-=(const Vec4& rhs);
        Vec4 Normalize() const;

        friend Vec4 operator*(const Vec4& lhs, float a);
        friend Vec4 operator*(float a, const Vec4& lhs);

    }; 

    Vec4 operator+(const Vec4& lhs, const Vec4& rhs);
    Vec4 operator-(const Vec4& lhs, const Vec4& rhs);
    Vec4 CrossProduct(const Vec4& lhs, const Vec4& rhs);
    Vec4 operator*(const Vec4& lhs, float a);
    Vec4 operator*(float a, const Vec4& lhs);
}

#endif
