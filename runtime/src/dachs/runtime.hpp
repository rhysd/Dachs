// cityhash64 is originally implemented in libc++ which is dual licensed under
// the MIT license and the UIUC License (a BSD-like license).
// See https://github.com/llvm-mirror/libcxx/blob/master/LICENSE.TXT for more detail.
//
// Copyright (c) 2009-2014 by the contributors listed in CREDITS.TXT
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#if !defined DACHS_RUNTIME_RUNTIME_HPP_INCLUDED
#define      DACHS_RUNTIME_RUNTIME_HPP_INCLUDED

#include <cstdint>
#include <utility>
#include <cstddef>
#include <cstring>

namespace dachs {
namespace runtime {

template <class Size>
struct cityhash64
{
    // cityhash64
    Size operator()(char const* s, Size len)
    {
        if (len <= 32) {
            if (len <= 16) {
            return hash_len_0_to_16(s, len);
            } else {
            return hash_len_17_to_32(s, len);
            }
        } else if (len <= 64) {
            return hash_len_33_to_64(s, len);
        }

        // For strings over 64 bytes we hash the end first, and then as we
        // loop we keep 56 bytes of state: v, w, x, y, and z.
        Size x = loadword(s + len - 40);
        Size y = loadword(s + len - 16) +
                    loadword(s + len - 56);
        Size z = hash_len_16(loadword(s + len - 48) + len,
                                loadword(s + len - 24));
        std::pair<Size, Size> v = weak_hash_len_32_with_seeds(s + len - 64, len, z);
        std::pair<Size, Size> w = weak_hash_len_32_with_seeds(s + len - 32, y + k1, x);
        x = x * k1 + loadword(s);

        // Decrease len to the nearest multiple of 64, and operate on 64-byte chunks.
        len = (len - 1) & ~static_cast<Size>(63);
        do {
            x = rotate(x + y + v.first + loadword(s + 8), 37) * k1;
            y = rotate(y + v.second + loadword(s + 48), 42) * k1;
            x ^= w.second;
            y += v.first + loadword(s + 40);
            z = rotate(z + w.first, 33) * k1;
            v = weak_hash_len_32_with_seeds(s, v.second * k1, x + w.first);
            w = weak_hash_len_32_with_seeds(s + 32, z + w.second,
                                                y + loadword(s + 16));
            std::swap(z, x);
            s += 64;
            len -= 64;
        } while (len != 0);
        return hash_len_16(
            hash_len_16(v.first, w.first) + shift_mix(y) * k1 + z,
            hash_len_16(v.second, w.second) + x);
    }


private:
    // Some primes between 2^63 and 2^64.
    static const Size k0 = 0xc3a5c85c97cb3127ULL;
    static const Size k1 = 0xb492b66fbe98f273ULL;
    static const Size k2 = 0x9ae16a3b2f90404fULL;
    static const Size k3 = 0xc949d7c7509e6557ULL;

    static Size loadword(void const* p)
    {
        Size r;
        std::memcpy(&r, p, sizeof(r));
        return r;
    }

    static Size rotate(Size val, int shift)
    {
        return shift == 0 ? val : ((val >> shift) | (val << (64 - shift)));
    }

    static Size rotate_by_at_least_1(Size val, int shift)
    {
        return (val >> shift) | (val << (64 - shift));
    }

    static Size shift_mix(Size val)
    {
        return val ^ (val >> 47);
    }

    static Size hash_len_16(Size u, Size v)
    {
        const Size mul = 0x9ddfea08eb382d69ULL;
        Size a = (u ^ v) * mul;
        a ^= (a >> 47);
        Size b = (v ^ a) * mul;
        b ^= (b >> 47);
        b *= mul;
        return b;
    }

    static Size hash_len_0_to_16(char const* s, Size len)
    {
        if (len > 8) {
            const Size a = loadword(s);
            const Size b = loadword(s + len - 8);
            return hash_len_16(a, rotate_by_at_least_1(b + len, len)) ^ b;
        }
        if (len >= 4) {
            const uint32_t a = loadword(s);
            const uint32_t b = loadword(s + len - 4);
            return hash_len_16(len + (a << 3), b);
        }
        if (len > 0) {
            const unsigned char a = s[0];
            const unsigned char b = s[len >> 1];
            const unsigned char c = s[len - 1];
            const uint32_t y = static_cast<uint32_t>(a) +
                (static_cast<uint32_t>(b) << 8);
            const uint32_t z = len + (static_cast<uint32_t>(c) << 2);
            return shift_mix(y * k2 ^ z * k3) * k2;
        }
        return k2;
    }

    static Size hash_len_17_to_32(char const* s, Size len)
    {
        const Size a = loadword(s) * k1;
        const Size b = loadword(s + 8);
        const Size c = loadword(s + len - 8) * k2;
        const Size d = loadword(s + len - 16) * k0;
        return hash_len_16(rotate(a - b, 43) + rotate(c, 30) + d,
                a + rotate(b ^ k3, 20) - c + len);
    }

    // Return a 16-byte hash for 48 bytes.  Quick and dirty.
    // Callers do best to use "random-looking" values for a and b.
    static std::pair<Size, Size> weak_hash_len_32_with_seeds(
            Size w, Size x, Size y, Size z, Size a, Size b)
    {
        a += w;
        b = rotate(b + a + z, 21);
        const Size c = a;
        a += x;
        a += y;
        b += rotate(a, 44);
        return std::pair<Size, Size>(a + z, b + c);
    }

    // Return a 16-byte hash for s[0] ... s[31], a, and b.  Quick and dirty.
    static std::pair<Size, Size> weak_hash_len_32_with_seeds(
            const char* s, Size a, Size b)
    {
        return weak_hash_len_32_with_seeds(loadword(s),
                loadword(s + 8),
                loadword(s + 16),
                loadword(s + 24),
                a,
                b);
    }

    // Return an 8-byte hash for 33 to 64 bytes.
    static Size hash_len_33_to_64(char const* s, std::size_t len)
    {
        Size z = loadword(s + 24);
        Size a = loadword(s) +
            (len + loadword(s + len - 16)) * k0;
        Size b = rotate(a + z, 52);
        Size c = rotate(a, 37);
        a += loadword(s + 8);
        c += rotate(a, 7);
        a += loadword(s + 16);
        Size vf = a + z;
        Size vs = b + rotate(a, 31) + c;
        a = loadword(s + len - 32);
        z += loadword(s + len - 8);
        b = rotate(a + z, 52);
        c = rotate(a, 37);
        a += loadword(s + len - 24);
        c += rotate(a, 7);
        a += loadword(s + len - 16);
        Size wf = a + z;
        Size ws = b + rotate(a, 31) + c;
        Size r = shift_mix((vf + ws) * k2 + (wf + vs) * k0);
        return shift_mix(r * k0 + vs) * k2;
    }
};

} // namespace runtime
} // namespace dachs

extern "C" {
    std::uint64_t __dachs_cityhash__(char const* const s);
    void __dachs_println_float__(double const d);
    void __dachs_println_int__(std::int64_t const i);
    void __dachs_println_uint__(std::uint64_t const u);
    void __dachs_println_char__(char const c);
    void __dachs_println_string__(char const* const s);
    void __dachs_println_symbol__(std::uint64_t const u);
    void __dachs_println_bool__(bool const b);
    void __dachs_print_float__(double const d);
    void __dachs_print_int__(std::int64_t const i);
    void __dachs_print_uint__(std::uint64_t const u);
    void __dachs_print_char__(char const c);
    void __dachs_print_string__(char const* const s);
    void __dachs_print_symbol__(std::uint64_t const s);
    void __dachs_print_bool__(bool const b);
    void __dachs_printf__(char const* const fmt, ...);
    void* __dachs_malloc__(std::uint64_t const size);
}

#endif    // DACHS_RUNTIME_RUNTIME_HPP_INCLUDED
