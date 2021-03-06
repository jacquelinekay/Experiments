// Copyright (c) 2013-2015 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0

#ifndef SSVVM_PARAMS
#define SSVVM_PARAMS

namespace ssvvm
{
    namespace Impl
    {
        template <std::size_t TIdx, typename TArg, typename T>
        inline void arrayFillHelper(T& mArray, const TArg& mArg)
        {
            mArray[TIdx] = Value::create<TArg>(mArg);
        }
        template <std::size_t TIdx, typename TArg, typename... TArgs,
            typename T>
        inline void arrayFillHelper(
            T& mArray, const TArg& mArg, const TArgs&... mArgs)
        {
            arrayFillHelper<TIdx>(mArray, mArg);
            arrayFillHelper<TIdx + 1>(mArray, FWD(mArgs)...);
        }
    }

    class Params
    {
    public:
        static constexpr std::size_t valueCount{3};

    private:
        std::array<Value, valueCount> values;

    public:
        inline Params() = default;
        template <typename... TArgs>
        inline Params(const TArgs&... mArgs) noexcept
        {
            Impl::arrayFillHelper<0>(values, mArgs...);
        }

        inline Value& operator[](std::size_t mIdx) noexcept
        {
            SSVU_ASSERT(mIdx < valueCount);
            return values[mIdx];
        }
        inline const Value& operator[](std::size_t mIdx) const noexcept
        {
            SSVU_ASSERT(mIdx < valueCount);
            return values[mIdx];
        }
    };
}

#endif
