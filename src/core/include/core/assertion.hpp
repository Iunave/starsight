#ifndef STARSIGHT_ASSERTION_HPP
#define STARSIGHT_ASSERTION_HPP

#define ASSERT_FAIL AssertionCustomFail
#include "assert.hpp"

void AssertionCustomFail(libassert::assert_type type, ASSERTION fatal, const libassert::assertion_printer& printer);

#endif //STARSIGHT_ASSERTION_HPP
