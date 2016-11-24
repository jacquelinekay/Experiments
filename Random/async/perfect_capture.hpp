#pragma once

#include <tuple>
#include <thread>
#include <mutex>
#ifdef __MINGW32__
#include <mingw.thread.h>
#include <mingw.mutex.h>
#include <mingw.condition_variable.h>
#endif  // __MINGW32__
#include <ecst/utils.hpp>
#include <utility>
#include <type_traits>

#define STATIC_ASSERT_SAME_TYPE(T0, T1) static_assert(std::is_same<T0, T1>{})
#define STATIC_ASSERT_SAME(a, T) STATIC_ASSERT_SAME_TYPE(decltype(a), T)

STATIC_ASSERT_SAME(FWD(std::declval<int>()), int&&);
STATIC_ASSERT_SAME(FWD(std::declval<int&>()), int&);
STATIC_ASSERT_SAME(FWD(std::declval<int&&>()), int&&);

template <typename T>
struct value_or_rvalue { using type = T; };

template <typename T>
struct value_or_rvalue<T&&> { using type = T; };

template <typename T>
using value_or_rvalue_t = typename value_or_rvalue<T>::type;

STATIC_ASSERT_SAME_TYPE(value_or_rvalue_t<int>, int);
STATIC_ASSERT_SAME_TYPE(value_or_rvalue_t<int&>, int&);
STATIC_ASSERT_SAME_TYPE(value_or_rvalue_t<int&&>, int);

template <typename T>
struct perfect_capture
{
    static_assert(!std::is_rvalue_reference<T>{});
    static_assert(!std::is_lvalue_reference<T>{});

private:
    T _x;

public:
    constexpr perfect_capture(T&& x)
        noexcept(std::is_nothrow_move_constructible<T>{})
        : _x{std::move(x)} { }

    constexpr perfect_capture(perfect_capture&& rhs)
        noexcept(std::is_nothrow_move_constructible<T>{})
        : _x{std::move(rhs._x)}
    {
    }

    constexpr perfect_capture& operator=(perfect_capture&& rhs)
        noexcept(std::is_nothrow_move_assignable<T>{})
    {
        _x = std::move(rhs._x);
        return *this;
    }

    // Prevent copies.
    perfect_capture(const perfect_capture&) = delete;
    perfect_capture& operator=(const perfect_capture&) = delete;

    constexpr auto& get() & noexcept { return _x; }
    constexpr const auto& get() const& noexcept { return _x; }

    constexpr operator T&() & noexcept { return _x; }
    constexpr operator const T&() const& noexcept { return _x; }

    constexpr auto get() && noexcept(std::is_nothrow_move_constructible<T>{}) { return std::move(_x); }
    constexpr operator T&&() && noexcept(std::is_nothrow_move_constructible<T>{}) { return std::move(_x); }
};

template <typename T>
struct perfect_capture<T&>
{
    static_assert(!std::is_rvalue_reference<T>{});

private:
    std::reference_wrapper<T> _x;

public:
    constexpr perfect_capture(T& x) noexcept : _x{x} { }

    constexpr perfect_capture(perfect_capture&& rhs) noexcept : _x{rhs._x}
    {
    }

    constexpr perfect_capture& operator=(perfect_capture&& rhs) noexcept
    {
        _x = rhs._x;
        return *this;
    }

    // Prevent copies.
    perfect_capture(const perfect_capture&) = delete;
    perfect_capture& operator=(const perfect_capture&) = delete;

    constexpr auto& get() & noexcept { return _x.get(); }
    constexpr const auto& get() const& noexcept { return _x.get(); }

    constexpr operator T&() & noexcept { return _x.get(); }
    constexpr operator const T&() const& noexcept { return _x.get(); }
};

template <typename T>
auto fwd_capture(T&& x)
{
    return perfect_capture<T>(FWD(x));
}

template <typename... Ts>
auto fwd_capture_as_tuple(Ts&&... xs)
{
    return std::make_tuple(fwd_capture(xs)...);
}

STATIC_ASSERT_SAME(fwd_capture(1), perfect_capture<int>);
STATIC_ASSERT_SAME(fwd_capture(std::declval<int>()), perfect_capture<int>);
STATIC_ASSERT_SAME(fwd_capture(std::declval<int&>()), perfect_capture<int&>);
STATIC_ASSERT_SAME(fwd_capture(std::declval<int&&>()), perfect_capture<int>);

#if defined(STATIC_TESTS)
namespace
{
    void  test0()[[maybe_unused]]
    {
        int x;
        auto p = fwd_capture(x);
        STATIC_ASSERT_SAME(p, perfect_capture<int&>);
    }

    void test1()[[maybe_unused]]
    {
        int x;
        auto p = fwd_capture(std::move(x));
        STATIC_ASSERT_SAME(p, perfect_capture<int>);
    }

    void test2()[[maybe_unused]]
    {
        auto p = fwd_capture(1);
        STATIC_ASSERT_SAME(p, perfect_capture<int>);
    }
}
#endif
