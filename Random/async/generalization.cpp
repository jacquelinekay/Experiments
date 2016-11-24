#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <mutex>
#ifdef __MINGW_32__
#include <mingw.thread.h>
#include <mingw.mutex.h>
#include <mingw.condition_variable.h>
#endif
#include <ecst/thread_pool.hpp>
#include <ecst/utils.hpp>
#include <experimental/tuple>
#include <functional>
#include <thread>
#include <tuple>
#include "perfect_capture.hpp"
#include "nothing.hpp"

#define IF_CONSTEXPR \
    if               \
    constexpr

namespace ll
{
    using pool = ecst::thread_pool;

    inline void sleep_ms(int ms)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    template <typename T>
    inline void print_sleep_ms(int ms, const T& x)
    {
        std::puts(x);
        sleep_ms(ms);
    }

    template <typename TDerived>
    struct continuable
    {
        using this_type = TDerived;

        auto& as_derived() & noexcept
        {
            return static_cast<TDerived&>(*this);
        }

        const auto& as_derived() const & noexcept
        {
            return static_cast<TDerived&>(*this);
        }

        auto as_derived() && noexcept
        {
            return std::move(as_derived());
        }

        template <typename... TConts>
        auto then(TConts&&... conts) &;

        template <typename... TConts>
        auto then(TConts&&... conts) &&;
    };

    struct context
    {
        pool& _p;
        context(pool& p) : _p{p}
        {
        }

        template <typename TF>
        auto build(TF&& f);

        template <typename ...TConts>
        auto compose(TConts&&... conts);
    };

    struct base_node
    {
        context& _ctx;
        base_node(context& ctx) noexcept : _ctx{ctx}
        {
        }
    };

    template <typename TReturn>
    struct root : base_node
    {
        using return_type = TReturn;
        using base_node::base_node;

        template <typename TNode, typename... TNodes>
        void start(TNode& n, TNodes&... ns)
        {
            n.execute(nothing, ns...);
        }

        auto& ctx() & noexcept
        {
            return this->_ctx;
        }

        const auto& ctx() const & noexcept
        {
            return this->_ctx;
        }
    };

    template <typename TParent>
    struct child_of
    {
        TParent _p;

        template <typename TParentFwd>
        child_of(TParentFwd&& p) : _p{FWD(p)}
        {
        }

        auto& parent() & noexcept
        {
            return this->_p;
        }

        const auto& parent() const & noexcept
        {
            return this->_p;
        }

        auto parent() && noexcept
        {
            return std::move(this->_p);
        }

        auto& ctx() & noexcept
        {
            return parent().ctx();
        }

        const auto& ctx() const & noexcept
        {
            return parent().ctx();
        }
    };

    template <typename TParent, typename TF>
    struct node_then : child_of<TParent>,
                       continuable<node_then<TParent, TF>>,
                       TF
    {
        using this_type = node_then<TParent, TF>;

        // TODO: might be useful?
        /*
        using input_type = void_to_nothing_t<typename TParent::return_type>;
        using return_type = decltype(call_ignoring_nothing(
            std::declval<TF>(), std::declval<input_type>()));
        */

        auto& as_f() noexcept
        {
            return static_cast<TF&>(*this);
        }

        template <typename TParentFwd, typename TFFwd>
        node_then(TParentFwd&& p, TFFwd&& f)
            : child_of<TParent>{FWD(p)}, TF{FWD(f)}
        {
        }

        template <typename T>
        auto execute(T&& x) &
        {
            this->ctx()._p.post([ this, x = FWD(x) ]() mutable {
                with_void_to_nothing(as_f(), FWD(x));
            });
        }

        template <typename T, typename TNode, typename... TNodes>
        auto execute(T&& x, TNode& n, TNodes&... ns) &
        {
            this->ctx()._p.post([ this, x = FWD(x), &n, &ns... ]() mutable {
                decltype(auto) res = with_void_to_nothing(as_f(), FWD(x));

                this->ctx()._p.post(
                    [ res = std::move(res), &n, &ns... ]() mutable {
                        n.execute(std::move(res), ns...);
                    });
            });
        }

        template <typename... TNodes>
        auto start(TNodes&... ns)
        {
            this->parent().start(*this, ns...);
        }
    };

    template <typename T>
    struct movable_atomic : std::atomic<T>
    {
        using base_type = std::atomic<T>;
        using base_type::base_type;

        movable_atomic(movable_atomic&& r) noexcept : base_type{r.load()}
        {
        }

        movable_atomic& operator=(movable_atomic&& r) noexcept
        {
            this->store(r.load());
            return *this;
        }
    };

    template <typename TParent, typename... TFs>
    struct node_wait_all : child_of<TParent>,
                           continuable<node_wait_all<TParent, TFs...>>,
                           TFs...
    {
        using this_type = node_wait_all<TParent, TFs...>;

        movable_atomic<int> _ctr{sizeof...(TFs)};

        template <typename TParentFwd, typename... TFFwds>
        node_wait_all(TParentFwd&& p, TFFwds&&... fs)
            : child_of<TParent>{FWD(p)}, TFs{FWD(fs)}...
        {
        }

        template <typename TParentFwd, typename... TConts>
        static compose_from_continuables(TParentFwd&& p, TConts&&... conts)
        {
            // return 
            return node_wait_all(p, result);
        }

        auto execute() &
        {
            //(this->ctx()._p.post([&] { static_cast<TFs&> (*this)(); }), ...);
        }

        template <typename TNode, typename... TNodes>
        auto execute(TNode& n, TNodes&... ns) &
        {
            auto exec = [this, &n, &ns...](auto& f) {
                this->ctx()._p.post([&] {
                    f();
                    if(--_ctr == 0)
                    {
                        n.execute(ns...);
                    }
                });
            };

            // TODO: gcc bug?
            //    (exec(static_cast<TFs&>(*this)), ...);
        }

        template <typename... TNodes>
        auto start(TNodes&... ns) &
        {
            this->parent().start(*this, ns...);
        }
    };
    /*
        template <typename TParent, typename TReturn, typename TF>
        template <typename... TConts>
        auto node_then<TParent, TReturn, TF>::wait_all(TConts&&... conts) &&
        {
            return node_wait_all<this_type, TConts...>{
                std::move(*this), FWD(conts)...};
        }
    */
    template <typename TF>
    auto context::build(TF&& f)
    {
        using return_type = void; // decltype(f());
        using root_type = root<return_type>;
        return node_then<root_type, TF>(root_type{*this}, FWD(f));
    }

    // compose multiple continuables
    template<typename... TConts>
    auto context::compose(TConts&&... conts)
    {
    }


    template <typename TDerived>
    template <typename... TConts>
    auto continuable<TDerived>::then(TConts&&... conts) &
    {
        IF_CONSTEXPR(sizeof...(conts) == 0)
        {
            static_assert("u wot m8");
        }
        else IF_CONSTEXPR(sizeof...(conts) == 1)
        {
            return node_then<this_type, TConts...>{as_derived(), FWD(conts)...};
        }
        else
        {
            return node_wait_all<this_type, TConts...>{
                as_derived(), FWD(conts)...};
        }
    }

    template <typename TDerived>
    template <typename... TConts>
    auto continuable<TDerived>::then(TConts&&... conts) &&
    {
        IF_CONSTEXPR(sizeof...(conts) == 0)
        {
            static_assert("u wot m8");
        }
        else IF_CONSTEXPR(sizeof...(conts) == 1)
        {
            return node_then<this_type, TConts...>{
                std::move(as_derived()), FWD(conts)...};
        }
        else
        {
            return node_wait_all<this_type, TConts...>{
                std::move(as_derived()), FWD(conts)...};
        }
    }
}

template<typename... TChains>
auto wait_until_complete(ll::context& ctx, TChains&&...chains)
{
    ecst::latch l{1};
    auto final_chain = ctx.compose(FWD(chains)...).then([&]{ l.count_down(); });
    final_chain.start();
    l.wait();
}

template <typename T>
void execute_after_move(T x)
{
    x.start();
    ll::sleep_ms(1200);
}

int main()
{
    ll::pool p;
    ll::context ctx{p};

    auto computation =               // .
        ctx.build([] { return 10; }) // .
            .then([](int x) { return std::to_string(x); })
            .then([](std::string x) { return "num: " + x; })
            .then([](std::string x) { std::printf("%s\n", x.c_str()); });

    ctx.build([] { return 10; })
        .then([](int x) { return std::to_string(x); })
        .then([](std::string x) { return "num: " + x; })
        .then([](std::string x) { std::printf("%s\n", x.c_str()); })
        .start();

    /* TODO:
        ctx.build<int>([] { return 10; })
            .when_all<int, float>
            (
                // TODO: int? or const int&?
                // *(should the arg be copied across every task?)*
                [](int x) { return x + 1; },
                [](int x) { return x + 1.f; }
            )
            .then<void>([](std::tuple<int, float> x) { std::printf("%s\n",
       x.c_str()); })
            .start();
    */

    /* TODO:
        ctx.build<int>([] { return 10; })
            .when_any<int, float>
            (
                [](int x) { return x + 1; },
                [](int x) { return x + 1.f; }
            )
            .then<void>([](std::variant<int, float> x) { std::printf("%s\n",
       x.c_str()); })
            .start();
    */

    /* TODO:
        ctx.build<int>([] { return 10; })
            .then<int>([](int x) { return std::to_string(x); })
            .then_timeout<std::string>(200ms, [](std::string x) { return "num: "
       + x; })
            .then<void>([](std::optional<std::string> x) { std::printf("%s\n",
       x.c_str()); })
            .start();
    */

    // TODO: deduce return type when possible!
    // (generalize stuff from variant articles)
    // (generic lambdas are probably not deducible)
    /*
        if arity is deducible
            if arity is 0
                DEDUCE void
            else if arity is 1
                DEDUCE with function traits
    */

    execute_after_move(std::move(computation));
    ll::sleep_ms(200);
}
