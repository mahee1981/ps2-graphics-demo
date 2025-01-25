#include <VU0Math/mat4.hpp>

ps2math::Mat4::Mat4()
{
    std::fill(std::begin(data), std::end(data), 0);
    data[0] = 1.0f;  // m11
    data[5] = 1.0f;  // m22
    data[10] = 1.0f; // m33
    data[15] = 1.0f; // m44
}

ps2math::Mat4 ps2math::Mat4::zero()
{
    Mat4 work;
    std::fill(std::begin(work.data), std::end(work.data), 0);
    return work;
}


ps2math::Mat4::Mat4(const float m11, const float m12, const float m13, const float m14,
    const float m21, const float m22, const float m23, const float m24,
    const float m31, const float m32, const float m33, const float m34,
    const float m41, const float m42, const float m43, const float m44)
{
    data[0] = m11;
    data[1] = m12;
    data[2] = m13;
    data[3] = m14;

    data[4] = m21;
    data[5] = m22;
    data[6] = m23;
    data[7] = m24;

    data[8] = m31;
    data[9] = m32;
    data[10] = m33;
    data[11] = m34;

    data[12] = m41;
    data[13] = m42;
    data[14] = m43;
    data[15] = m44;
}

ps2math::Mat4::Mat4(const std::array<float, 16> &values)
{
    std::copy(std::begin(values), std::end(values), std::begin(data));
}

ps2math::Mat4& ps2math::Mat4::operator=(const Mat4& rhs)
{
    if(this == &rhs)
        return *this;

    asm volatile(
        "lqc2		$vf1, 0x00(%1)	\n"
        "lqc2		$vf2, 0x10(%1)	\n"
        "lqc2		$vf3, 0x20(%1)	\n"
        "lqc2		$vf4, 0x30(%1)	\n"
        "sqc2		$vf1, 0x00(%0)	\n"
        "sqc2		$vf2, 0x10(%0)	\n"
        "sqc2		$vf3, 0x20(%0)	\n"
        "sqc2		$vf4, 0x30(%0)	\n"
        :
        : "r"(this->GetDataPtr()), "r"(rhs.GetDataPtr())
        : "memory");

    return *this;
}

ps2math::Mat4::~Mat4()
{
}

ps2math::Mat4 ps2math::Mat4::identity()
{
    return Mat4();
}

ps2math::Mat4 ps2math::Mat4::rotateX(const ps2math::Mat4& model, float angle)
{
    Mat4 work = Mat4();

    work.data[5] = cosf(angle);
    work.data[10] = cosf(angle);
    work.data[6] = -sinf(angle);
    work.data[9] = sinf(angle);

    return model * work;
}

ps2math::Mat4 ps2math::Mat4::rotateY(const ps2math::Mat4& model, float angle)
{
    Mat4 work = Mat4();

    work.data[0] = cosf(angle);
    work.data[2] = sinf(angle);
    work.data[8] = -sinf(angle);
    work.data[10] = cosf(angle);

    return model * work;
}

ps2math::Mat4 ps2math::Mat4::rotateZ(const ps2math::Mat4& model, float angle)
{
    Mat4 work = Mat4();

    work.data[0] = cosf(angle);
    work.data[1] = -sinf(angle);
    work.data[4] = sinf(angle);
    work.data[5] = cosf(angle);

    return model * work;
}

ps2math::Mat4 ps2math::Mat4::translate(const Mat4& model, const Vec4 &translationVector)
{
    Mat4 work = Mat4();

    work.data[12] = translationVector.x;
    work.data[13] = translationVector.y;
    work.data[14] = translationVector.z;
    work.data[15] = 1;  

    return model * work;

}
ps2math::Mat4 ps2math::Mat4::scale(const Mat4 &model, const Vec4 &scaleVector)
{
    Mat4 work = Mat4();

    work.data[0] = scaleVector.x;
    work.data[5] = scaleVector.y;
    work.data[10] = scaleVector.z;

    return model * work;
}
//row-major order -> vectors are rows = Vec * Mat
ps2math::Vec4 ps2math::operator*(const Vec4& lhs, const Mat4& rhs)
{
    Vec4 work;
    asm volatile(
        "lqc2		$vf1, 0x00(%2)	\n"
        "lqc2		$vf2, 0x10(%2)	\n"
        "lqc2		$vf3, 0x20(%2)	\n"
        "lqc2		$vf4, 0x30(%2)	\n"
        "lqc2		$vf5, 0x00(%1)	\n"
        "vmulax 	$ACC, $vf1, $vf5\n" 
        "vmadday	$ACC, $vf2, $vf5\n"
        "vmaddaz	$ACC, $vf3, $vf5\n"
        "vmaddw	    $vf6, $vf4, $vf5\n" 
        "sqc2		$vf6, 0x00(%0)	\n"
        :
        : "r"(&work), "r"(&lhs), "r"(rhs.GetDataPtr())
        : "memory");

    return work;
}

ps2math::Mat4 ps2math::operator*(const Mat4& lhs, const Mat4& rhs)
{
    Mat4 work = Mat4();
    asm volatile(
        "lqc2	  $vf1, 0x00(%0) \n"
        "lqc2	  $vf2, 0x10(%0) \n"
        "lqc2	  $vf3, 0x20(%0) \n"
        "lqc2	  $vf4, 0x30(%0) \n"
        "lqc2	  $vf5, 0x00(%1) \n"
        "lqc2	  $vf6, 0x10(%1) \n"
        "lqc2	  $vf7, 0x20(%1) \n"
        "lqc2	  $vf8, 0x30(%1) \n"
        "vmulax.xyzw	$ACC, $vf5, $vf1\n"
        "vmadday.xyzw	$ACC, $vf6, $vf1\n"
        "vmaddaz.xyzw	$ACC, $vf7, $vf1\n"
        "vmaddw.xyzw	$vf1, $vf8, $vf1\n"
        "vmulax.xyzw	$ACC, $vf5, $vf2\n"
        "vmadday.xyzw	$ACC, $vf6, $vf2\n"
        "vmaddaz.xyzw	$ACC, $vf7, $vf2\n"
        "vmaddw.xyzw	$vf2, $vf8, $vf2\n"
        "vmulax.xyzw	$ACC, $vf5, $vf3\n"
        "vmadday.xyzw	$ACC, $vf6, $vf3\n"
        "vmaddaz.xyzw	$ACC, $vf7, $vf3\n"
        "vmaddw.xyzw	$vf3, $vf8, $vf3\n"
        "vmulax.xyzw	$ACC, $vf5, $vf4\n"
        "vmadday.xyzw	$ACC, $vf6, $vf4\n"
        "vmaddaz.xyzw	$ACC, $vf7, $vf4\n"
        "vmaddw.xyzw	$vf4, $vf8, $vf4\n"
        "sqc2	  $vf1, 0x00(%2) \n"
        "sqc2	  $vf2, 0x10(%2) \n"
        "sqc2	  $vf3, 0x20(%2) \n"
        "sqc2	  $vf4, 0x30(%2) \n"
        :
        : "r"(lhs.GetDataPtr()), "r"(rhs.GetDataPtr()), "r"(work.GetDataPtr())
        : "memory");
    return work;
}

ps2math::Mat4 ps2math::Mat4::perspective(float fieldOfViewRadians, float aspectRatio, float nearPlaneDistance, float farPlaneDistance)
{
    Mat4 perspective = zero();

    float scale = 1.0f / std::tanf(fieldOfViewRadians * 0.5);
    
    perspective.data[0] = scale * aspectRatio;
    perspective.data[5] = scale;
    perspective.data[10] = farPlaneDistance/ (farPlaneDistance - nearPlaneDistance);
    perspective.data[11] = 1.0f;
    perspective.data[14] = (-farPlaneDistance * nearPlaneDistance) / (farPlaneDistance - nearPlaneDistance);
    perspective.data[15] = 0;

    return perspective;
    
}
void ps2math::Mat4::PrintMatrix()
{
    for (int i = 0; i < 4; i++) {
        printf("Row %d: ", i + 1);
        for (int j = 0; j < 4; j++) {
            printf("%f ", data[4 * i + j]);
        }
        printf("\n");
    }
}
