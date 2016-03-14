/* Copyright (c) 2016 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#ifndef BOOST_HTTP_ROUTER_HPP
#define BOOST_HTTP_ROUTER_HPP

#include <cstdint>

#include <functional>
#include <string>
#include <regex>
#include <tuple>

#include <boost/http/headers.hpp>
#include <boost/http/traits.hpp>

namespace boost {
namespace http {

template<class Headers, class Body, class... Tail>
struct declare_message
{
    typedef Headers headers_type;
    typedef Body body_type;

    headers_type &headers() { return headers_; }

    const headers_type &headers() const { return headers_; }

    body_type &body() { return body_; }

    const body_type &body() const { return body_; }

    headers_type &trailers() { return trailers_; }

    const headers_type &trailers() const { return trailers_; }

    std::tuple<typename Tail::type...> stub; // blob, dummy...?

private:
    headers_type headers_;
    body_type body_;
    headers_type trailers_;
};

template<class Headers, class Body, class... T>
struct is_message<declare_message<Headers, Body, T...>>: public std::true_type
{};

namespace static_router {

namespace detail {

struct router_indexer
{
    void *handler_index = NULL;
};

template<class Message>
class searcher
{
public:
    virtual void do_thing(std::string url, Message &imessage) = 0;
};

template<class T, size_t Idx, class T1, class... Types>
struct tuple_get_impl
{
    static constexpr size_t result = tuple_get_impl<T, Idx + 1, Types...>::result;
};

template<class T, size_t Idx, class... Types>
struct tuple_get_impl<T, Idx, T, Types...>
{
    static constexpr size_t result = Idx;
};

template<class T, class... Types>
T& tuple_get(std::tuple<Types...> &t)
{
    return std::get<tuple_get_impl<T, 0, Types...>::result>(t);
}

template<class M>
void*& get_handler_index(M &imessage)
{
    return tuple_get<router_indexer>(imessage.stub).handler_index;
}

template<class M, size_t idx, class F, class NextRouter>
struct router
{
    typedef router<M, idx, F, NextRouter> Self;

    router(const char *rule, F functor, NextRouter next_router)
        : our_searcher(*this)
        , rule(rule)
        , functor(std::move(functor))
        , next_router(std::move(next_router))
    {}

    template<class F2>
    router<M, idx+1, F2, Self> append_route(const char *rule,
                                            F2 functor) const &&
    {
        return router<M, idx+1, F2, Self>(rule, std::move(functor),
                                          std::move(*this));
    }

    void do_easy_thing(const std::string &url, M &imessage)
    {
        // it should replaced with a regex whose state machine is created at
        // compile-time
        std::regex self_regex(rule, std::regex_constants::extended);
        if (!std::regex_search(url, self_regex)) {
            return next_router.do_easy_thing(url, imessage);
        }

        get_handler_index(imessage) = static_cast<searcher<M>*>(&our_searcher);
        functor(url, imessage);
    }

private:
    struct our_searcher_t: searcher<M>
    {
        our_searcher_t(Self &self)
            : self(self)
        {}

        void do_thing(std::string url, M &imessage) override
        {
            self.next_router.do_easy_thing(url, imessage);
        }

    private:
        Self &self;
    } our_searcher;

    const char *rule;
    F functor;
    NextRouter next_router;
};

template<class M>
struct router<M, 0, void, void>
{
    typedef router<M, 0, void, void> Self;

    template<class F>
    router<M, 1, F, Self> append_route(const char *rule, F functor) const &&
    {
        return router<M, 1, F, Self>(rule, std::move(functor),
                                     std::move(*this));
    }

    void do_easy_thing(const std::string &/*url*/, M &/*imessage*/)
    {
        // TODO: what to do with not found router?
    }
};

} // namespace detail {

struct with_index
{
    typedef detail::router_indexer type;
};

template<class M>
detail::router<M, 0, void, void> make()
{
    return detail::router<M, 0, void, void>();
}

template<class M>
void next(std::string url, M &imessage)
{
    auto searcher = reinterpret_cast<detail::searcher<M>*>
        (detail::get_handler_index(imessage));
    searcher->do_thing(url, imessage);
}

inline void done()
{
    // for now, it does nothing
}

} // namespace static_router {

} // namespace http
} // namespace boost

#endif // BOOST_HTTP_ROUTER_HPP
