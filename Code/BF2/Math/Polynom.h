// Real polynomial in one variable: a std::vector<float> of coefficients, lowest power first.
// Header-only, as shipped: every member is a weak inline in both images (Math/Polynom.h is the
// __FILE__ of findRoots' degree error). Recovered from libaidll.so (Linux 1.5.3153, ia-32,
// symbols); bf2 carries the same weak bodies (findRoots 0x83ADDE0, evaluate 0x83AEFE0,
// operator+= 0x83AF030, operator*= 0x83AF180). The constructors, the copy and the two products
// have no symbol of their own; their shape is the one findRoots and Aimer::getNearestImpactPoint
// inline ([INFERRED] spelling).
// [PLATFORM] precision: the x87 build evaluates and sums in extended precision.

// The complex helpers below adapt GCC 3 libstdc++ std_complex.h.
// Upstream: https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.3.6/libstdc++-v3/include/std/std_complex.h
// Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002
// Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
// USA.

// As a special exception, you may use this file as part of a free software
// library without restriction.  Specifically, if other files instantiate
// templates or use macros or inline functions from this file, or you compile
// this file and link it with other files to produce an executable, this
// file does not by itself cause the resulting executable to be covered by
// the GNU General Public License.  This exception does not however
// invalidate any other reasons why the executable file might be covered by
// the GNU General Public License.

//
// ISO C++ 14882: 26.2  Complex Numbers
// Note: this is not a conforming implementation.
// Initially implemented by Ulrich Drepper <drepper@cygnus.com>
// Improved by Gabriel Dos Reis <dosreis@cmla.ens-cachan.fr>
//

#ifndef MATH_POLYNOM_H
#define MATH_POLYNOM_H

#include <cmath>
#include <complex>
#include <vector>

#include "../Core/Debug.h"

namespace dice {
namespace hfe {

namespace polynom_detail {

// The image's own std::sqrt<float>(const complex<float>&) (libaidll 0x1CA770) and
// std::pow<float>(const complex<float>&, const float&) (0x1CAA80): GCC 3 libstdc++ instantiations,
// transcribed so that the native library cannot change the roots. [CHOSEN, not recovered]
// packaging; the algorithms are upstream libstdc++'s. Other libraries differ materially here:
// a sqrt of a negative real with a tiny nonzero real part flips Cardano's sign test below.
inline float complexAbs(const std::complex<float>& z)
{
    const float x = std::fabs(z.real());
    const float y = std::fabs(z.imag());
    const float s = y <= x ? x : y;
    if (s == 0.0f)
        return s;
    const float xs = z.real() / s;
    const float ys = z.imag() / s;
    return s * std::sqrt(ys * ys + xs * xs);
}

inline std::complex<float> complexSqrt(const std::complex<float>& z)
{
    const float x = z.real();
    const float y = z.imag();
    if (x == 0.0f)
    {
        const float t = std::sqrt(std::fabs(y) * 0.5f);
        return std::complex<float>(t, y < 0.0f ? -t : t);
    }
    const float a = complexAbs(z);
    const float t = std::sqrt(std::fabs(x) + a + std::fabs(x) + a);
    const float u = t * 0.5f;
    if (x <= 0.0f)
        return std::complex<float>(std::fabs(y) / t, y < 0.0f ? -u : u);
    return std::complex<float>(u, y / t);
}

inline std::complex<float> complexPow(const std::complex<float>& z, const float& y)
{
    if (z.imag() == 0.0f)
        return std::complex<float>(std::pow(z.real(), y), 0.0f);
    const float logAbs = std::log(complexAbs(z));
    const float angle = std::atan2(z.imag(), z.real());
    const float r = std::exp(logAbs * y);
    return std::complex<float>(r * std::cos(y * angle), std::sin(y * angle) * r);
}

} // namespace polynom_detail

class Polynom;
Polynom operator*(float factor, const Polynom& polynom);
Polynom operator*(const Polynom& lhs, const Polynom& rhs);

class Polynom : public std::vector<float>
{
public:
    Polynom() {}

    // `degree + 1` zero coefficients (the image fills them and zeroes them once more).
    explicit Polynom(unsigned int size)
        : std::vector<float>(size, 0.0f)
    {
        for (unsigned int i = 0; i < size; ++i)
            (*this)[i] = 0.0f;
    }

    // The image copies by default construction and assignment (vector::operator=).
    Polynom(const Polynom& other)
        : std::vector<float>()
    {
        std::vector<float>::operator=(other);
    }

    // Horner's scheme; an empty polynomial is 0. ELF 0x1CA310.
    float evaluate(float x) const
    {
        const unsigned int n = size();
        float result = 0.0f;
        if (n >= 2)
        {
            result = (*this)[n - 1];
            for (unsigned int i = n - 2; i < n; --i)
                result = result * x + (*this)[i];
        }
        else if (n == 1)
        {
            result = (*this)[0];
        }
        return result;
    }

    // ELF 0x1CA370. Grows to the longer operand.
    Polynom& operator+=(const Polynom& other)
    {
        const unsigned int n = size();
        const unsigned int m = other.size();
        if (n < m)
        {
            resize(m, 0.0f);
            unsigned int i = 0;
            for (; i < n; ++i)
                (*this)[i] = other[i] + (*this)[i];
            for (; i < m; ++i)
                (*this)[i] = other[i];
        }
        else
        {
            for (unsigned int i = 0; i < m; ++i)
                (*this)[i] = other[i] + (*this)[i];
        }
        return *this;
    }

    // ELF 0x1CA4D0. The product has size() + other.size() - 1 coefficients.
    Polynom& operator*=(const Polynom& other)
    {
        Polynom product(size() - 1 + other.size());
        for (unsigned int i = 0; i < size(); ++i)
            for (unsigned int j = 0; j < other.size(); ++j)
                product[i + j] = other[j] * (*this)[i] + product[i + j];
        std::vector<float>::operator=(product);
        return *this;
    }

    // Appends the real roots of a polynomial of degree 1 to 3 (trailing zero coefficients are
    // dropped first) to `roots`. A negative quadratic discriminant adds none, one below 1e-5 a
    // double root once. The cubic is depressed by x = t - c2 / (3 c3) and solved by Cardano: one
    // real root for a discriminant above 1e-10, else the real parts of the three complex roots.
    // Degree 0 adds nothing; a higher degree is reported (an Error-type Debug, which aborts in
    // this tree). ELF 0x1C8F50.
    void findRoots(std::vector<float>& roots) const
    {
        unsigned int degree = size() - 1;
        while (degree != 0 && (*this)[degree] == 0.0f)
            --degree;

        switch (degree)
        {
        case 0:
            return;
        case 1:
            roots.push_back(-(*this)[0] / (*this)[1]);
            return;
        case 2:
        {
            const float b = (*this)[1];
            const float discriminant = b * b - (*this)[2] * 4.0f * (*this)[0];
            if (!(discriminant >= 0.0f))
                return;
            if (discriminant < 1e-05f)
            {
                roots.push_back(-b / ((*this)[2] + (*this)[2]));
                return;
            }
            const float root = std::sqrt(discriminant);
            roots.push_back((-b - root) / ((*this)[2] + (*this)[2]));
            roots.push_back((root - (*this)[1]) / ((*this)[2] + (*this)[2]));
            return;
        }
        case 3:
            break;
        default:
            Debug(DebugTypeError, "Math/Polynom.h", 0x100, "") << "Polynom::findRoots() unhandled polynom degree";
            return;
        }

        const float c3 = (*this)[3];
        Polynom shift(2);   // x = t + shift[0]
        shift[0] = -(*this)[2] / (c3 * 3.0f);
        shift[1] = 1.0f;
        Polynom depressed(1);
        depressed[0] = (*this)[0];
        depressed += (*this)[1] * shift;
        depressed += (*this)[2] * shift * shift;
        depressed += c3 * shift * shift * shift;
        Polynom scale(1);
        scale[0] = 1.0f / c3;
        depressed *= scale;   // t^3 + p t + q

        const float p3 = depressed[1] / 3.0f;
        const float q = depressed[0];
        const float discriminant = p3 * p3 * p3 + q * 0.5f * (q * 0.5f);
        if (discriminant > 1e-10f)
        {
            const float root = std::sqrt(discriminant);
            float a = -q * 0.5f + root;
            float b = -q * 0.5f - root;
            const float u = a < 0.0f ? -std::pow(-a, 0.33333334f) : std::pow(a, 0.33333334f);
            const float v = b < 0.0f ? -std::pow(-b, 0.33333334f) : std::pow(b, 0.33333334f);
            roots.push_back(shift.evaluate(v + u));
            return;
        }

        typedef std::complex<float> Complex;
        const Complex half(-q * 0.5f, 0.0f);
        const Complex root = polynom_detail::complexSqrt(Complex(discriminant, 0.0f));
        const Complex a = half + root;
        const Complex b = half - root;
        const float third = 0.33333334f;
        const Complex u = a.real() < 0.0f ? -polynom_detail::complexPow(-a, third) : polynom_detail::complexPow(a, third);
        const Complex v = b.real() < 0.0f ? -polynom_detail::complexPow(-b, third) : polynom_detail::complexPow(b, third);
        const Complex sum = u + v;
        const Complex mid = -(sum / 2.0f);
        const Complex offset = (u - v) / 2.0f * Complex(0.0f, 1.7320508f);
        roots.push_back(shift.evaluate(sum.real()));
        roots.push_back(shift.evaluate((mid - offset).real()));
        roots.push_back(shift.evaluate((mid + offset).real()));
    }
};

// The products findRoots inlines: a scaled copy, and a copy multiplied in place.
inline Polynom operator*(float factor, const Polynom& polynom)
{
    Polynom result(polynom.size());
    for (unsigned int i = 0; i < polynom.size(); ++i)
        result[i] = factor * polynom[i];
    return result;
}

inline Polynom operator*(const Polynom& lhs, const Polynom& rhs)
{
    Polynom result(lhs);
    return result *= rhs;
}

} // namespace hfe
} // namespace dice

#endif // MATH_POLYNOM_H
