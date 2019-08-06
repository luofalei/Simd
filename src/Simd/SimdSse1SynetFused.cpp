/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2019 Yermalayeu Ihar.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#include "Simd/SimdMemory.h"
#include "Simd/SimdArray.h"
#include "Simd/SimdStore.h"
#include "Simd/SimdExtract.h"
#include "Simd/SimdSynet.h"
#include "Simd/SimdBase.h"
#include "Simd/SimdSse1.h"

namespace Simd
{
#ifdef SIMD_SSE_ENABLE    
    namespace Sse
    {
         template <bool align> SIMD_INLINE void SynetFusedLayerForward0(const float * src, const float * bias, const float * scale, __m128 sign, float * dst, size_t offset)
        {
            __m128 _bias = Load<align>(bias + offset);
            __m128 x = _mm_add_ps(Load<align>(src + offset), _bias);
            __m128 _scale = Load<align>(scale + offset);
            Store<align>(dst + offset, _mm_add_ps(_mm_mul_ps(_mm_sub_ps(x, _mm_andnot_ps(sign, x)), _scale), _mm_max_ps(_mm_setzero_ps(), x)));
        }

        template <bool align> SIMD_INLINE void SynetFusedLayerForward0(const float * src, __m128 bias, __m128 scale, __m128 sign, float * dst, size_t offset)
        {
            __m128 x = _mm_add_ps(Load<align>(src + offset), bias);
            Store<align>(dst + offset, _mm_add_ps(_mm_mul_ps(_mm_sub_ps(x, _mm_andnot_ps(sign, x)), scale), _mm_max_ps(_mm_setzero_ps(), x)));
        }

        template <bool align> void SynetFusedLayerForward0Nchw(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(spatial) && Aligned(dst));

            size_t aligned = AlignLo(spatial, QF);
            size_t partial = AlignLo(spatial, F);
            __m128 sign = _mm_set1_ps(-0.0f);
            for (size_t c = 0; c < channels; ++c)
            {
                size_t s = 0;
                if (partial)
                {
                    __m128 _bias = _mm_set1_ps(bias[c]);
                    __m128 _scale = _mm_set1_ps(scale[c]);
                    for (; s < aligned; s += QF)
                    {
                        SynetFusedLayerForward0<align>(src, _bias, _scale, sign, dst, s + F * 0);
                        SynetFusedLayerForward0<align>(src, _bias, _scale, sign, dst, s + F * 1);
                        SynetFusedLayerForward0<align>(src, _bias, _scale, sign, dst, s + F * 2);
                        SynetFusedLayerForward0<align>(src, _bias, _scale, sign, dst, s + F * 3);
                    }
                    for (; s < partial; s += F)
                        SynetFusedLayerForward0<align>(src, _bias, _scale, sign, dst, s);
                }
                for (; s < spatial; ++s)
                    dst[s] = Base::SynetFusedLayerForward0(src[s] + bias[c], scale[c]);
                src += spatial;
                dst += spatial;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward0Nchw(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src) && Aligned(spatial) && Aligned(dst))
                SynetFusedLayerForward0Nchw<true>(src, bias, scale, channels, spatial, dst);
            else
                SynetFusedLayerForward0Nchw<false>(src, bias, scale, channels, spatial, dst);
        }

        template <bool align> void SynetFusedLayerForward0Nhwc(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(bias) && Aligned(scale) && Aligned(channels) && Aligned(dst));

            size_t aligned = AlignLo(channels, QF);
            size_t partial = AlignLo(channels, F);
            __m128 sign = _mm_set1_ps(-0.0f);
            for (size_t s = 0; s < spatial; ++s)
            {
                size_t c = 0;
                if (partial)
                {
                    for (; c < aligned; c += QF)
                    {
                        SynetFusedLayerForward0<align>(src, bias, scale, sign, dst, c + F * 0);
                        SynetFusedLayerForward0<align>(src, bias, scale, sign, dst, c + F * 1);
                        SynetFusedLayerForward0<align>(src, bias, scale, sign, dst, c + F * 2);
                        SynetFusedLayerForward0<align>(src, bias, scale, sign, dst, c + F * 3);
                    }
                    for (; c < partial; c += F)
                        SynetFusedLayerForward0<align>(src, bias, scale, sign, dst, c);
                }
                for (; c < channels; ++c)
                    dst[c] = Base::SynetFusedLayerForward0(src[c] + bias[c], scale[c]);
                src += channels;
                dst += channels;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward0Nhwc(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src) && Aligned(bias) && Aligned(scale) && Aligned(channels) && Aligned(dst))
                SynetFusedLayerForward0Nhwc<true>(src, bias, scale, channels, spatial, dst);
            else
                SynetFusedLayerForward0Nhwc<false>(src, bias, scale, channels, spatial, dst);
        }

        template <bool align> void SynetFusedLayerForward0Nchw4c(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(dst));

            size_t spatialF = spatial * F;
            size_t spatial4F = AlignLo(spatial, 4)*F;
            __m128 sign = _mm_set1_ps(-0.0f);
            for (size_t c = 0; c < channels; c += F)
            {
                __m128 _bias = Load<false>(bias + c);
                __m128 _scale = Load<false>(scale + c);
                size_t s = 0;
                for (; s < spatial4F; s += 4 * F)
                {
                    SynetFusedLayerForward0<align>(src, _bias, _scale, sign, dst, s + F * 0);
                    SynetFusedLayerForward0<align>(src, _bias, _scale, sign, dst, s + F * 1);
                    SynetFusedLayerForward0<align>(src, _bias, _scale, sign, dst, s + F * 2);
                    SynetFusedLayerForward0<align>(src, _bias, _scale, sign, dst, s + F * 3);
                }
                for (; s < spatialF; s += F)
                    SynetFusedLayerForward0<align>(src, _bias, _scale, sign, dst, s);
                src += spatialF;
                dst += spatialF;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward0Nchw4c(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src) && Aligned(dst))
                SynetFusedLayerForward0Nchw4c<true>(src, bias, scale, channels, spatial, dst);
            else
                SynetFusedLayerForward0Nchw4c<false>(src, bias, scale, channels, spatial, dst);
        }

        void SynetFusedLayerForward0(const float * src, const float * bias, const float * scale, size_t channels, size_t spatial, float * dst, SimdTensorFormatType format)
        {
            if (Base::NchwCompatible(channels, spatial, format))
                SynetFusedLayerForward0Nchw(src, bias, scale, channels, spatial, dst);
            else if (Base::NhwcCompatible(channels, spatial, format))
                SynetFusedLayerForward0Nhwc(src, bias, scale, channels, spatial, dst);
            else if (format == SimdTensorFormatNchw4c)
                SynetFusedLayerForward0Nchw4c(src, bias, scale, channels, spatial, dst);
            else
                Base::SynetFusedLayerForward0(src, bias, scale, channels, spatial, dst, format);
        }

        //---------------------------------------------------------------------

        template <bool align> SIMD_INLINE void SynetFusedLayerForward1(const float * src, const float * bias0, const float * scale1, const float *  bias1, float * dst, size_t offset)
        {
            __m128 _bias0 = Load<align>(bias0 + offset);
            __m128 x = _mm_add_ps(Load<align>(src + offset), _bias0);
            __m128 _scale1 = Load<align>(scale1 + offset);
            __m128 _bias1 = Load<align>(bias1 + offset);
            Store<align>(dst + offset, _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_max_ps(_mm_setzero_ps(), _mm_sub_ps(_mm_setzero_ps(), x)), _scale1), _bias1), _mm_max_ps(_mm_setzero_ps(), x)));
        }

        template <bool align> SIMD_INLINE void SynetFusedLayerForward1(const float * src, __m128 bias0, __m128 scale1, __m128 bias1, float * dst, size_t offset)
        {
            __m128 x = _mm_add_ps(Load<align>(src + offset), bias0);
            Store<align>(dst + offset, _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_max_ps(_mm_setzero_ps(), _mm_sub_ps(_mm_setzero_ps(), x)), scale1), bias1), _mm_max_ps(_mm_setzero_ps(), x)));
        }

        template <bool align> void SynetFusedLayerForward1Nchw(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(spatial) && Aligned(dst));

            size_t aligned = AlignLo(spatial, QF);
            size_t partial = AlignLo(spatial, F);
            for (size_t c = 0; c < channels; ++c)
            {
                size_t s = 0;
                if (partial)
                {
                    __m128 _bias0 = _mm_set1_ps(bias0[c]);
                    __m128 _scale1 = _mm_set1_ps(scale1[c]);
                    __m128 _bias1 = _mm_set1_ps(bias1[c]);
                    for (; s < aligned; s += QF)
                    {
                        SynetFusedLayerForward1<align>(src, _bias0, _scale1, _bias1, dst, s + F * 0);
                        SynetFusedLayerForward1<align>(src, _bias0, _scale1, _bias1, dst, s + F * 1);
                        SynetFusedLayerForward1<align>(src, _bias0, _scale1, _bias1, dst, s + F * 2);
                        SynetFusedLayerForward1<align>(src, _bias0, _scale1, _bias1, dst, s + F * 3);
                    }
                    for (; s < partial; s += F)
                        SynetFusedLayerForward1<align>(src, _bias0, _scale1, _bias1, dst, s);
                }
                for (; s < spatial; ++s)
                    dst[s] = Base::SynetFusedLayerForward1(src[s] + bias0[c], scale1[c], bias1[c]);
                src += spatial;
                dst += spatial;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward1Nchw(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src) && Aligned(spatial) && Aligned(dst))
                SynetFusedLayerForward1Nchw<true>(src, bias0, scale1, bias1, channels, spatial, dst);
            else
                SynetFusedLayerForward1Nchw<false>(src, bias0, scale1, bias1, channels, spatial, dst);
        }

        template <bool align> void SynetFusedLayerForward1Nhwc(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(bias0) && Aligned(scale1) && Aligned(bias1) && Aligned(channels) && Aligned(dst));

            size_t aligned = AlignLo(channels, QF);
            size_t partial = AlignLo(channels, F);
            for (size_t s = 0; s < spatial; ++s)
            {
                size_t c = 0;
                if (partial)
                {
                    for (; c < aligned; c += QF)
                    {
                        SynetFusedLayerForward1<align>(src, bias0, scale1, bias1, dst, c + F * 0);
                        SynetFusedLayerForward1<align>(src, bias0, scale1, bias1, dst, c + F * 1);
                        SynetFusedLayerForward1<align>(src, bias0, scale1, bias1, dst, c + F * 2);
                        SynetFusedLayerForward1<align>(src, bias0, scale1, bias1, dst, c + F * 3);
                    }
                    for (; c < partial; c += F)
                        SynetFusedLayerForward1<align>(src, bias0, scale1, bias1, dst, c);
                }
                for (; c < channels; ++c)
                    dst[c] = Base::SynetFusedLayerForward1(src[c] + bias0[c], scale1[c], bias1[c]);
                src += channels;
                dst += channels;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward1Nhwc(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src) && Aligned(bias0) && Aligned(scale1) && Aligned(bias1) && Aligned(channels) && Aligned(dst))
                SynetFusedLayerForward1Nhwc<true>(src, bias0, scale1, bias1, channels, spatial, dst);
            else
                SynetFusedLayerForward1Nhwc<false>(src, bias0, scale1, bias1, channels, spatial, dst);
        }

        template <bool align> void SynetFusedLayerForward1Nchw4c(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst)
        {
            if (align)
                assert(Aligned(src) && Aligned(dst));

            size_t spatialF = spatial * F;
            size_t spatial4F = AlignLo(spatial, 4)*F;
            for (size_t c = 0; c < channels; c += F)
            {
                __m128 _bias0 = Load<false>(bias0 + c);
                __m128 _scale1 = Load<false>(scale1 + c);
                __m128 _bias1 = Load<false>(bias1 + c);
                size_t s = 0;
                for (; s < spatial4F; s += 4 * F)
                {
                    SynetFusedLayerForward1<align>(src, _bias0, _scale1, _bias1, dst, s + F * 0);
                    SynetFusedLayerForward1<align>(src, _bias0, _scale1, _bias1, dst, s + F * 1);
                    SynetFusedLayerForward1<align>(src, _bias0, _scale1, _bias1, dst, s + F * 2);
                    SynetFusedLayerForward1<align>(src, _bias0, _scale1, _bias1, dst, s + F * 3);
                }
                for (; s < spatialF; s += F)
                    SynetFusedLayerForward1<align>(src, _bias0, _scale1, _bias1, dst, s);
                src += spatialF;
                dst += spatialF;
            }
        }

        SIMD_INLINE void SynetFusedLayerForward1Nchw4c(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst)
        {
            if (Aligned(src) && Aligned(dst))
                SynetFusedLayerForward1Nchw4c<true>(src, bias0, scale1, bias1, channels, spatial, dst);
            else
                SynetFusedLayerForward1Nchw4c<false>(src, bias0, scale1, bias1, channels, spatial, dst);
        }

        void SynetFusedLayerForward1(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t channels, size_t spatial, float * dst, SimdTensorFormatType format)
        {
            if (Base::NchwCompatible(channels, spatial, format))
                SynetFusedLayerForward1Nchw(src, bias0, scale1, bias1, channels, spatial, dst);
            else if (Base::NhwcCompatible(channels, spatial, format))
                SynetFusedLayerForward1Nhwc(src, bias0, scale1, bias1, channels, spatial, dst);
            else if (format == SimdTensorFormatNchw4c)
                SynetFusedLayerForward1Nchw4c(src, bias0, scale1, bias1, channels, spatial, dst);
            else
                Base::SynetFusedLayerForward1(src, bias0, scale1, bias1, channels, spatial, dst, format);
        }

        //---------------------------------------------------------------------

        template <bool align> SIMD_INLINE void SynetFusedLayerForward2(const float * src, const float * scale, const float * bias, __m128 slope, float * dst, size_t offset)
        {
            __m128 _src = Load<align>(src + offset);
            __m128 _scale = Load<align>(scale + offset);
            __m128 _bias = Load<align>(bias + offset);
            __m128 x = _mm_add_ps(_mm_mul_ps(_src, _scale), _bias);
            Store<align>(dst + offset, _mm_add_ps(_mm_max_ps(_mm_setzero_ps(), x), _mm_mul_ps(_mm_min_ps(_mm_setzero_ps(), x), slope)));
        }

        template <bool align> SIMD_INLINE void SynetFusedLayerForward2(const float * src, __m128 scale, __m128 bias, __m128 slope, float * dst, size_t offset)
        {
            __m128 _src = Load<align>(src + offset);
            __m128 x = _mm_add_ps(_mm_mul_ps(_src, scale), bias);
            Store<align>(dst + offset, _mm_add_ps(_mm_max_ps(_mm_setzero_ps(), x), _mm_mul_ps(_mm_min_ps(_mm_setzero_ps(), x), slope)));
        }

        template <bool align> void SynetFusedLayerForward2(const float * src, const float * scale, const float * bias, size_t count, size_t size, const float * slope, float * dst, SimdBool trans)
        {
            if (align)
                assert(((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(scale) && Aligned(bias) : Aligned(size)) && Aligned(src) && Aligned(dst));
            __m128 _slope = _mm_set1_ps(slope[0]);
            if ((trans || size == 1) && count != 1)
            {
                size_t aligned = AlignLo(count, QF);
                size_t partial = AlignLo(count, F);
                for (size_t j = 0; j < size; ++j)
                {
                    size_t i = 0;
                    if (partial)
                    {
                        for (; i < aligned; i += QF)
                        {
                            SynetFusedLayerForward2<align>(src, scale, bias, _slope, dst, i + 0 * F);
                            SynetFusedLayerForward2<align>(src, scale, bias, _slope, dst, i + 1 * F);
                            SynetFusedLayerForward2<align>(src, scale, bias, _slope, dst, i + 2 * F);
                            SynetFusedLayerForward2<align>(src, scale, bias, _slope, dst, i + 3 * F);
                        }
                        for (; i < partial; i += F)
                            SynetFusedLayerForward2<align>(src, scale, bias, _slope, dst, i);
                    }
                    for (; i < count; ++i)
                        dst[i] = Base::SynetFusedLayerForward2(src[i], scale[i], bias[i], slope[0]);
                    src += count;
                    dst += count;
                }
            }
            else
            {
                size_t aligned = AlignLo(size, QF);
                size_t partial = AlignLo(size, F);
                for (size_t i = 0; i < count; ++i)
                {
                    size_t j = 0;
                    if (partial)
                    {
                        __m128 _scale = _mm_set1_ps(scale[i]);
                        __m128 _bias = _mm_set1_ps(bias[i]);
                        for (; j < aligned; j += QF)
                        {
                            SynetFusedLayerForward2<align>(src, _scale, _bias, _slope, dst, j + 0 * F);
                            SynetFusedLayerForward2<align>(src, _scale, _bias, _slope, dst, j + 1 * F);
                            SynetFusedLayerForward2<align>(src, _scale, _bias, _slope, dst, j + 2 * F);
                            SynetFusedLayerForward2<align>(src, _scale, _bias, _slope, dst, j + 3 * F);
                        }
                        for (; j < partial; j += F)
                            SynetFusedLayerForward2<align>(src, _scale, _bias, _slope, dst, j);
                    }
                    for (; j < size; ++j)
                        dst[j] = Base::SynetFusedLayerForward2(src[j], scale[i], bias[i], slope[0]);
                    src += size;
                    dst += size;
                }
            }
        }

        void SynetFusedLayerForward2(const float * src, const float * scale, const float * bias, size_t count, size_t size, const float * slope, float * dst, SimdBool trans)
        {
            if (((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(scale) && Aligned(bias) : Aligned(size)) && Aligned(src) && Aligned(dst))
                SynetFusedLayerForward2<true>(src, scale, bias, count, size, slope, dst, trans);
            else
                SynetFusedLayerForward2<false>(src, scale, bias, count, size, slope, dst, trans);
        }

        //---------------------------------------------------------------------

        template <bool align> SIMD_INLINE void SynetFusedLayerForward3(const float * src, const float * bias, const float * scale, __m128 sign, float * dst, size_t offset)
        {
            __m128 _bias = Load<align>(bias + offset);
            __m128 x = _mm_add_ps(Load<align>(src + offset), _bias);
            __m128 _scale = Load<align>(scale + offset);
            __m128 pos = _mm_max_ps(_mm_setzero_ps(), x);
            __m128 neg = _mm_min_ps(_mm_setzero_ps(), x);
            Store<align>(dst + offset, _mm_add_ps(pos, _mm_mul_ps(_scale, neg)));
        }

        template <bool align> SIMD_INLINE void SynetFusedLayerForward3(const float * src, __m128 bias, __m128 scale, __m128 sign, float * dst, size_t offset)
        {
            __m128 x = _mm_add_ps(Load<align>(src + offset), bias);
            __m128 pos = _mm_max_ps(_mm_setzero_ps(), x);
            __m128 neg = _mm_min_ps(_mm_setzero_ps(), x);
            Store<align>(dst + offset, _mm_add_ps(pos, _mm_mul_ps(scale, neg)));
        }

        template <bool align> void SynetFusedLayerForward3(const float * src, const float * bias, const float * scale, size_t count, size_t size, float * dst, SimdBool trans)
        {
            if (align)
                assert(((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(scale) && Aligned(bias) : Aligned(size)) && Aligned(src) && Aligned(dst));
            __m128 sign = _mm_set1_ps(-0.0f);
            if ((trans || size == 1) && count != 1)
            {
                size_t aligned = AlignLo(count, QF);
                size_t partial = AlignLo(count, F);
                for (size_t j = 0; j < size; ++j)
                {
                    size_t i = 0;
                    if (partial)
                    {
                        for (; i < aligned; i += QF)
                        {
                            SynetFusedLayerForward3<align>(src, bias, scale, sign, dst, i + 0 * F);
                            SynetFusedLayerForward3<align>(src, bias, scale, sign, dst, i + 1 * F);
                            SynetFusedLayerForward3<align>(src, bias, scale, sign, dst, i + 2 * F);
                            SynetFusedLayerForward3<align>(src, bias, scale, sign, dst, i + 3 * F);
                        }
                        for (; i < partial; i += F)
                            SynetFusedLayerForward3<align>(src, bias, scale, sign, dst, i);
                    }
                    for (; i < count; ++i)
                        dst[i] = Base::SynetFusedLayerForward3(src[i] + bias[i], scale[i]);
                    src += count;
                    dst += count;
                }
            }
            else
            {
                size_t aligned = AlignLo(size, QF);
                size_t partial = AlignLo(size, F);
                for (size_t i = 0; i < count; ++i)
                {
                    size_t j = 0;
                    if (partial)
                    {
                        __m128 _bias = _mm_set1_ps(bias[i]);
                        __m128 _scale = _mm_set1_ps(scale[i]);
                        for (; j < aligned; j += QF)
                        {
                            SynetFusedLayerForward3<align>(src, _bias, _scale, sign, dst, j + 0 * F);
                            SynetFusedLayerForward3<align>(src, _bias, _scale, sign, dst, j + 1 * F);
                            SynetFusedLayerForward3<align>(src, _bias, _scale, sign, dst, j + 2 * F);
                            SynetFusedLayerForward3<align>(src, _bias, _scale, sign, dst, j + 3 * F);
                        }
                        for (; j < partial; j += F)
                            SynetFusedLayerForward3<align>(src, _bias, _scale, sign, dst, j);
                    }
                    for (; j < size; ++j)
                        dst[j] = Base::SynetFusedLayerForward3(src[j] + bias[i], scale[i]);
                    src += size;
                    dst += size;
                }
            }
        }

        void SynetFusedLayerForward3(const float * src, const float * bias, const float * scale, size_t count, size_t size, float * dst, SimdBool trans)
        {
            if (((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(scale) && Aligned(bias) : Aligned(size)) && Aligned(src) && Aligned(dst))
                SynetFusedLayerForward3<true>(src, bias, scale, count, size, dst, trans);
            else
                SynetFusedLayerForward3<false>(src, bias, scale, count, size, dst, trans);
        }

        //---------------------------------------------------------------------

        template <bool align> SIMD_INLINE void SynetFusedLayerForward4(const float * src, const float * bias0, __m128 scale1, __m128 bias1, float * dst0, float * dst1, size_t offset)
        {
            __m128 x = _mm_add_ps(Load<align>(src + offset), Load<align>(bias0 + offset));
            Store<align>(dst0 + offset, _mm_max_ps(_mm_setzero_ps(), x));
            Store<align>(dst1 + offset, _mm_max_ps(_mm_setzero_ps(), _mm_add_ps(bias1, _mm_mul_ps(scale1, x))));
        }

        template <bool align> SIMD_INLINE void SynetFusedLayerForward4(const float * src, __m128 bias0, __m128 scale1, __m128 bias1, float * dst0, float * dst1, size_t offset)
        {
            __m128 x = _mm_add_ps(Load<align>(src + offset), bias0);
            Store<align>(dst0 + offset, _mm_max_ps(_mm_setzero_ps(), x));
            Store<align>(dst1 + offset, _mm_max_ps(_mm_setzero_ps(), _mm_add_ps(bias1, _mm_mul_ps(scale1, x))));
        }

        template<bool align> void SynetFusedLayerForward4(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t count, size_t size, float * dst, SimdBool trans)
        {
            if (align)
                assert(((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(bias0) : Aligned(size)) && Aligned(src) && Aligned(dst));
            __m128 _scale1 = _mm_set1_ps(scale1[0]);
            __m128 _bias1 = _mm_set1_ps(bias1[0]);
            if ((trans || size == 1) && count != 1)
            {
                float * dst0 = dst, * dst1 = dst + count;
                size_t aligned = AlignLo(count, QF);
                size_t partial = AlignLo(count, F);
                for (size_t j = 0; j < size; ++j)
                {
                    size_t i = 0;
                    if (partial)
                    {
                        for (; i < aligned; i += QF)
                        {
                            SynetFusedLayerForward4<align>(src, bias0, _scale1, _bias1, dst0, dst1, i + 0 * F);
                            SynetFusedLayerForward4<align>(src, bias0, _scale1, _bias1, dst0, dst1, i + 1 * F);
                            SynetFusedLayerForward4<align>(src, bias0, _scale1, _bias1, dst0, dst1, i + 2 * F);
                            SynetFusedLayerForward4<align>(src, bias0, _scale1, _bias1, dst0, dst1, i + 3 * F);
                        }
                        for (; i < partial; i += F)
                            SynetFusedLayerForward4<align>(src, bias0, _scale1, _bias1, dst0, dst1, i);
                    }
                    for (; i < count; ++i)
                        Base::SynetFusedLayerForward4(src[i], bias0[i], scale1[0], bias1[0], dst0 + i, dst1 + i);
                    src += count;
                    dst0 += 2 * count;
                    dst1 += 2 * count;
                }
            }
            else
            {
                float * dst0 = dst, *dst1 = dst + count * size;
                size_t aligned = AlignLo(size, QF);
                size_t partial = AlignLo(size, F);
                for (size_t i = 0; i < count; ++i)
                {
                    size_t j = 0;
                    if (partial)
                    {
                        __m128 _bias0 = _mm_set1_ps(bias0[i]);
                        for (; j < aligned; j += QF)
                        {
                            SynetFusedLayerForward4<align>(src, _bias0, _scale1, _bias1, dst0, dst1, j + 0 * F);
                            SynetFusedLayerForward4<align>(src, _bias0, _scale1, _bias1, dst0, dst1, j + 1 * F);
                            SynetFusedLayerForward4<align>(src, _bias0, _scale1, _bias1, dst0, dst1, j + 2 * F);
                            SynetFusedLayerForward4<align>(src, _bias0, _scale1, _bias1, dst0, dst1, j + 3 * F);
                        }
                        for (; j < partial; j += F)
                            SynetFusedLayerForward4<align>(src, _bias0, _scale1, _bias1, dst0, dst1, j);
                    }
                    for (; j < size; ++j)
                        Base::SynetFusedLayerForward4(src[j], bias0[i], scale1[0], bias1[0], dst0 + j, dst1 + j);
                    src += size;
                    dst0 += size;
                    dst1 += size;
                }
            }
        }

        void SynetFusedLayerForward4(const float * src, const float * bias0, const float * scale1, const float * bias1, size_t count, size_t size, float * dst, SimdBool trans)
        {
            if (((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(bias0) : Aligned(size)) && Aligned(src) && Aligned(dst))
                SynetFusedLayerForward4<true>(src, bias0, scale1, bias1, count, size, dst, trans);
            else
                SynetFusedLayerForward4<false>(src, bias0, scale1, bias1, count, size, dst, trans);
        }

        //---------------------------------------------------------------------

        template <bool align> SIMD_INLINE void SynetFusedLayerForward8(const float * src0, const float * src1, const float * src2, float * dst, size_t offset)
        {
            Store<align>(dst + offset, _mm_add_ps(Load<align>(src0 + offset), _mm_mul_ps(Load<align>(src1 + offset), Load<align>(src2 + offset))));
        }

        template <bool align> SIMD_INLINE void SynetFusedLayerForward8(const float * src0, const float * src1, const __m128 & src2, float * dst, size_t offset)
        {
            Store<align>(dst + offset, _mm_add_ps(Load<align>(src0 + offset), _mm_mul_ps(Load<align>(src1 + offset), src2)));
        }

        template <bool align> void SynetFusedLayerForward8(const float * src0, const float * src1, const float * src2, size_t count, size_t size, float * dst, SimdBool trans)
        {
            if (align)
                assert(((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(src2) : Aligned(size)) && Aligned(src0) && Aligned(src1) && Aligned(dst));
            if ((trans || size == 1) && count != 1)
            {
                size_t aligned = AlignLo(count, QF);
                size_t partial = AlignLo(count, F);
                for (size_t j = 0; j < size; ++j)
                {
                    size_t i = 0;
                    if (partial)
                    {
                        for (; i < aligned; i += QF)
                        {
                            SynetFusedLayerForward8<align>(src0, src1, src2, dst, i + 0 * F);
                            SynetFusedLayerForward8<align>(src0, src1, src2, dst, i + 1 * F);
                            SynetFusedLayerForward8<align>(src0, src1, src2, dst, i + 2 * F);
                            SynetFusedLayerForward8<align>(src0, src1, src2, dst, i + 3 * F);
                        }
                        for (; i < partial; i += F)
                            SynetFusedLayerForward8<align>(src0, src1, src2, dst, i);
                    }
                    for (; i < count; ++i)
                        dst[i] = Base::SynetFusedLayerForward8(src0[i], src1[i], src2[i]);
                    src0 += count;
                    src1 += count;
                    dst += count;
                }
            }
            else
            {
                size_t aligned = AlignLo(size, QF);
                size_t partial = AlignLo(size, F);
                for (size_t i = 0; i < count; ++i)
                {
                    size_t j = 0;
                    if (partial)
                    {
                        __m128 _src2 = _mm_set1_ps(src2[i]);
                        for (; j < aligned; j += QF)
                        {
                            SynetFusedLayerForward8<align>(src0, src1, _src2, dst, j + 0 * F);
                            SynetFusedLayerForward8<align>(src0, src1, _src2, dst, j + 1 * F);
                            SynetFusedLayerForward8<align>(src0, src1, _src2, dst, j + 2 * F);
                            SynetFusedLayerForward8<align>(src0, src1, _src2, dst, j + 3 * F);
                        }
                        for (; j < partial; j += F)
                            SynetFusedLayerForward8<align>(src0, src1, _src2, dst, j);
                    }
                    for (; j < size; ++j)
                        dst[j] = Base::SynetFusedLayerForward8(src0[j], src1[j], src2[i]);
                    src0 += size;
                    src1 += size;
                    dst += size;
                }
            }
        }

        void SynetFusedLayerForward8(const float * src0, const float * src1, const float * src2, size_t count, size_t size, float * dst, SimdBool trans)
        {
            if (((trans || size == 1) && count != 1 ? Aligned(count) && Aligned(src2) : Aligned(size)) && Aligned(src0) && Aligned(src1) && Aligned(dst))
                SynetFusedLayerForward8<true>(src0, src1, src2, count, size, dst, trans);
            else
                SynetFusedLayerForward8<false>(src0, src1, src2, count, size, dst, trans);
        }

        //---------------------------------------------------------------------

        template <bool align> SIMD_INLINE void SynetFusedLayerForward9(const float * src, const float * scale, const float * bias, float * dst0, float * dst1, size_t offset)
        {
            __m128 _src = Load<align>(src + offset);
            Store<align>(dst0 + offset, _mm_max_ps(_mm_setzero_ps(), _mm_add_ps(_mm_mul_ps(_src, Load<align>(scale + offset)), Load<align>(bias + offset))));
            Store<align>(dst1 + offset, _src);
        }

        template <bool align> SIMD_INLINE void SynetFusedLayerForward9(const float * src, const float * scale, const float * bias, float * dst0, size_t offset)
        {
            __m128 _src = Load<align>(src + offset);
            Store<align>(dst0 + offset, _mm_max_ps(_mm_setzero_ps(), _mm_add_ps(_mm_mul_ps(_src, Load<align>(scale + offset)), Load<align>(bias + offset))));
        }

        template <bool align> SIMD_INLINE void SynetFusedLayerForward9(const float * src, const __m128 & scale, const __m128 & bias, float * dst0, float * dst1, size_t offset)
        {
            __m128 _src = Load<align>(src + offset);
            Store<align>(dst0 + offset, _mm_max_ps(_mm_setzero_ps(), _mm_add_ps(_mm_mul_ps(_src, scale), bias)));
            Store<align>(dst1 + offset, _src);
        }

        template <bool align> SIMD_INLINE void SynetFusedLayerForward9(const float * src, const __m128 & scale, const __m128 & bias, float * dst0, size_t offset)
        {
            __m128 _src = Load<align>(src + offset);
            Store<align>(dst0 + offset, _mm_max_ps(_mm_setzero_ps(), _mm_add_ps(_mm_mul_ps(_src, scale), bias)));
        }

        template<bool align> void SynetFusedLayerForward9(const float * src0, const float * src1, const float * scale0, const float * bias0, size_t count0, size_t count1, size_t size, float * dst0, float * dst1, SimdBool trans)
        {
            if (align)
                assert((trans || size == 1 ? Aligned(count0) && Aligned(count1) && Aligned(scale0) && Aligned(bias0) : Aligned(size)) && Aligned(src0) && Aligned(src1) && Aligned(dst0) && Aligned(dst1));
            const float * scale1 = scale0 + count0;
            const float * bias1 = bias0 + count0;
            if (trans || size == 1)
            {
                size_t aligned0 = AlignLo(count0, QF);
                size_t partial0 = AlignLo(count0, F);
                size_t aligned1 = AlignLo(count1, QF);
                size_t partial1 = AlignLo(count1, F);
                if (dst1)
                {
                    for (size_t j = 0; j < size; ++j)
                    {
                        size_t i = 0;
                        for (; i < aligned0; i += QF)
                        {
                            SynetFusedLayerForward9<align>(src0, scale0, bias0, dst0, dst1, i + 0 * F);
                            SynetFusedLayerForward9<align>(src0, scale0, bias0, dst0, dst1, i + 1 * F);
                            SynetFusedLayerForward9<align>(src0, scale0, bias0, dst0, dst1, i + 2 * F);
                            SynetFusedLayerForward9<align>(src0, scale0, bias0, dst0, dst1, i + 3 * F);
                        }
                        for (; i < partial0; i += F)
                            SynetFusedLayerForward9<align>(src0, scale0, bias0, dst0, dst1, i);
                        for (; i < count0; ++i)
                            dst0[i] = Base::SynetFusedLayerForward9(src0[i], scale0[i], bias0[i]), dst1[i] = src0[i];
                        src0 += count0; 
                        dst0 += count0; 
                        dst1 += count0;
                        i = 0;
                        for (; i < aligned1; i += QF)
                        {
                            SynetFusedLayerForward9<align>(src1, scale1, bias1, dst0, dst1, i + 0 * F);
                            SynetFusedLayerForward9<align>(src1, scale1, bias1, dst0, dst1, i + 1 * F);
                            SynetFusedLayerForward9<align>(src1, scale1, bias1, dst0, dst1, i + 2 * F);
                            SynetFusedLayerForward9<align>(src1, scale1, bias1, dst0, dst1, i + 3 * F);
                        }
                        for (; i < partial1; i += F)
                            SynetFusedLayerForward9<align>(src1, scale1, bias1, dst0, dst1, i);
                        for (; i < count1; ++i)
                            dst0[i] = Base::SynetFusedLayerForward9(src1[i], scale1[i], bias1[i]), dst1[i] = src1[i];
                        src1 += count1;
                        dst0 += count1;
                        dst1 += count1;
                    }
                }
                else
                {
                    for (size_t j = 0; j < size; ++j)
                    {
                        size_t i = 0;
                        for (; i < aligned0; i += QF)
                        {
                            SynetFusedLayerForward9<align>(src0, scale0, bias0, dst0, i + 0 * F);
                            SynetFusedLayerForward9<align>(src0, scale0, bias0, dst0, i + 1 * F);
                            SynetFusedLayerForward9<align>(src0, scale0, bias0, dst0, i + 2 * F);
                            SynetFusedLayerForward9<align>(src0, scale0, bias0, dst0, i + 3 * F);
                        }
                        for (; i < partial0; i += F)
                            SynetFusedLayerForward9<align>(src0, scale0, bias0, dst0, i);
                        for (; i < count0; ++i)
                            dst0[i] = Base::SynetFusedLayerForward9(src0[i], scale0[i], bias0[i]);
                        src0 += count0;
                        dst0 += count0;
                        i = 0;
                        for (; i < aligned1; i += QF)
                        {
                            SynetFusedLayerForward9<align>(src1, scale1, bias1, dst0, i + 0 * F);
                            SynetFusedLayerForward9<align>(src1, scale1, bias1, dst0, i + 1 * F);
                            SynetFusedLayerForward9<align>(src1, scale1, bias1, dst0, i + 2 * F);
                            SynetFusedLayerForward9<align>(src1, scale1, bias1, dst0, i + 3 * F);
                        }
                        for (; i < partial1; i += F)
                            SynetFusedLayerForward9<align>(src1, scale1, bias1, dst0, i);
                        for (; i < count1; ++i)
                            dst0[i] = Base::SynetFusedLayerForward9(src1[i], scale1[i], bias1[i]);
                        src1 += count1;
                        dst0 += count1;
                    }
                }
            }
            else
            {
                size_t aligned = AlignLo(size, QF);
                size_t partial = AlignLo(size, F);
                if (dst1)
                {
                    for (size_t i = 0; i < count0; ++i, src0 += size, dst0 += size, dst1 += size)
                    {
                        size_t j = 0;
                        if (partial)
                        {
                            __m128 _scale0 = _mm_set1_ps(scale0[i]);
                            __m128 _bias0 = _mm_set1_ps(bias0[i]);
                            for (; j < aligned; j += QF)
                            {
                                SynetFusedLayerForward9<align>(src0, _scale0, _bias0, dst0, dst1, j + 0 * F);
                                SynetFusedLayerForward9<align>(src0, _scale0, _bias0, dst0, dst1, j + 1 * F);
                                SynetFusedLayerForward9<align>(src0, _scale0, _bias0, dst0, dst1, j + 2 * F);
                                SynetFusedLayerForward9<align>(src0, _scale0, _bias0, dst0, dst1, j + 3 * F);
                            }
                            for (; j < partial; j += F)
                                SynetFusedLayerForward9<align>(src0, _scale0, _bias0, dst0, dst1, j);
                        }
                        for (; j < size; ++j)
                            dst0[j] = Base::SynetFusedLayerForward9(src0[j], scale0[i], bias0[i]), dst1[j] = src0[j];
                    }
                    for (size_t i = 0; i < count1; ++i, src1 += size, dst0 += size, dst1 += size)
                    {
                        size_t j = 0;
                        if (partial)
                        {
                            __m128 _scale1 = _mm_set1_ps(scale1[i]);
                            __m128 _bias1 = _mm_set1_ps(bias1[i]);
                            for (; j < aligned; j += QF)
                            {
                                SynetFusedLayerForward9<align>(src1, _scale1, _bias1, dst0, dst1, j + 0 * F);
                                SynetFusedLayerForward9<align>(src1, _scale1, _bias1, dst0, dst1, j + 1 * F);
                                SynetFusedLayerForward9<align>(src1, _scale1, _bias1, dst0, dst1, j + 2 * F);
                                SynetFusedLayerForward9<align>(src1, _scale1, _bias1, dst0, dst1, j + 3 * F);
                            }
                            for (; j < partial; j += F)
                                SynetFusedLayerForward9<align>(src1, _scale1, _bias1, dst0, dst1, j);
                        }
                        for (; j < size; ++j)
                            dst0[j] = Base::SynetFusedLayerForward9(src1[j], scale1[i], bias1[i]), dst1[j] = src1[j];
                    }
                }
                else
                {
                    for (size_t i = 0; i < count0; ++i, src0 += size, dst0 += size)
                    {
                        size_t j = 0;
                        if (partial)
                        {
                            __m128 _scale0 = _mm_set1_ps(scale0[i]);
                            __m128 _bias0 = _mm_set1_ps(bias0[i]);
                            for (; j < aligned; j += QF)
                            {
                                SynetFusedLayerForward9<align>(src0, _scale0, _bias0, dst0, j + 0 * F);
                                SynetFusedLayerForward9<align>(src0, _scale0, _bias0, dst0, j + 1 * F);
                                SynetFusedLayerForward9<align>(src0, _scale0, _bias0, dst0, j + 2 * F);
                                SynetFusedLayerForward9<align>(src0, _scale0, _bias0, dst0, j + 3 * F);
                            }
                            for (; j < partial; j += F)
                                SynetFusedLayerForward9<align>(src0, _scale0, _bias0, dst0, j);
                        }
                        for (; j < size; ++j)
                            dst0[j] = Base::SynetFusedLayerForward9(src0[j], scale0[i], bias0[i]);
                    }
                    for (size_t i = 0; i < count1; ++i, src1 += size, dst0 += size)
                    {
                        size_t j = 0;
                        if (partial)
                        {
                            __m128 _scale1 = _mm_set1_ps(scale1[i]);
                            __m128 _bias1 = _mm_set1_ps(bias1[i]);
                            for (; j < aligned; j += QF)
                            {
                                SynetFusedLayerForward9<align>(src1, _scale1, _bias1, dst0, j + 0 * F);
                                SynetFusedLayerForward9<align>(src1, _scale1, _bias1, dst0, j + 1 * F);
                                SynetFusedLayerForward9<align>(src1, _scale1, _bias1, dst0, j + 2 * F);
                                SynetFusedLayerForward9<align>(src1, _scale1, _bias1, dst0, j + 3 * F);
                            }
                            for (; j < partial; j += F)
                                SynetFusedLayerForward9<align>(src1, _scale1, _bias1, dst0, j);
                        }
                        for (; j < size; ++j)
                            dst0[j] = Base::SynetFusedLayerForward9(src1[j], scale1[i], bias1[i]);
                    }
                }
            }
        }

        void SynetFusedLayerForward9(const float * src0, const float * src1, const float * scale0, const float * bias0, size_t count0, size_t count1, size_t size, float * dst0, float * dst1, SimdBool trans)
        {
            if ((trans || size == 1 ? Aligned(count0) && Aligned(count1) && Aligned(scale0) && Aligned(bias0) : Aligned(size)) && Aligned(src0) && Aligned(src1) && Aligned(dst0) && Aligned(dst1))
                SynetFusedLayerForward9<true>(src0, src1, scale0, bias0, count0, count1, size, dst0, dst1, trans);
            else
                SynetFusedLayerForward9<false>(src0, src1, scale0, bias0, count0, count1, size, dst0, dst1, trans);
        }
    }
#endif// SIMD_SSE_ENABLE
}