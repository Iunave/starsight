#include "assertion.hpp"
#include "log.hpp"
#include <iostream>

void AssertionCustomFail(libassert::assert_type type, ASSERTION fatal, const libassert::assertion_printer& printer)
{
    std::string Message = printer(0);
    LOG_ERROR("{}", Message);

    if(fatal == ASSERTION::FATAL)
    {
        throw libassert::verification_failure();
    }
}
