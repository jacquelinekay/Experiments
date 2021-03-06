// Copyright (c) 2013-2015 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0

#ifndef SSVVM_COMMON
#define SSVVM_COMMON

namespace ssvvm
{
    template <typename T>
    using VMFnPtr = void (T::*)();

    // Built-in value types and conversions
    enum class VMVal
    {
        Void,
        Int,
        Float
    };

    template <typename T>
    inline constexpr VMVal getVMVal() noexcept;
    template <>
    inline constexpr VMVal getVMVal<void>() noexcept
    {
        return VMVal::Void;
    }
    template <>
    inline constexpr VMVal getVMVal<int>() noexcept
    {
        return VMVal::Int;
    }
    template <>
    inline constexpr VMVal getVMVal<float>() noexcept
    {
        return VMVal::Float;
    }

    template <typename T>
    inline int getTokenAsInt(T& mTokens, std::size_t mIdx)
    {
        return ssvu::sToInt(mTokens[mIdx].contents.c_str());
    }
    template <typename T>
    inline float getTokenAsFloat(T& mTokens, std::size_t mIdx)
    {
        return ssvu::sToFloat(
            mTokens[mIdx]
                .contents.substr(0, mTokens[mIdx].contents.size() - 2)
                .c_str());
    }
    template <typename T>
    inline const std::string& getTokenContents(T& mTokens, std::size_t mIdx)
    {
        return mTokens[mIdx].contents;
    }
}

#endif
