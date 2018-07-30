#include "solid/system/common.hpp"
#include <atomic>
#include <iostream>

#include "solid/system/exception.hpp"
#include <vector>

#include "solid/serialization/v1/binary.hpp"

using namespace std;
using namespace solid;

using solid::serialization::binary::ExtendedData;

int test_extdata(int argc, char* argv[])
{
    ExtendedData e1(static_cast<uint32_t>(10));
    cout << e1.first_uint32_t_value() << endl;

    e1.generic(std::string("ceva"));

    cout << *e1.genericCast<std::string>() << endl;

    solid_check(e1.genericCast<std::vector<int>>() == nullptr);

    return 0;
}
