#if !defined DACHS_FATAL_HPP_INCLUDED
#define      DACHS_FATAL_HPP_INCLUDED

#include <cstddef>
#include <cstddef>
#include <iostream>

namespace dachs {
namespace fatal {

[[noreturn]] inline
void internal_compilation_error(
        char const* const file,
        char const* const func,
        std::size_t const line,
        std::ostream &output = std::cerr)
{
    output << "Internal compilation error at file:" << file << " function:" << func << " line:" << line << std::endl;
    std::abort();
}

#define DACHS_RAISE_INTERNAL_COMPILATION_ERROR \
    ::dachs::fatal::internal_compilation_error(__FILE__, __func__, __LINE__);

} // namespace fatal
} // namespace dachs

#endif    // DACHS_FATAL_HPP_INCLUDED
