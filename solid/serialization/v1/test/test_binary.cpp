#include "solid/serialization/v1/binary.hpp"
#include <iostream>
#include <map>
#include <memory>
#include <vector>

using namespace solid;
using namespace std;

struct Test;
using TestPointerT = shared_ptr<Test>;

struct Test {
    using KeyValueVectorT = std::vector<std::pair<std::string, std::string>>;
    using MapT            = std::map<std::string, uint64_t>;

    std::string     str;
    KeyValueVectorT kv_vec;
    MapT            kv_map;
    uint32_t        v32;

    template <class S>
    void solidSerializeV1(S& _s)
    {
        _s.push(str, "Test::str");
        _s.pushContainer(kv_vec, "Test::kv_vec").pushContainer(kv_map, "Test::kv_map");
        _s.pushCross(v32, "Test::v32");
    }

    void init();
    void check() const;

    static TestPointerT create()
    {
        return make_shared<Test>();
    }
};

using SerializerT   = serialization::binary::Serializer<void>;
using DeserializerT = serialization::binary::Deserializer<void>;
using TypeIdMapT    = serialization::TypeIdMap<SerializerT, DeserializerT>;

int test_binary(int argc, char* argv[])
{
    solid::log_start(std::cerr, {".*:EWX"});

    string     test_data;
    TypeIdMapT typemap;

    typemap.registerType<Test>(0 /*protocol ID*/);

    { //serialization
        SerializerT ser(&typemap);
        const int   bufcp = 64;
        char        buf[bufcp];
        int         rv;

        shared_ptr<Test> test = Test::create();

        test->init();

        ser.push(test, "test");

        while ((rv = ser.run(buf, bufcp)) > 0) {
            test_data.append(buf, rv);
        }
    }
    { //deserialization
        DeserializerT des(&typemap);

        shared_ptr<Test> test;

        des.push(test, "test");

        int rv = des.run(test_data.data(), test_data.size());

        solid_check(rv == static_cast<int>(test_data.size()));
        test->check();
    }
    return 0;
}

namespace {
const pair<string, string> kv_array[] = {
    {"first_key", "first_value"},
    {"second_key", "secon_value"},
    {"third_key", "third_value"},
    {"fourth_key", "fourth_value"},
    {"fifth_key", "fifth_value"},
    {"sixth_key", "sixth_value"},
    {"seventh_key", "seventh_value"},
    {"eighth_key", "eighth_value"},
    {"nineth_key", "nineth_value"},
    {"tenth_key", "tenth_value"},
};

const size_t kv_array_size = sizeof(kv_array) / sizeof(pair<string, string>);

} //namespace

void Test::init()
{
    kv_vec.reserve(kv_array_size);

    for (size_t i = 0; i < kv_array_size; ++i) {
        str.append(kv_array[i].first);
        str.append(kv_array[i].second);
        kv_vec.push_back(kv_array[i]);
        kv_map[kv_array[i].first] = i;
    }
    v32 = str.size();
    check();
}

void Test::check() const
{
    solid_dbg(generic_logger, Info, "str = " << str);
    solid_check(kv_vec.size() == kv_map.size());
    string tmpstr;
    for (size_t i = 0; i < kv_vec.size(); ++i) {
        tmpstr.append(kv_array[i].first);
        tmpstr.append(kv_array[i].second);
        solid_check(kv_vec[i] == kv_array[i]);
        MapT::const_iterator it = kv_map.find(kv_array[i].first);
        solid_check(it != kv_map.end());
        solid_check(it->second == i);
    }
    solid_check(tmpstr == str);
    solid_check(str.size() == v32);
}
