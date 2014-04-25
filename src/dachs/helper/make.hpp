#if !defined DACHS_HELPER_MAKE_HPP_INCLUDED
#define      DACHS_HELPER_MAKE_HPP_INCLUDED

#include <memory>

#include "dachs/helper/util.hpp"

namespace dachs {
namespace helper {

template<class Ptr, class... Args>
inline Ptr make(Args &&... args)
{
    static_assert(is_shared_ptr<Ptr>::value, "make<>(): Ptr is not shared_ptr.");
    return std::make_shared<typename Ptr::element_type>(std::forward<Args>(args)...);
}

} // namespace helper
} // namespace dachs

#endif    // DACHS_HELPER_MAKE_HPP_INCLUDED
