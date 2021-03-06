// Copyright (c) 2013-2015 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0
// http://vittorioromeo.info | vittorio.romeo@outlook.com

#include "./shared.hpp"
#include "./legacy.hpp"
#include "./behavior.hpp"
#include "./resource_base.hpp"
#include "./unique_resource.hpp"
#include "./shared_resource.hpp"
#include "./weak.hpp"
#include "./make_resource.hpp"
#include "./access.hpp"
#include "./interface.hpp"
#include "./tests.hpp"
#include "./tests_runner.hpp"

#include <memory>


template <typename TAccess>
struct vbo_interface : TAccess
{
    using TAccess::TAccess;

    void my_interface_method_0()
    {
        [](auto x)
        {
            (void)x;
        }(this->get());
    }
};

template <typename TAccess>
struct ptr_interface : TAccess
{
    using TAccess::TAccess;

    auto& operator*() noexcept
    {
        return *(this->get());
    }

    const auto& operator*() const noexcept
    {
        return *(this->get());
    }

    auto operator-> () noexcept
    {
        return this->get();
    }

    auto operator-> () const noexcept
    {
        return this->get();
    }
};

// TODO: facade<interface, behavior, access>
// or facade<interface, behavior>
// facade<interface, behavior>::make<access::unmanaged>
// facade<interface, behavior>::make<access::unique>
// facade<interface, behavior>::make<access::shared>
// facade<interface, behavior>::make_unmanaged
// facade<interface, behavior>::make_unique
// facade<interface, behavior>::make_shared
// ?

using unmanaged_vbo = vbo_interface<access::unmanaged<behavior::vbo_b>>;
using unique_vbo = vbo_interface<access::unique<behavior::vbo_b>>;
using shared_vbo = vbo_interface<access::shared<behavior::vbo_b>>;

template <typename T>
using my_unmanaged_ptr =
    ptr_interface<access::unmanaged<behavior::free_store_b<T>>>;

template <typename T>
using my_unique_ptr = ptr_interface<access::unique<behavior::free_store_b<T>>>;

template <typename T>
using my_shared_ptr = ptr_interface<access::shared<behavior::free_store_b<T>>>;



template <typename... Ts>
auto make_unmanaged_vbo(Ts&&... xs)
{
    return make_unmanaged_interface<behavior::vbo_b, vbo_interface>(FWD(xs)...);
}

template <typename... Ts>
auto make_unique_vbo(Ts&&... xs)
{
    return make_unique_interface<behavior::vbo_b, vbo_interface>(FWD(xs)...);
}

template <typename... Ts>
auto make_shared_vbo(Ts&&... xs)
{
    return make_shared_interface<behavior::vbo_b, vbo_interface>(FWD(xs)...);
}

void example_ptrs()
{
    using bt = behavior::free_store_b<int>;

    // Unmanaged
    {
        my_unmanaged_ptr<int> p(bt::init(new int));
        *p = 10;

        assert(*p == 10);

        // TODO:
        // bt::deinit(p);
        // should this compile?

        bt::deinit(p.get());
    }

    // Unique
    {
        my_unique_ptr<int> p(bt::init(new int));
        *p = 10;

        assert(*p == 10);

        // Does not compile as intended:
        // my_unique_ptr<int> pp = p;

        my_unique_ptr<int> pp = std::move(p);
        assert(*pp == 10);
    }

    // Shared
    {
        my_shared_ptr<int> p(bt::init(new int));

        *p = 10;
        assert(*p == 10);

        my_shared_ptr<int> pp = p;
        assert(*p == 10);
        assert(*pp == 10);
    }
}

int main()
{
    test::run_all();
    example_ptrs();

    std::cout << sizeof(std::shared_ptr<int>) << "\n";
    std::cout << sizeof(std::weak_ptr<int>) << "\n";

    /*
    {
        auto x_vbo =
            make_unmanaged_interface<behavior::vbo_b, vbo_interface>(1);
        x_vbo.my_interface_method_0();
    }

    {
        auto u_vbo = make_unique_interface<behavior::vbo_b, vbo_interface>(1);
        u_vbo.my_interface_method_0();
    }

    {
        auto s_vbo = make_shared_interface<behavior::vbo_b, vbo_interface>(1);
        s_vbo.my_interface_method_0();
    }
    */

    // Unmanaged
    {
        auto x = make_unmanaged_vbo(1);
        x.my_interface_method_0();

        x.dispose();
    }

    // Unique
    {
        auto x = make_unique_vbo(1);
        x.my_interface_method_0();
        assert(x);

        // Does not compile, as intended:
        // auto y = x;

        // OK:
        auto y = std::move(x);
        y.my_interface_method_0();
        assert(y);
        assert(!x);
    }

    // Shared
    {
        auto x = make_shared_vbo(1);
        x.my_interface_method_0();
        assert(x);

        {
            // OK:
            auto y = x;
            assert(x);
            assert(y);

            auto z = x;
            assert(x);
            assert(y);
            assert(z);

            z.reset();

            assert(x);
            assert(y);
            assert(!z);
        }

        assert(x);
    }

    return 0;
}

// TODO:
// ??
// "interface" -> "wrapper"
// SFML RAII classes as resources
// does polymorphism work with unique and/or shared?
// sanitize tests with address and memory
// std::hash?
// thread-safe sptr
// weak_resource
// resource_base
// optimized and unsafe sptr methods
// * ex.: copy_ownership (assumes sptr owns something)
// should access::impl::resource derive? maybe just store resource as member?