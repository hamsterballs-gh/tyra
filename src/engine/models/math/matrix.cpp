/*
# ______       ____   ___
#   |     \/   ____| |___|    
#   |     |   |   \  |   |       
#-----------------------------------------------------------------------
# Copyright 2020, tyra - https://github.com/h4570/tyra
# Licenced under Apache License 2.0
# Sandro Sobczyński <sandro.sobczynski@gmail.com>
*/

#include "../../include/models/math/matrix.hpp"

#include "../../include/utils/math.hpp"
#include <stdio.h>

// ----
// Constructors/Destructors
// ----

/** Create by specifying all points */
Matrix::Matrix(float m11, float m12, float m13, float m14,
               float m21, float m22, float m23, float m24,
               float m31, float m32, float m33, float m34,
               float m41, float m42, float m43, float m44)
{
    // data[0] = m11;
    // data[1] = m12;
    // data[2] = m13;
    // data[3] = m14;

    // data[4] = m21;
    // data[5] = m22;
    // data[6] = m23;
    // data[7] = m24;

    // data[8] = m31;
    // data[9] = m32;
    // data[10] = m33;
    // data[11] = m34;

    // data[12] = m41;
    // data[13] = m42;
    // data[14] = m43;
    // data[15] = m44;
}

/** Create with another matrix values */
Matrix::Matrix(const Matrix &v)
{
    // TODO memcpy();
    data[0] = v.data[0];
    data[1] = v.data[1];
    data[2] = v.data[2];
    data[3] = v.data[3];

    data[4] = v.data[4];
    data[5] = v.data[5];
    data[6] = v.data[6];
    data[7] = v.data[7];

    data[8] = v.data[8];
    data[9] = v.data[9];
    data[10] = v.data[10];
    data[11] = v.data[11];

    data[12] = v.data[12];
    data[13] = v.data[13];
    data[14] = v.data[14];
    data[15] = v.data[15];
}

void Matrix::identity()
{
    asm volatile(
        "vsub.xyzw  vf4, vf0, vf0 \n\t"
        "vadd.w     vf4, vf4, vf0 \n\t"
        "vmr32.xyzw vf5, vf4      \n\t"
        "vmr32.xyzw vf6, vf5      \n\t"
        "vmr32.xyzw vf7, vf6      \n\t"
        "sqc2       vf4, 0x30(%0) \n\t"
        "sqc2       vf5, 0x20(%0) \n\t"
        "sqc2       vf6, 0x10(%0) \n\t"
        "sqc2       vf7, 0x0(%0)  \n\t"
        :
        : "r"(this->data));
}

void Matrix::translate(const Vector3 &t_val)
{
    this->data[12] += t_val.x; // 3,0
    this->data[13] += t_val.y; // 3,1
    this->data[14] += t_val.z; // 3,2
}

void Matrix::rotateX(const float &t_radians)
{
    Matrix temp = Matrix();
    temp.identity();
    float c = Math::cos(t_radians);
    float s = Math::sin(t_radians);
    this->data[5] = c;  // 1,1
    this->data[6] = s;  // 1,2
    this->data[9] = -s; // 2,1
    this->data[10] = c; // 2,2
}

void Matrix::rotateY(const float &t_radians)
{
    Matrix temp = Matrix();
    temp.identity();
    float c = Math::cos(t_radians);
    float s = Math::sin(t_radians);
    this->data[0] = c;  // 0,0
    this->data[2] = -s; // 0,3
    this->data[8] = s;  // 2,0
    this->data[10] = c; // 2,2
}

void Matrix::rotateZ(const float &t_radians)
{
    Matrix temp = Matrix();
    temp.identity();
    float c = Math::cos(t_radians);
    float s = Math::sin(t_radians);
    this->data[0] = c;  // 0,0
    this->data[1] = s;  // 0,1
    this->data[4] = -s; // 1,0
    this->data[5] = c;  // 1,1
}

Matrix Matrix::operator*(const Matrix &t)
{
    Matrix result;
    asm volatile(
        "lqc2         vf1, 0x00(%1) \n\t"
        "lqc2         vf2, 0x10(%1) \n\t"
        "lqc2         vf3, 0x20(%1) \n\t"
        "lqc2         vf4, 0x30(%1) \n\t"
        "lqc2         vf5, 0x00(%2) \n\t"
        "lqc2         vf6, 0x10(%2) \n\t"
        "lqc2         vf7, 0x20(%2) \n\t"
        "lqc2         vf8, 0x30(%2) \n\t"
        "vmulax.xyzw  ACC, vf5, vf1 \n\t"
        "vmadday.xyzw ACC, vf6, vf1 \n\t"
        "vmaddaz.xyzw ACC, vf7, vf1 \n\t"
        "vmaddw.xyzw  vf1, vf8, vf1 \n\t"
        "vmulax.xyzw  ACC, vf5, vf2 \n\t"
        "vmadday.xyzw ACC, vf6, vf2 \n\t"
        "vmaddaz.xyzw ACC, vf7, vf2 \n\t"
        "vmaddw.xyzw  vf2, vf8, vf2 \n\t"
        "vmulax.xyzw  ACC, vf5, vf3 \n\t"
        "vmadday.xyzw ACC, vf6, vf3 \n\t"
        "vmaddaz.xyzw ACC, vf7, vf3 \n\t"
        "vmaddw.xyzw  vf3, vf8, vf3 \n\t"
        "vmulax.xyzw  ACC, vf5, vf4 \n\t"
        "vmadday.xyzw ACC, vf6, vf4 \n\t"
        "vmaddaz.xyzw ACC, vf7, vf4 \n\t"
        "vmaddw.xyzw  vf4, vf8, vf4 \n\t"
        "sqc2         vf1, 0x00(%0) \n\t"
        "sqc2         vf2, 0x10(%0) \n\t"
        "sqc2         vf3, 0x20(%0) \n\t"
        "sqc2         vf4, 0x30(%0) \n\t"
        :
        : "r"(result.data), "r"(this->data), "r"(t.data)
        : "memory");
    return result;
}

void Matrix::setScale(const Vector3 &t_val)
{
    this->identity();
    this->data[0] = t_val.x;
    this->data[5] = t_val.y;
    this->data[10] = t_val.z;
    this->data[15] = 1.0F;
}

void Matrix::rotationByAngle(const float &t_angle, const Vector3 &t_axis)
{
    Vector3 localAxis = Vector3(t_axis);
    localAxis.normalize();
    float x = localAxis.x;
    float y = localAxis.y;
    float z = localAxis.z;

    float c = Math::cos(t_angle);
    float s = Math::sin(t_angle);

    this->data[0] = x * x * (1 - c) + c;
    this->data[1] = y * x * (1 - c) + z * s;
    this->data[2] = x * z * (1 - c) - y * s;
    this->data[3] = 0.0F;

    this->data[4] = x * y * (1 - c) - z * s;
    this->data[5] = y * y * (1 - c) + c;
    this->data[6] = y * z * (1 - c) + x * s;
    this->data[7] = 0.0F;

    this->data[8] = x * z * (1 - c) + y * s;
    this->data[9] = y * z * (1 - c) - x * s;
    this->data[10] = z * z * (1 - c) + c;
    this->data[11] = 0.0F;

    this->data[12] = 0.0F;
    this->data[13] = 0.0F;
    this->data[14] = 0.0F;
    this->data[15] = 1.0F;
}

void Matrix::operator*=(const Matrix &t)
{
    asm volatile(
        "lqc2         vf1, 0x00(%1) \n\t"
        "lqc2         vf2, 0x10(%1) \n\t"
        "lqc2         vf3, 0x20(%1) \n\t"
        "lqc2         vf4, 0x30(%1) \n\t"
        "lqc2         vf5, 0x00(%2) \n\t"
        "lqc2         vf6, 0x10(%2) \n\t"
        "lqc2         vf7, 0x20(%2) \n\t"
        "lqc2         vf8, 0x30(%2) \n\t"
        "vmulax.xyzw  ACC, vf5, vf1 \n\t"
        "vmadday.xyzw ACC, vf6, vf1 \n\t"
        "vmaddaz.xyzw ACC, vf7, vf1 \n\t"
        "vmaddw.xyzw  vf1, vf8, vf1 \n\t"
        "vmulax.xyzw  ACC, vf5, vf2 \n\t"
        "vmadday.xyzw ACC, vf6, vf2 \n\t"
        "vmaddaz.xyzw ACC, vf7, vf2 \n\t"
        "vmaddw.xyzw  vf2, vf8, vf2 \n\t"
        "vmulax.xyzw  ACC, vf5, vf3 \n\t"
        "vmadday.xyzw ACC, vf6, vf3 \n\t"
        "vmaddaz.xyzw ACC, vf7, vf3 \n\t"
        "vmaddw.xyzw  vf3, vf8, vf3 \n\t"
        "vmulax.xyzw  ACC, vf5, vf4 \n\t"
        "vmadday.xyzw ACC, vf6, vf4 \n\t"
        "vmaddaz.xyzw ACC, vf7, vf4 \n\t"
        "vmaddw.xyzw  vf4, vf8, vf4 \n\t"
        "sqc2         vf1, 0x00(%0) \n\t"
        "sqc2         vf2, 0x10(%0) \n\t"
        "sqc2         vf3, 0x20(%0) \n\t"
        "sqc2         vf4, 0x30(%0) \n\t"
        :
        : "r"(this->data), "r"(this->data), "r"(t.data)
        : "memory");
}

Matrix Matrix::operator&(const Matrix &t)
{
    Matrix result;
    asm __volatile__(
        "lqc2			vf4, 0x00(%1) \n\t"
        "lqc2			vf5, 0x10(%1) \n\t"
        "lqc2			vf6, 0x20(%1) \n\t"
        "lqc2			vf7, 0x30(%1) \n\t"

        "lqc2			vf8, 0x00(%2) \n\t"
        "lqc2			vf9, 0x10(%2) \n\t"
        "lqc2			vf10, 0x20(%2) \n\t"
        "lqc2			vf11, 0x30(%2) \n\t"

        "vmulax.xyzw		ACC, vf4, vf8 \n\t"
        "vmadday.xyzw	ACC, vf5, vf8 \n\t"
        "vmaddaz.xyzw	ACC, vf6, vf8 \n\t"
        "vmaddw.xyzw		vf12, vf7, vf8 \n\t"
        "vmulax.xyzw		ACC, vf4, vf9 \n\t"
        "vmadday.xyzw	ACC, vf5, vf9 \n\t"
        "vmaddaz.xyzw	ACC, vf6, vf9 \n\t"
        "vmaddw.xyzw		vf13, vf7, vf9 \n\t"
        "vmulax.xyzw		ACC, vf4, vf10 \n\t"
        "vmadday.xyzw	ACC, vf5, vf10 \n\t"
        "vmaddaz.xyzw	ACC, vf6, vf10 \n\t"
        "vmaddw.xyzw		vf14, vf7, vf10 \n\t"
        "vmulax.xyzw		ACC, vf4, vf11 \n\t"
        "vmadday.xyzw	ACC, vf5, vf11 \n\t"
        "vmaddaz.xyzw	ACC, vf6, vf11 \n\t"
        "vmaddw.xyzw		vf15, vf7, vf11 \n\t"

        "sqc2			vf12, 0x00(%0) \n\t"
        "sqc2			vf13, 0x10(%0) \n\t"
        "sqc2			vf14, 0x20(%0) \n\t"
        "sqc2			vf15, 0x30(%0) \n\t"
        :
        : "r"(result.data), "r"(this->data), "r"(t.data));
    return result;
}

void Matrix::operator&=(const Matrix &t)
{
    asm volatile(
        "lqc2			vf4, 0x00(%1) \n\t"
        "lqc2			vf5, 0x10(%1) \n\t"
        "lqc2			vf6, 0x20(%1) \n\t"
        "lqc2			vf7, 0x30(%1) \n\t"

        "lqc2			vf8, 0x00(%2) \n\t"
        "lqc2			vf9, 0x10(%2) \n\t"
        "lqc2			vf10, 0x20(%2) \n\t"
        "lqc2			vf11, 0x30(%2) \n\t"

        "vmulax.xyzw		ACC, vf4, vf8 \n\t"
        "vmadday.xyzw	ACC, vf5, vf8 \n\t"
        "vmaddaz.xyzw	ACC, vf6, vf8 \n\t"
        "vmaddw.xyzw		vf12, vf7, vf8 \n\t"
        "vmulax.xyzw		ACC, vf4, vf9 \n\t"
        "vmadday.xyzw	ACC, vf5, vf9 \n\t"
        "vmaddaz.xyzw	ACC, vf6, vf9 \n\t"
        "vmaddw.xyzw		vf13, vf7, vf9 \n\t"
        "vmulax.xyzw		ACC, vf4, vf10 \n\t"
        "vmadday.xyzw	ACC, vf5, vf10 \n\t"
        "vmaddaz.xyzw	ACC, vf6, vf10 \n\t"
        "vmaddw.xyzw		vf14, vf7, vf10 \n\t"
        "vmulax.xyzw		ACC, vf4, vf11 \n\t"
        "vmadday.xyzw	ACC, vf5, vf11 \n\t"
        "vmaddaz.xyzw	ACC, vf6, vf11 \n\t"
        "vmaddw.xyzw		vf15, vf7, vf11 \n\t"

        "sqc2			vf12, 0x00(%0) \n\t"
        "sqc2			vf13, 0x10(%0) \n\t"
        "sqc2			vf14, 0x20(%0) \n\t"
        "sqc2			vf15, 0x30(%0) \n\t"
        :
        : "r"(this->data), "r"(this->data), "r"(t.data));
}

/** Create empty matrix */
Matrix::Matrix()
{
    data[0] = 0.0F;
    data[1] = 0.0F;
    data[2] = 0.0F;
    data[3] = 0.0F;

    data[4] = 0.0F;
    data[5] = 0.0F;
    data[6] = 0.0F;
    data[7] = 0.0F;

    data[8] = 0.0F;
    data[9] = 0.0F;
    data[10] = 0.0F;
    data[11] = 0.0F;

    data[12] = 0.0F;
    data[13] = 0.0F;
    data[14] = 0.0F;
    data[15] = 0.0F;
}

Matrix::~Matrix() {}

// ----
// Methods
// ----

/** Set up a perspective projection matrix
 * 
 * Clone of gluPerspective()
 * https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml
 * @param fov (FOV in radians)/2
 * @param aspect Aspect ratio
 * @param scrW Half of screen width
 * @param scrH Half of screen height
 * @param zNear Distance to near plane
 * @param zFar Distance to far plane
 * @param projScale Projection scale
 */
void Matrix::setPerspective(ScreenSettings &t_screen)
{
    // float fovYdiv2 = Math::HALF_ANG2RAD * t_screen.fov;
    // float cotFOV = 1.0F / (Math::sin(fovYdiv2) / Math::cos(fovYdiv2));
    // float w = cotFOV * (t_screen.width / t_screen.projectionScale) / t_screen.aspectRatio;
    // float h = cotFOV * (t_screen.height / t_screen.projectionScale);

    // this->data[0] = w;
    // this->data[1] = 0.0F;
    // this->data[2] = 0.0F;
    // this->data[3] = 0.0F;

    // this->data[4] = 0.0F;
    // this->data[5] = -h;
    // this->data[6] = 0.0F;
    // this->data[7] = 0.0F;

    // this->data[8] = 0.0F;
    // this->data[9] = 0.0F;
    // this->data[10] =
    //     (t_screen.farPlaneDist + t_screen.nearPlaneDist) /
    //     (t_screen.farPlaneDist - t_screen.nearPlaneDist);
    // this->data[11] = -1.0F;

    // this->data[12] = 0.0F;
    // this->data[13] = 0.0F;
    // this->data[14] =
    //     (2.0F * t_screen.farPlaneDist * t_screen.nearPlaneDist) /
    //     (t_screen.farPlaneDist - t_screen.nearPlaneDist);
    // this->data[15] = 0.0F;
}

void Matrix::setFrustum(ScreenSettings &t_screen)
{
    // frustum & perspective both produce perspective projection

    float fov = t_screen.fov * Math::ANG2RAD;
    float h = 2.0F * t_screen.nearPlaneDist * Math::tan(fov / 2.0F);
    float w = h * t_screen.aspectRatio;

    float left = -w / 2.0F;
    float right = w / 2.0F;
    float bottom = -h / 2.0F;
    float top = h / 2.0F;
    float zNear = t_screen.nearPlaneDist;
    float zFar = t_screen.farPlaneDist;

    this->data[0] = (2.0f * zNear) / (right - left);
    this->data[1] = 0.0F;
    this->data[2] = 0.0F;
    this->data[3] = 0.0F;

    this->data[4] = 0.0F;
    this->data[5] = (2.0f * zNear) / (top - bottom);
    this->data[6] = 0.0F;
    this->data[7] = 0.0F;

    this->data[8] = (right + left) / (right - left);
    this->data[9] = (top + bottom) / (top - bottom);
    this->data[10] = -(zFar + zNear) / (zFar - zNear);
    this->data[11] = -1.0F;

    this->data[12] = 0.0F;
    this->data[13] = 0.0F;
    this->data[14] = (-2.0f * zFar * zNear) / (zFar - zNear);
    this->data[15] = 0.0F;
}

void Matrix::camera(const Vector3 &t_pos, const Vector3 &t_vz, const Vector3 &t_vy)
{
    //	Matrix  vf4, vf5, vf6, vf7
    //	t_pos   vf8
    //	t_vz    vf9
    //	t_vy    vf10
    //	vtmp    vf11
    asm __volatile__(
        "lqc2		    vf9, 0x00(%2)       \n\t"
        "lqc2		    vf10, 0x00(%3)      \n\t"
        // mtmp.unit()
        "vsub.w		    vf5, vf0, vf0   # mtmp[1][PW] = 0.0F \n\t"
        // vtmp.outerProduct(vy, vz);
        "vopmula.xyz	ACC, vf10, vf9      \n\t"
        "vopmsub.xyz	vf11, vf9, vf10     \n\t"
        // mtmp[0] = vtmp.normalize();
        "vmul.xyz	    vf12, vf11, vf11    \n\t"
        "vaddy.x		vf12, vf12, vf12    \n\t"
        "vaddz.x		vf12, vf12, vf12    \n\t"
        "vrsqrt		    Q, vf0w, vf12x      \n\t"
        "vsub.xyzw	    vf4, vf0, vf0       \n\t"
        "vwaitq                             \n\t"
        "vmulq.xyz      vf4, vf11, Q        \n\t"
        // mtmp[2] = vz.normalize();
        "vmul.xyz       vf12, vf9, vf9      \n\t"
        "vaddy.x		vf12, vf12, vf12    \n\t"
        "vaddz.x		vf12, vf12, vf12    \n\t"
        "vrsqrt	        Q, vf0w, vf12x      \n\t"
        "vsub.xyzw      vf6, vf0, vf0       \n\t"
        "vwaitq                             \n\t"
        "vmulq.xyz      vf6, vf9, Q         \n\t"
        // mtmp[1].outerProduct(mtmp[2], mtmp[0]);
        "vopmula.xyz	ACC, vf6, vf4       \n\t"
        "vopmsub.xyz	vf5, vf4, vf6       \n\t"
        // mtmp.transpose(pos);
        "lqc2		    vf7, 0x00(%1)       \n\t"
        // m = mtmp.inverse();
        "qmfc2.ni		$11, vf0            \n\t"
        "qmfc2.ni		$8, vf4             \n\t"
        "qmfc2.ni		$9, vf5             \n\t"
        "qmfc2.ni		$10, vf6            \n\t"

        "pextlw			$12, $9, $8 \n\t"
        "pextuw			$13, $9, $8 \n\t"
        "pextlw			$14, $11, $10 \n\t"
        "pextuw			$15, $11, $10 \n\t"
        "pcpyld			$8, $14, $12 \n\t"
        "pcpyud			$9, $12, $14 \n\t"
        "pcpyld			$10, $15, $13 \n\t"

        "qmtc2.ni		$8, vf16 \n\t"
        "qmtc2.ni		$9, vf17 \n\t"
        "qmtc2.ni		$10, vf18 \n\t"
        "vmulax.xyz		ACC, vf16, vf7 \n\t"
        "vmadday.xyz	ACC, vf17, vf7 \n\t"
        "vmaddz.xyz		vf5, vf18, vf7 \n\t"
        "vsub.xyzw		vf5, vf0, vf5 \n\t"

        "sq				$8, 0x00(%0) \n\t"
        "sq				$9, 0x10(%0) \n\t"
        "sq				$10, 0x20(%0) \n\t"
        "sqc2			vf5, 0x30(%0) \n\t"
        :
        : "r"(this->data), "r"(t_pos.xyz), "r"(t_vz.xyz), "r"(t_vy.xyz)
        : "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15");
}

/** Create a view matrix that transforms coordinates in
 * such a way that the user looks at a target vector
 * direction from a position vector.
 * 
 * Clone of OpenGL lookAt function
 * https://learnopengl.com/Getting-started/Camera
*/
void Matrix::lookAt(Vector3 &t_position, Vector3 &t_target)
{
    Vector3 up_vec, view_vec;
    asm __volatile__(
        "lqc2           vf4, 0x00(%2)	# eye                               \n\t"
        "lqc2		    vf5, 0x00(%3)	# obj                               \n\t"
        "vsub.xyz	    vf7, vf4, vf5	# view_vec = vf7                    \n\t"
        "vmove.xyzw	    vf6, vf0                                            \n\t"
        "vaddw.y		vf6, vf0, vf0	# vf6 = { 0.0f, 1.0f, 0.0f, 1.0f }  \n\t"
        "vopmula.xyz	ACC, vf6, vf7                                       \n\t"
        "vopmsub.xyz	vf9, vf7, vf6	# vec = vf9                         \n\t"
        "vopmula.xyz	ACC, vf7, vf9                                       \n\t"
        "vopmsub.xyz	vf8, vf9, vf7	# up_vec = vf8                      \n\t"
        "sqc2		    vf7, 0x00(%0)	# view_vec                          \n\t"
        "sqc2		    vf8, 0x00(%1)	# up_vec                            \n\t"
        :
        : "r"(view_vec.xyz), "r"(up_vec.xyz), "r"(t_position.xyz), "r"(t_target.xyz));
    Matrix temp;
    temp.camera(t_position, view_vec, up_vec);
    unit();
    operator&=(temp);
}

const void Matrix::print() const
{
    printf("MATRIX(\n%f, %f, %f, %f\n%f, %f, %f, %f\n%f, %f, %f, %f\n%f, %f, %f, %f\n)\n",
           data[0], data[1], data[2], data[3],
           data[4], data[5], data[6], data[7],
           data[8], data[9], data[10], data[11],
           data[12], data[13], data[14], data[15]);
}
