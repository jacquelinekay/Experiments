#include <atomic>
#include <chrono>
#include <cstdio>
#include <ecst/thread_pool.hpp>
#include <ecst/utils.hpp>
#include <functional>
#include <thread>

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

    struct context
    {
        pool& _p;
        context(pool& p) : _p{p}
        {
        }

        template <typename TF>
        auto build(TF f);
    };

    struct base_node
    {
        context& _ctx;
        base_node(context& ctx) : _ctx{ctx}
        {
        }
    };

    struct root : base_node
    {
        using base_node::base_node;

        template <typename TNode, typename... TNodes>
        void start(TNode& n, TNodes&... ns)
        {
            n.execute(ns...);
        }
    };

    template <typename TParent, typename TF>
    struct node_then : base_node
    {
        using this_type = node_then<TParent, TF>;

        TParent _p;
        TF _f;

        node_then(context& ctx, TParent&& p, TF&& f)
            : base_node{ctx}, _p{p}, _f{f}
        {
        }

        auto execute()
        {
            _ctx._p.post(_f);
        }

        template <typename TNode, typename... TNodes>
        auto execute(TNode& n, TNodes&... ns)
        {
            _ctx._p.post([&] {
                _f();
                _ctx._p.post([&] { n.execute(ns...); });
            });
        }

        template <typename TCont>
        auto then(TCont cont)
        {
            return node_then<this_type, TCont>{
                this->_ctx, std::move(*this), std::move(cont)};
        }

        template <typename... TNodes>
        auto start(TNodes&... ns)
        {
            _p.start(*this, ns...);
        }
    };



    template <typename TF>
    auto context::build(TF f)
    {
        return node_then<root, TF>(*this, root{*this}, std::move(f));
    }
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

    auto computation = ctx.build([] { ll::print_sleep_ms(250, "A"); })
                           .then([] { ll::print_sleep_ms(250, "B"); })
                           .then([] { ll::print_sleep_ms(250, "C"); });

    execute_after_move(std::move(computation));
}
