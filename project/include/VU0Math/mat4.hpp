#pragma once

#include <array>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <VU0Math/vec4.hpp>

namespace ps2math {
    class Mat4 {
        public:

            Mat4();
            explicit Mat4(const float m11, const float m12, const float m13, const float m14,
                          const float m21, const float m22, const float m23, const float m24,
                          const float m31, const float m32, const float m33, const float m34,
                          const float m41, const float m42, const float m43, const float m44);
            ~Mat4();

            Mat4& RotateX(float angle);
            Mat4& RotateY(float angle);
            Mat4& RotateZ(float angle);

            static Mat4 identity();

            friend Vec4 operator*(const Mat4& lhs, const Vec4& rhs);
        private:
            float data[16] alignas(16*sizeof(float));
            const std::size_t size = 16;
            inline const float* GetDataPtr() const { return &data[0]; }
            inline float* GetDataPtr() { return &data[0]; }
    };

}