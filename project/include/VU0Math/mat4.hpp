#ifndef MAT4_HPP
#define MAT4_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include "VU0Math/vec4.hpp"

namespace ps2math {

//row-major EE matrix implementation
class Mat4 {
public:
    Mat4();
    explicit Mat4(const float m11, const float m12, const float m13, const float m14,
        const float m21, const float m22, const float m23, const float m24,
        const float m31, const float m32, const float m33, const float m34,
        const float m41, const float m42, const float m43, const float m44);

    explicit Mat4(const std::array<float,16> &values); 

    ~Mat4();

    Mat4& operator=(const Mat4& rhs);

    static Mat4 identity();
    static Mat4 zero();
    
    static Mat4 rotateX(const Mat4& model, float angle);
    static Mat4 rotateY(const Mat4& model, float angle);
    static Mat4 rotateZ(const Mat4& model, float angle);
    static Mat4 translate(const Mat4& model, const Vec4 &translationVector);
    static Mat4 scale(const Mat4 &model, const Vec4 &scaleVector);

    static Mat4 perspective(float fieldOfView, float aspectRatio, float nearPlaneDistance, float farPlaneDistance); 

    friend Mat4 operator*(const Mat4& lhs, const Mat4& rhs);
    friend Vec4 operator*(const Vec4& lhs, const Mat4& rhs);

    void PrintMatrix();
private:
    float data[16] alignas(16 * sizeof(float));
    inline const float* GetDataPtr() const { return &data[0]; }
    inline float* GetDataPtr() { return &data[0]; }
};

Mat4 operator*(const Mat4& lhs, const Mat4& rhs);
Vec4 operator*(const Vec4& lhs, const Mat4& rhs);
}

#endif
