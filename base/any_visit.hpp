#pragma once
#ifndef HSBA_SLICER_ANY_VISIT_HPP
#define HSBA_SLICER_ANY_VISIT_HPP

#include <any>
#include <variant>
#include <type_traits>

#include <boost/any.hpp>

#include "template_helper.hpp"

namespace HsBa::Slicer::Utils
{

    // visit any
    // ref: https://codereview.stackexchange.com/questions/275440/visit-for-stdany

    /// <summary>
    /// visit std::any
    /// </summary>
    /// <typeparam name="...Ts">visited types</typeparam>
    /// <typeparam name="Callback">callback function</typeparam>
    /// <typeparam name="...Args">other arguments</typeparam>
    /// <param name="callback">callback function</param>
    /// <param name="any">any</param>
    /// <param name="...args">other arguments</param>
    /// <returns>return value of callback function, void if callback function returns void</returns>
    template <typename... Ts, typename Callback, typename... Args>
    inline auto Visit(Callback&& callback, const std::any& any, Args&&... args)
    {
        using R = typename AllTheSame<
            decltype(std::declval<Callback>()(std::declval<Ts>(), std::declval<Args>()...))...
        >::type;
        if constexpr (std::is_void_v<R>) // callback returns void
        {
            int dummy[] = {
            [&]() {
                if (any.type() == typeid(Ts)) {
                    std::forward<Callback>(callback)(
                        std::any_cast<Ts>(any),
                        std::forward<Args>(args)...
                    );
                }
                return 0;
            }() ...
            };
            (void)dummy;
            return;
        }
        else
        {
            R ret{};
            int dummy[] = {
                [&]() {
                    if (any.type() == typeid(Ts)) {
                        ret = std::forward<Callback>(callback)(
                            std::any_cast<Ts>(any),
                            std::forward<Args>(args)...
                        );
                    }
                    return 0;
                }() ...
            };
            (void)dummy;
            return ret;
        }
    }

    /// <summary>
    /// visit std::any
    /// </summary>
    /// <typeparam name="...Ts">visited types</typeparam>
    /// <typeparam name="Callback">callback function</typeparam>
    /// <typeparam name="...Args">other arguments</typeparam>
    /// <param name="callback">callback function</param>
    /// <param name="any">any</param>
    /// <param name="...args">other arguments</param>
    /// <returns>return value of callback function, void if callback function returns void</returns>
    template <typename... Ts, typename Callback, typename... Args>
    inline auto Visit(Callback&& callback, const boost::any& any, Args&&... args)
    {
        using R = typename AllTheSame<
            decltype(std::declval<Callback>()(std::declval<Ts>(), std::declval<Args>()...))...
        >::type;
        if constexpr (std::is_void_v<R>) // callback returns void
        {
            int dummy[] = {
            [&]() {
                if (any.type() == typeid(Ts)) {
                    std::forward<Callback>(callback)(
                        boost::any_cast<Ts>(any),
                        std::forward<Args>(args)...
                    );
                }
                return 0;
            }() ...
            };
            (void)dummy;
            return;
        }
        else
        {
            R ret{};
            int dummy[] = {
                [&]() {
                    if (any.type() == typeid(Ts)) {
                        ret = std::forward<Callback>(callback)(
                            boost::any_cast<Ts>(any),
                            std::forward<Args>(args)...
                        );
                    }
                    return 0;
                }() ...
            };
            (void)dummy;
            return ret;
        }
    }
}// namespace HsBa::Slicer::Utils

#endif // !HSBA_SLICER_ANY_VISIT_HPP
