#include "assertion.hpp"
#include "log.hpp"
#include <iostream>

void AssertionCustomFail(libassert::assert_type type, ASSERTION fatal, const libassert::assertion_printer& printer)
{
    std::string Message = printer(0);

    if(fatal == ASSERTION::FATAL)
    {
        LOG_ERROR("{}", Message);
        abort();
    }
    else
    {
        LOG_WARNING("{}", Message);
    }
}
