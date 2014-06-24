#if !defined DACHS_HELPER_MAKE_OVERLOAD_HPP_INCLUDED
#define      DACHS_HELPER_MAKE_OVERLOAD_HPP_INCLUDED

namespace dachs {
namespace helper {

template<class F, class... FS>
struct overloaded_function : F, overloaded_function<FS...> {
    using F::operator();
    using overloaded_function<FS...>::operator();

    overloaded_function(F const& f, FS... fs) noexcept
        : F(f), overloaded_function<FS...>(fs...)
    {}
};

template<class F>
struct overloaded_function<F> : F {
    using F::operator();

    overloaded_function(F const& f) noexcept
        : F(f)
    {}
};

template<class... FS>
inline auto make_overload(FS const&... fs)
{
    return overloaded_function<FS...>(fs...);
}

} // namespace helper
} // namespace dachs

#endif    // DACHS_HELPER_MAKE_OVERLOAD_HPP_INCLUDED
