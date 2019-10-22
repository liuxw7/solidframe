// solid/serialization/src/binary.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/serialization/v1/binary.hpp"
#include "solid/serialization/v1/binarybasic.hpp"
#include "solid/system/cstring.hpp"
#include "solid/utility/ioformat.hpp"
#include <atomic>
#include <cstddef>
#include <cstring>

namespace solid {
namespace serialization {
namespace binary {

const LoggerT logger{"solid::serialization::binary"};

/*static*/ Limits const& Limits::unlimited_instance()
{
    static const Limits l;
    return l;
}

/*static*/ size_t ExtendedData::newTypeId()
{
    static std::atomic<size_t> crtid(1);
    return crtid.fetch_add(1);
}
//========================================================================
/*static*/ const char* Base::default_name = "-";

class ErrorCategory : public ErrorCategoryT {
public:
    ErrorCategory() {}

private:
    const char* name() const noexcept(true)
    {
        return "solid::serialization::binary";
    }

    std::string message(int _ev) const
    {
        switch (_ev) {
        case Base::ERR_NOERROR:
            return "No error";
        case Base::ERR_ARRAY_LIMIT:
            return "Array limit";
        case Base::ERR_ARRAY_MAX_LIMIT:
            return "Array max limit";
        case Base::ERR_BITSET_SIZE:
            return "Destination bitset small";
        case Base::ERR_CONTAINER_LIMIT:
            return "Container limit";
        case Base::ERR_CONTAINER_MAX_LIMIT:
            return "Container max limit";
        case Base::ERR_STREAM_LIMIT:
            return "Stream limit";
        case Base::ERR_STREAM_CHUNK_MAX_LIMIT:
            return "Stream chunk max limit";
        case Base::ERR_STREAM_SEEK:
            return "Stream seek";
        case Base::ERR_STREAM_READ:
            return "Stream read";
        case Base::ERR_STREAM_WRITE:
            return "Stream write";
        case Base::ERR_STREAM_SENDER:
            return "Stream sender";
        case Base::ERR_STRING_LIMIT:
            return "String limit";
        case Base::ERR_STRING_MAX_LIMIT:
            return "String max limit";
        case Base::ERR_UTF8_LIMIT:
            return "Utf8 limit";
        case Base::ERR_UTF8_MAX_LIMIT:
            return "Utf8 max limit";
        case Base::ERR_POINTER_UNKNOWN:
            return "Unknown pointer type id";
        case Base::ERR_REINIT:
            return "Reinit error";
        case Base::ERR_NO_TYPE_MAP:
            return "Serializer/Deserializer not initialized with a TypeIdMap";
        case Base::ERR_DESERIALIZE_VALUE:
            return "pushCrossValue/pushValue cannot be used in Deserializer";
        case Base::ERR_CROSS_VALUE_SMALL:
            return "Stored cross integer, too big for load value";
        default:
            return "Unknown error";
        }
    }
};

const ErrorCategory ec;

/*static*/ ErrorConditionT Base::make_error(Errors _err)
{
    return ErrorConditionT(_err, ec);
}

/*static*/ ReturnValues Base::setStringLimit(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    _rb.lmts.stringlimit = static_cast<size_t>(_rfd.s);
    return SuccessE;
}
/*static*/ ReturnValues Base::setStreamLimit(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    _rb.lmts.streamlimit = _rfd.s;
    return SuccessE;
}
/*static*/ ReturnValues Base::setContainerLimit(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    _rb.lmts.containerlimit = static_cast<size_t>(_rfd.s);
    return SuccessE;
}
void Base::replace(const FncData& _rfd)
{
    fstk.top() = _rfd;
}
ReturnValues Base::popExtStack(Base& _rb, FncData&, void* /*_pctx*/)
{
    _rb.estk.pop();
    return SuccessE;
}

/*static*/ ReturnValues Base::noop(Base& _rd, FncData& _rfd, void* /*_pctx*/)
{
    if (_rfd.s) {
        --_rfd.s;
        return ContinueE;
    }
    return SuccessE;
}

//========================================================================
/*static*/ char* SerializerBase::storeValue(char* _pd, const uint8_t _val)
{
    return serialization::binary::store(_pd, _val);
}
/*static*/ char* SerializerBase::storeValue(char* _pd, const uint16_t _val)
{
    return serialization::binary::store(_pd, _val);
}
/*static*/ char* SerializerBase::storeValue(char* _pd, const uint32_t _val)
{
    return serialization::binary::store(_pd, _val);
}
/*static*/ char* SerializerBase::storeValue(char* _pd, const uint64_t _val)
{
    return serialization::binary::store(_pd, _val);
}
SerializerBase::~SerializerBase()
{
}
void SerializerBase::clear()
{
    run(nullptr, 0, nullptr);
}
void SerializerBase::doPushStringLimit()
{
    fstk.push(FncData(&Base::setStringLimit, 0, 0, rdefaultlmts.stringlimit));
}
void SerializerBase::doPushStringLimit(size_t _v)
{
    fstk.push(FncData(&Base::setStringLimit, 0, 0, _v));
}
void SerializerBase::doPushStreamLimit()
{
    fstk.push(FncData(&Base::setStreamLimit, 0, 0, rdefaultlmts.streamlimit));
}
void SerializerBase::doPushStreamLimit(uint64_t _v)
{
    fstk.push(FncData(&Base::setStreamLimit, 0, 0, _v));
}
void SerializerBase::doPushContainerLimit()
{
    fstk.push(FncData(&Base::setContainerLimit, 0, 0, rdefaultlmts.containerlimit));
}
void SerializerBase::doPushContainerLimit(size_t _v)
{
    fstk.push(FncData(&Base::setContainerLimit, 0, 0, _v));
}

int SerializerBase::run(char* _pb, size_t _bl, void* _pctx)
{
    cpb = pb = _pb;
    be       = cpb + _bl;
    while (fstk.size()) {
        solid_collect(statistics_.onLoop);
        FncData& rfd = fstk.top();
        switch ((*reinterpret_cast<FncT>(rfd.f))(*this, rfd, _pctx)) {
        case ContinueE:
            continue;
        case SuccessE:
            fstk.pop();
            continue;
        case WaitE:
            return static_cast<int>(cpb - pb);
        case FailureE:
            resetLimits();
            return -1;
        default:
            return -1;
        }
    }

    resetLimits();

    solid_assert(fstk.size() || (fstk.empty() && estk.empty()));
    return static_cast<int>(cpb - pb);
}

/*static*/ ReturnValues SerializerBase::storeBitvec(Base& _rs, FncData& _rfd, void* /*_pctx*/)
{
    SerializerBase& rs(static_cast<SerializerBase&>(_rs));

    if (!rs.cpb)
        return SuccessE;

    const std::vector<bool>* pbs = reinterpret_cast<std::vector<bool>*>(_rfd.p);
    const char*              n   = _rfd.n;

    if (pbs) {
        if (pbs->size() > rs.lmts.containerlimit) {
            rs.err = make_error(ERR_CONTAINER_LIMIT);
            return FailureE;
        }

        uint64_t crcsz;

        if (!compute_value_with_crc(crcsz, pbs->size())) {
            rs.err = make_error(ERR_CONTAINER_MAX_LIMIT);
            return FailureE;
        }
        _rfd.f = &SerializerBase::storeBitvecContinue;
        _rfd.s = 0;
        solid_dbg(logger, Info, " sz = " << crcsz);
        rs.fstk.push(FncData(&SerializerBase::template storeCross<uint64_t>, nullptr, n, 0, crcsz));
    } else {
        solid_dbg(logger, Info, " sz = " << rs.estk.top().first_uint64_t_value());
        _rfd.f = &SerializerBase::template storeCross<uint64_t>;
        _rfd.d = -1;
    }
    return ContinueE;
}

/*static*/ ReturnValues SerializerBase::storeBitvecContinue(Base& _rs, FncData& _rfd, void* /*_pctx*/)
{
    SerializerBase& rs(static_cast<SerializerBase&>(_rs));

    if (!rs.cpb)
        return SuccessE;

    const std::vector<bool>* pbs    = reinterpret_cast<std::vector<bool>*>(_rfd.p);
    size_t                   bitoff = 0;

    while ((rs.be - rs.cpb) && _rfd.s < pbs->size()) {
        uint8_t* puc = reinterpret_cast<uint8_t*>(rs.cpb);

        if (bitoff == 0) {
            *puc = 0;
        }

        *puc |= (((*pbs)[_rfd.s]) ? (1 << bitoff) : 0);
        ++_rfd.s;
        ++bitoff;
        if (bitoff == 8) {
            ++rs.cpb;
            bitoff = 0;
        }
    }

    if (_rfd.s < pbs->size()) {
        return WaitE;
    } else if (bitoff) {
        ++rs.cpb;
    }

    return SuccessE;
}

template <>
ReturnValues SerializerBase::storeBinary<0>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    SerializerBase& rs(static_cast<SerializerBase&>(_rb));
    if (!rs.cpb)
        return SuccessE;
    size_t len = rs.be - rs.cpb;
    if (len > _rfd.s)
        len = static_cast<size_t>(_rfd.s);
    solid_dbg(logger, Info, _rfd.s << ' ' << len << ' ' << trim_str((const char*)_rfd.p, len, 4, 4));
    memcpy(rs.cpb, _rfd.p, len);
    rs.cpb += len;
    _rfd.p = (char*)_rfd.p + len;
    _rfd.s -= len;
    if (_rfd.s)
        return WaitE;
    return SuccessE;
}

template <>
ReturnValues SerializerBase::storeBinary<1>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "");
    SerializerBase& rs(static_cast<SerializerBase&>(_rb));

    if (!rs.cpb)
        return SuccessE;
    const size_t len = rs.be - rs.cpb;

    if (len) {
        *rs.cpb = *reinterpret_cast<const char*>(_rfd.p);
        ++rs.cpb;
        return SuccessE;
    }
    return WaitE;
}

template <>
ReturnValues SerializerBase::storeBinary<2>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "");
    SerializerBase& rs(static_cast<SerializerBase&>(_rb));

    if (!rs.cpb)
        return SuccessE;
    const size_t len = rs.be - rs.cpb;
    const char*  ps  = reinterpret_cast<const char*>(_rfd.p);

    if (len >= 2) {
        *(rs.cpb + 0) = *(ps + 0);
        *(rs.cpb + 1) = *(ps + 1);
        rs.cpb += 2;
        return SuccessE;
    } else if (len >= 1) {
        *(rs.cpb + 0) = *(ps + 0);
        _rfd.f        = &SerializerBase::storeBinary<1>;
        _rfd.p        = const_cast<char*>(ps + 1);
        rs.cpb += 1;
    }
    return WaitE;
}

template <>
ReturnValues SerializerBase::storeBinary<4>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "");
    SerializerBase& rs(static_cast<SerializerBase&>(_rb));
    if (!rs.cpb)
        return SuccessE;
    const size_t len = rs.be - rs.cpb;
    const char*  ps  = reinterpret_cast<const char*>(_rfd.p);
    if (len >= 4) {
        *(rs.cpb + 0) = *(ps + 0);
        *(rs.cpb + 1) = *(ps + 1);
        *(rs.cpb + 2) = *(ps + 2);
        *(rs.cpb + 3) = *(ps + 3);
        rs.cpb += 4;
        return SuccessE;
    } else if (len >= 3) {
        *(rs.cpb + 0) = *(ps + 0);
        *(rs.cpb + 1) = *(ps + 1);
        *(rs.cpb + 2) = *(ps + 2);
        _rfd.p        = const_cast<char*>(ps + 3);
        _rfd.f        = &SerializerBase::storeBinary<1>;
        rs.cpb += 3;
        return WaitE;
    } else if (len >= 2) {
        *(rs.cpb + 0) = *(ps + 0);
        *(rs.cpb + 1) = *(ps + 1);
        _rfd.p        = const_cast<char*>(ps + 2);
        rs.cpb += 2;
        _rfd.f = &SerializerBase::storeBinary<2>;
        return WaitE;
    } else if (len >= 1) {
        *(rs.cpb + 0) = *(ps + 0);
        _rfd.p        = const_cast<char*>(ps + 1);
        rs.cpb += 1;
        _rfd.s = 3;
    } else {
        _rfd.s = 4;
    }
    _rfd.f = &SerializerBase::storeBinary<0>;
    return WaitE;
}

template <>
ReturnValues SerializerBase::storeBinary<8>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "");
    SerializerBase& rs(static_cast<SerializerBase&>(_rb));

    if (!rs.cpb)
        return SuccessE;

    const size_t len = rs.be - rs.cpb;
    const char*  ps  = reinterpret_cast<const char*>(_rfd.p);
    if (len >= 8) {
        *(rs.cpb + 0) = *(ps + 0);
        *(rs.cpb + 1) = *(ps + 1);
        *(rs.cpb + 2) = *(ps + 2);
        *(rs.cpb + 3) = *(ps + 3);
        *(rs.cpb + 4) = *(ps + 4);
        *(rs.cpb + 5) = *(ps + 5);
        *(rs.cpb + 6) = *(ps + 6);
        *(rs.cpb + 7) = *(ps + 7);
        rs.cpb += 8;
        return SuccessE;
    } else if (len >= 7) {
        *(rs.cpb + 0) = *(ps + 0);
        *(rs.cpb + 1) = *(ps + 1);
        *(rs.cpb + 2) = *(ps + 2);
        *(rs.cpb + 3) = *(ps + 3);
        *(rs.cpb + 4) = *(ps + 4);
        *(rs.cpb + 5) = *(ps + 5);
        *(rs.cpb + 6) = *(ps + 6);
        _rfd.p        = const_cast<char*>(ps + 7);
        _rfd.f        = &SerializerBase::storeBinary<1>;
        rs.cpb += 7;
        return WaitE;
    } else if (len >= 6) {
        *(rs.cpb + 0) = *(ps + 0);
        *(rs.cpb + 1) = *(ps + 1);
        *(rs.cpb + 2) = *(ps + 2);
        *(rs.cpb + 3) = *(ps + 3);
        *(rs.cpb + 4) = *(ps + 4);
        *(rs.cpb + 5) = *(ps + 5);
        _rfd.p        = const_cast<char*>(ps + 6);
        _rfd.f        = &SerializerBase::storeBinary<2>;
        rs.cpb += 6;
        return WaitE;
    } else if (len >= 5) {
        *(rs.cpb + 0) = *(ps + 0);
        *(rs.cpb + 1) = *(ps + 1);
        *(rs.cpb + 2) = *(ps + 2);
        *(rs.cpb + 3) = *(ps + 3);
        *(rs.cpb + 4) = *(ps + 4);
        _rfd.p        = const_cast<char*>(ps + 5);
        rs.cpb += 5;
        _rfd.s = 3;
    } else if (len >= 4) {
        *(rs.cpb + 0) = *(ps + 0);
        *(rs.cpb + 1) = *(ps + 1);
        *(rs.cpb + 2) = *(ps + 2);
        *(rs.cpb + 3) = *(ps + 3);
        _rfd.p        = const_cast<char*>(ps + 4);
        _rfd.f        = &SerializerBase::storeBinary<4>;
        rs.cpb += 4;
        return WaitE;
    } else if (len >= 3) {
        *(rs.cpb + 0) = *(ps + 0);
        *(rs.cpb + 1) = *(ps + 1);
        *(rs.cpb + 2) = *(ps + 2);
        _rfd.p        = const_cast<char*>(ps + 3);
        rs.cpb += 3;
        _rfd.s = 5;
    } else if (len >= 2) {
        *(rs.cpb + 0) = *(ps + 0);
        *(rs.cpb + 1) = *(ps + 1);
        _rfd.p        = const_cast<char*>(ps + 2);
        rs.cpb += 2;
        _rfd.s = 6;
    } else if (len >= 1) {
        *(rs.cpb + 0) = *(ps + 0);
        _rfd.p        = const_cast<char*>(ps + 1);
        rs.cpb += 1;
        _rfd.s = 7;
    } else {
        _rfd.s = 8;
    }
    _rfd.f = &SerializerBase::storeBinary<0>;
    return WaitE;
}

template <>
ReturnValues SerializerBase::store<bool>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "" << _rfd.n);
    SerializerBase& rs(static_cast<SerializerBase&>(_rb));

    if (!rs.cpb)
        return SuccessE;
    const size_t len = rs.be - rs.cpb;

    if (len) {
        *rs.cpb = (*static_cast<const bool*>(_rfd.p) ? 1 : 0);
        ++rs.cpb;
        return SuccessE;
    }
    return WaitE;
}

template <>
ReturnValues SerializerBase::store<int8_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "" << _rfd.n);
    _rfd.s = sizeof(int8_t);
    _rfd.f = &SerializerBase::storeBinary<1>;
    return storeBinary<1>(_rb, _rfd, nullptr);
}
template <>
ReturnValues SerializerBase::store<uint8_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "" << _rfd.n);
    _rfd.s = sizeof(uint8_t);
    _rfd.f = &SerializerBase::storeBinary<1>;
    _rfd.p = &_rfd.d;
    return storeBinary<1>(_rb, _rfd, nullptr);
}
template <>
ReturnValues SerializerBase::store<int16_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "" << _rfd.n);
    _rfd.s = sizeof(int16_t);
    _rfd.f = &SerializerBase::storeBinary<2>;
    return storeBinary<2>(_rb, _rfd, nullptr);
}
template <>
ReturnValues SerializerBase::store<uint16_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "" << _rfd.n);
    _rfd.s = sizeof(uint16_t);
    _rfd.f = &SerializerBase::storeBinary<2>;
    return storeBinary<2>(_rb, _rfd, nullptr);
}
template <>
ReturnValues SerializerBase::store<int32_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "" << _rfd.n);
    _rfd.s = sizeof(int32_t);
    _rfd.f = &SerializerBase::storeBinary<4>;
    return storeBinary<4>(_rb, _rfd, nullptr);
}
template <>
ReturnValues SerializerBase::store<uint32_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "" << _rfd.n);
    _rfd.s = sizeof(uint32_t);
    _rfd.f = &SerializerBase::storeBinary<4>;
    return storeBinary<4>(_rb, _rfd, nullptr);
}
template <>
ReturnValues SerializerBase::store<int64_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "");
    _rfd.s = sizeof(int64_t);
    _rfd.f = &SerializerBase::storeBinary<8>;
    return storeBinary<8>(_rb, _rfd, nullptr);
}
template <>
ReturnValues SerializerBase::store<uint64_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "" << _rfd.n << *((uint64_t*)_rfd.p));
    _rfd.s = sizeof(uint64_t);
    _rfd.f = &SerializerBase::storeBinary<8>;
    return storeBinary<8>(_rb, _rfd, nullptr);
}
/*template <>
ReturnValues SerializerBase::store<ulong>(Base &_rb, FncData &_rfd){
    solid_dbg(logger, Info, "");
    _rfd.s = sizeof(ulong);
    _rfd.f = &SerializerBase::storeBinary;
    return ContinueE;
}*/
template <>
ReturnValues SerializerBase::store<std::string>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    SerializerBase& rs(static_cast<SerializerBase&>(_rb));
    solid_dbg(logger, Info, "");
    if (!rs.cpb)
        return SuccessE;
    std::string* c = reinterpret_cast<std::string*>(_rfd.p);
    if (c->size() > rs.lmts.stringlimit) {
        rs.err = make_error(ERR_STRING_LIMIT);
        return FailureE;
    }

    uint64_t crcsz;

    if (!compute_value_with_crc(crcsz, c->size())) {
        rs.err = make_error(ERR_STRING_MAX_LIMIT);
        return FailureE;
    }
    rs.replace(FncData(&SerializerBase::storeBinary<0>, (void*)c->data(), _rfd.n, c->size()));
    rs.fstk.push(FncData(&SerializerBase::storeCross<uint64_t>, nullptr, _rfd.n, 0, crcsz));
    return ContinueE;
}

ReturnValues SerializerBase::storeStreamBegin(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    SerializerBase& rs(static_cast<SerializerBase&>(_rb));

    if (!rs.cpb)
        return SuccessE;

    size_t toread = rs.be - rs.cpb;

    if (toread < MIN_STREAM_BUFFER_SIZE)
        return WaitE;

    rs.streamerr.clear();
    rs.streamsz = 0;
    if (_rfd.p == nullptr) {
        rs.cpb = storeValue(rs.cpb, (uint16_t)0xffff);
        rs.pop(); //returning ok will also pop storeStream
        return SuccessE;
    }
    if (_rfd.s != -1ULL) {
        std::istream& ris = *reinterpret_cast<std::istream*>(_rfd.p);
        ris.seekg(_rfd.s);
        if (
            static_cast<int64_t>(_rfd.s) != ris.tellg()) {
            rs.streamerr = make_error(ERR_STREAM_SEEK);
            rs.cpb       = storeValue(rs.cpb, (uint16_t)0xffff);
            rs.pop(); //returning ok will also pop storeStream
        }
    }
    return SuccessE;
}
ReturnValues SerializerBase::storeStreamCheck(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    SerializerBase& rs(static_cast<SerializerBase&>(_rb));
    if (!rs.cpb)
        return SuccessE;
    if (_rfd.s > rs.lmts.streamlimit) {
        rs.streamerr = rs.err = make_error(ERR_STREAM_LIMIT);
        return FailureE;
    }
    return SuccessE;
}
ReturnValues SerializerBase::storeStream(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    SerializerBase& rs(static_cast<SerializerBase&>(_rb));

    solid_dbg(logger, Info, "");
    if (!rs.cpb)
        return SuccessE;

    uint64_t toread = rs.be - rs.cpb;

    if (toread < MIN_STREAM_BUFFER_SIZE)
        return WaitE;

    toread -= 2; //the buffsize

    if (toread > _rfd.s) {
        toread = _rfd.s;
    }

    if (toread > max_value_without_crc_16()) {
        toread = max_value_without_crc_16();
    }

    if (toread == 0) {
        rs.cpb = storeValue(rs.cpb, (uint16_t)0);
        return SuccessE;
    }

    std::istream& ris = *reinterpret_cast<std::istream*>(_rfd.p);

    ptrdiff_t rv;

    if (ris) {
        ris.read(rs.cpb + 2, toread);
        rv = ris.gcount();
    } else {
        rv = 0;
    }

    solid_dbg(logger, Info, "toread = " << toread << " rv = " << rv);

    if (rv > 0) {

        if ((rs.streamsz + rv) > rs.lmts.streamlimit) {
            rs.streamerr = rs.err = make_error(ERR_STREAM_LIMIT);
            solid_dbg(logger, Info, "ERR_STREAM_LIMIT");
            return FailureE;
        }

        toread = rv;

        uint16_t crcsz = 0;

        compute_value_with_crc(crcsz, toread);

        storeValue(rs.cpb, crcsz);
        solid_dbg(logger, Info, "store crcsz = " << crcsz << " sz = " << toread);

        solid_dbg(logger, Info, "store value " << crcsz);

        rs.cpb += toread + 2;
        rs.streamsz += toread;
    } else if (rv == 0) {
        solid_dbg(logger, Info, "done storing stream");
        rs.cpb = storeValue(rs.cpb, (uint16_t)0);
        return SuccessE;
    } else {
        rs.streamerr = make_error(ERR_STREAM_READ);
        solid_dbg(logger, Info, "ERR_STREAM_READ");
        rs.cpb = storeValue(rs.cpb, (uint16_t)0xffff);
        return SuccessE;
    }

    if (_rfd.s != -1ULL) {
        _rfd.s -= toread;
        if (_rfd.s == 0) {
            return ContinueE;
        }
    }
    solid_dbg(logger, Info, "streamsz = " << rs.streamsz);
    return ContinueE;
}

/*static*/ ReturnValues SerializerBase::storeUtf8(Base& _rs, FncData& _rfd, void* /*_pctx*/)
{
    SerializerBase& rs(static_cast<SerializerBase&>(_rs));
    if ((_rfd.s - 1) > rs.lmts.stringlimit) {
        rs.err = make_error(ERR_UTF8_LIMIT);
        return FailureE;
    }
    if ((_rfd.s - 1) > max_value_without_crc_32()) {
        rs.err = make_error(ERR_UTF8_MAX_LIMIT);
        return FailureE;
    }
    _rfd.f = &SerializerBase::storeBinary<0>;
    return ContinueE;
}

/*static*/ ReturnValues SerializerBase::storeCrossContinue(Base& _rs, FncData& _rfd, void* /*_pctx*/)
{
    SerializerBase& rs(static_cast<SerializerBase&>(_rs));
    if (!rs.cpb)
        return SuccessE;

    const size_t len     = rs.be - rs.cpb;
    size_t       towrite = rs.tmpstr.size() - _rfd.s;

    if (towrite > len) {
        towrite = len;
    }

    memcpy(rs.cpb, rs.tmpstr.data() + _rfd.s, towrite);

    rs.cpb += towrite;

    _rfd.s += towrite;

    if (_rfd.s == rs.tmpstr.size()) {
        rs.tmpstr.clear();
        return SuccessE;
    }
    return WaitE;
}

template <>
/*static*/ ReturnValues SerializerBase::storeCross<uint8_t>(Base& _rs, FncData& _rfd, void* /*_pctx*/)
{
    SerializerBase& rs(static_cast<SerializerBase&>(_rs));

    if (!rs.cpb)
        return SuccessE;

    const uint8_t v   = static_cast<uint8_t>(_rfd.d);
    const size_t  len = rs.be - rs.cpb;
    const size_t  vsz = cross::size(v);

    if (len >= vsz) {
        rs.cpb = binary::cross::store(rs.cpb, len, v);
        return SuccessE;
    } else {
        rs.tmpstr.resize(vsz);
        binary::cross::store(const_cast<char*>(rs.tmpstr.data()), vsz, v);
        memcpy(rs.cpb, rs.tmpstr.data(), len);
        rs.cpb += len;
        _rfd.s = len;
        _rfd.f = storeCrossContinue;
    }
    return WaitE;
}
template <>
/*static*/ ReturnValues SerializerBase::storeCross<uint16_t>(Base& _rs, FncData& _rfd, void* /*_pctx*/)
{
    SerializerBase& rs(static_cast<SerializerBase&>(_rs));

    if (!rs.cpb)
        return SuccessE;

    const uint16_t v   = static_cast<uint16_t>(_rfd.d);
    const size_t   len = rs.be - rs.cpb;
    const size_t   vsz = cross::size(v);

    if (len >= vsz) {
        rs.cpb = binary::cross::store(rs.cpb, len, v);
        return SuccessE;
    } else {
        rs.tmpstr.resize(vsz);
        binary::cross::store(const_cast<char*>(rs.tmpstr.data()), vsz, v);
        memcpy(rs.cpb, rs.tmpstr.data(), len);
        rs.cpb += len;
        _rfd.s = len;
        _rfd.f = storeCrossContinue;
    }
    return WaitE;
}
template <>
/*static*/ ReturnValues SerializerBase::storeCross<uint32_t>(Base& _rs, FncData& _rfd, void* /*_pctx*/)
{
    SerializerBase& rs(static_cast<SerializerBase&>(_rs));

    if (!rs.cpb)
        return SuccessE;

    const uint32_t v   = static_cast<uint32_t>(_rfd.d);
    const size_t   len = rs.be - rs.cpb;
    const size_t   vsz = cross::size(v);

    if (len >= vsz) {
        rs.cpb = binary::cross::store(rs.cpb, len, v);
        return SuccessE;
    } else {
        rs.tmpstr.resize(vsz);
        binary::cross::store(const_cast<char*>(rs.tmpstr.data()), vsz, v);
        memcpy(rs.cpb, rs.tmpstr.data(), len);
        rs.cpb += len;
        _rfd.s = len;
        _rfd.f = storeCrossContinue;
    }
    return WaitE;
}
template <>
/*static*/ ReturnValues SerializerBase::storeCross<uint64_t>(Base& _rs, FncData& _rfd, void* /*_pctx*/)
{
    SerializerBase& rs(static_cast<SerializerBase&>(_rs));

    if (!rs.cpb)
        return SuccessE;

    const uint64_t v   = _rfd.d;
    const size_t   len = rs.be - rs.cpb;
    const size_t   vsz = cross::size(v);

    if (len >= vsz) {
        rs.cpb = binary::cross::store(rs.cpb, len, v);
        return SuccessE;
    } else {
        rs.tmpstr.resize(vsz);
        binary::cross::store(const_cast<char*>(rs.tmpstr.data()), vsz, v);
        memcpy(rs.cpb, rs.tmpstr.data(), len);
        rs.cpb += len;
        _rfd.s = len;
        _rfd.f = storeCrossContinue;
    }
    return WaitE;
}
//========================================================================
//      Deserializer
//========================================================================

/*static*/ const char* DeserializerBase::loadValue(const char* _ps, uint8_t& _val)
{
    return serialization::binary::load(_ps, _val);
}
/*static*/ const char* DeserializerBase::loadValue(const char* _ps, uint16_t& _val)
{
    return serialization::binary::load(_ps, _val);
}
/*static*/ const char* DeserializerBase::loadValue(const char* _ps, uint32_t& _val)
{
    return serialization::binary::load(_ps, _val);
}
/*static*/ const char* DeserializerBase::loadValue(const char* _ps, uint64_t& _val)
{
    return serialization::binary::load(_ps, _val);
}
DeserializerBase::~DeserializerBase()
{
}
void DeserializerBase::clear()
{
    solid_dbg(logger, Info, "clear deserializer");
    run(nullptr, 0, nullptr);
}

void DeserializerBase::doPushStringLimit()
{
    fstk.push(FncData(&Base::setStringLimit, 0, 0, rdefaultlmts.stringlimit));
}
void DeserializerBase::doPushStringLimit(size_t _v)
{
    fstk.push(FncData(&Base::setStringLimit, 0, 0, _v));
}
void DeserializerBase::doPushStreamLimit()
{
    fstk.push(FncData(&Base::setStreamLimit, 0, 0, rdefaultlmts.streamlimit));
}
void DeserializerBase::doPushStreamLimit(uint64_t _v)
{
    fstk.push(FncData(&Base::setStreamLimit, 0, 0, _v));
}
void DeserializerBase::doPushContainerLimit()
{
    fstk.push(FncData(&Base::setContainerLimit, 0, 0, rdefaultlmts.containerlimit));
}
void DeserializerBase::doPushContainerLimit(size_t _v)
{
    fstk.push(FncData(&Base::setContainerLimit, 0, 0, _v));
}

int DeserializerBase::run(const char* _pb, size_t _bl, void* _pctx)
{

    cpb = pb = _pb;
    be       = pb + _bl;
    while (fstk.size()) {
        solid_collect(statistics_.onLoop);
        FncData& rfd = fstk.top();
        switch ((*reinterpret_cast<FncT>(rfd.f))(*this, rfd, _pctx)) {
        case ContinueE:
            continue;
        case SuccessE:
            fstk.pop();
            continue;
        case WaitE:
            return static_cast<int>(cpb - pb);
        case FailureE:
            resetLimits();
            return -1;
        default:
            return -1;
        }
    }

    resetLimits();

    solid_assert(fstk.size() || (fstk.empty() && estk.empty()));

    return static_cast<int>(cpb - pb);
}

/*static*/ ReturnValues DeserializerBase::loadBitvec(Base& _rd, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rd));
    if (!rd.cpb)
        return SuccessE;
    _rfd.f = &DeserializerBase::loadBitvecBegin;
    rd.pushExtended((uint64_t)0);
    rd.fstk.push(FncData(&DeserializerBase::loadCross<uint64_t>, &rd.estk.top().first_uint64_t_value()));
    return ContinueE;
}

/*static*/ ReturnValues DeserializerBase::loadBitvecBegin(Base& _rd, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rd));
    if (!rd.cpb) {
        rd.estk.pop();
        return SuccessE;
    }

    uint64_t len = rd.estk.top().first_uint64_t_value();

    if (len != InvalidSize()) {
        uint64_t crcsz;
        if (check_value_with_crc(crcsz, len)) {
            rd.estk.top().first_uint64_t_value() = crcsz;
            len                                  = crcsz;
        } else {
            rd.err = make_error(ERR_STRING_MAX_LIMIT);
            return FailureE;
        }
    }
    if (len >= rd.lmts.containerlimit) {
        solid_dbg(logger, Info, "error");
        rd.err = make_error(ERR_CONTAINER_LIMIT);
        return FailureE;
    }

    std::vector<bool>* pbs = reinterpret_cast<std::vector<bool>*>(_rfd.p);

    pbs->resize(len, false);

    _rfd.f = &DeserializerBase::loadBitvecContinue;
    _rfd.s = 0;
    return ContinueE;
}

/*static*/ ReturnValues DeserializerBase::loadBitvecContinue(Base& _rd, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rd));

    if (!rd.cpb) {
        rd.estk.pop();
        return SuccessE;
    }

    std::vector<bool>* pbs    = reinterpret_cast<std::vector<bool>*>(_rfd.p);
    uint64_t&          len    = rd.estk.top().first_uint64_t_value();
    size_t             bitoff = 0;

    while ((rd.be - rd.cpb) > 0 && _rfd.s < len) {
        const uint8_t* puc = reinterpret_cast<const uint8_t*>(rd.cpb);

        (*pbs)[_rfd.s] = ((*puc & (1 << bitoff)) != 0);

        ++_rfd.s;
        ++bitoff;

        if (bitoff == 8) {
            ++rd.cpb;
            bitoff = 0;
        }
    }

    if (_rfd.s < len) {
        return WaitE;
    } else if (bitoff) {
        ++rd.cpb;
    }

    rd.estk.pop();
    return SuccessE;
}

template <>
ReturnValues DeserializerBase::loadBinary<0>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));
    solid_dbg(logger, Info, "" << _rfd.s);
    if (!rd.cpb)
        return SuccessE;
    size_t len = rd.be - rd.cpb;
    if (len > _rfd.s)
        len = static_cast<size_t>(_rfd.s);
    memcpy(_rfd.p, rd.cpb, len);
    rd.cpb += len;
    _rfd.p = (char*)_rfd.p + len;
    _rfd.s -= len;
    if (len == 1) {
        solid_dbg(logger, Error, "");
    }

    solid_dbg(logger, Info, "" << len);
    if (_rfd.s)
        return WaitE;
    return SuccessE;
}

template <>
ReturnValues DeserializerBase::loadBinary<1>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));
    solid_dbg(logger, Info, "");
    if (!rd.cpb)
        return SuccessE;
    const size_t len = rd.be - rd.cpb;
    char*        ps  = reinterpret_cast<char*>(_rfd.p);
    solid_dbg(logger, Info, "" << len << ' ' << (void*)rd.cpb);
    if (len >= 1) {
        *(ps + 0) = *(rd.cpb + 0);
        rd.cpb += 1;
        return SuccessE;
    }
    return WaitE;
}

template <>
ReturnValues DeserializerBase::loadBinary<2>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));
    solid_dbg(logger, Info, "");
    if (!rd.cpb)
        return SuccessE;
    const size_t len = rd.be - rd.cpb;
    char*        ps  = reinterpret_cast<char*>(_rfd.p);
    if (len >= 2) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        rd.cpb += 2;
        return SuccessE;
    } else if (len >= 1) {
        *(ps + 0) = *(rd.cpb + 0);
        rd.cpb += 1;
        _rfd.p = ps + 1;
        _rfd.f = &DeserializerBase::loadBinary<1>;
    }
    return WaitE;
}

template <>
ReturnValues DeserializerBase::loadBinary<3>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));
    solid_dbg(logger, Info, "");
    if (!rd.cpb)
        return SuccessE;
    const size_t len = rd.be - rd.cpb;
    char*        ps  = reinterpret_cast<char*>(_rfd.p);
    if (len >= 3) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        rd.cpb += 3;
        return SuccessE;
    } else if (len >= 2) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        rd.cpb += 2;
        _rfd.p = ps + 2;
        _rfd.f = &DeserializerBase::loadBinary<1>;
    } else if (len >= 1) {
        *(ps + 0) = *(rd.cpb + 0);
        rd.cpb += 1;
        _rfd.p = ps + 1;
        _rfd.f = &DeserializerBase::loadBinary<2>;
    }
    return WaitE;
}

template <>
ReturnValues DeserializerBase::loadBinary<4>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));
    solid_dbg(logger, Info, "");
    if (!rd.cpb)
        return SuccessE;
    const size_t len = rd.be - rd.cpb;
    char*        ps  = reinterpret_cast<char*>(_rfd.p);
    if (len >= 4) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        *(ps + 3) = *(rd.cpb + 3);
        rd.cpb += 4;
        return SuccessE;
    } else if (len >= 3) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        rd.cpb += 3;
        _rfd.p = ps + 3;
        solid_dbg(logger, Error, "" << len << ' ' << (void*)rd.cpb);
        _rfd.f = &DeserializerBase::loadBinary<1>;
    } else if (len >= 2) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        rd.cpb += 2;
        _rfd.p = ps + 2;
        _rfd.f = &DeserializerBase::loadBinary<2>;
    } else if (len >= 1) {
        *(ps + 0) = *(rd.cpb + 0);
        rd.cpb += 1;
        _rfd.p = ps + 1;
        _rfd.f = &DeserializerBase::loadBinary<3>;
    }
    return WaitE;
}

template <>
ReturnValues DeserializerBase::loadBinary<5>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));
    solid_dbg(logger, Info, "");
    if (!rd.cpb)
        return SuccessE;
    const size_t len = rd.be - rd.cpb;
    char*        ps  = reinterpret_cast<char*>(_rfd.p);
    if (len >= 5) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        *(ps + 3) = *(rd.cpb + 3);
        *(ps + 4) = *(rd.cpb + 4);
        rd.cpb += 5;
        return SuccessE;
    } else if (len >= 4) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        *(ps + 3) = *(rd.cpb + 3);
        rd.cpb += 4;
        _rfd.p = ps + 4;
        _rfd.f = &DeserializerBase::loadBinary<1>;
    } else if (len >= 3) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        rd.cpb += 3;
        _rfd.p = ps + 3;
        _rfd.f = &DeserializerBase::loadBinary<2>;
    } else if (len >= 2) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        rd.cpb += 2;
        _rfd.p = ps + 2;
        _rfd.f = &DeserializerBase::loadBinary<3>;
    } else if (len >= 1) {
        *(ps + 0) = *(rd.cpb + 0);
        rd.cpb += 1;
        _rfd.p = ps + 1;
        _rfd.f = &DeserializerBase::loadBinary<4>;
    }
    return WaitE;
}

template <>
ReturnValues DeserializerBase::loadBinary<6>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));
    solid_dbg(logger, Info, "");
    if (!rd.cpb)
        return SuccessE;
    const size_t len = rd.be - rd.cpb;
    char*        ps  = reinterpret_cast<char*>(_rfd.p);
    if (len >= 6) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        *(ps + 3) = *(rd.cpb + 3);
        *(ps + 4) = *(rd.cpb + 4);
        *(ps + 5) = *(rd.cpb + 5);
        rd.cpb += 6;
        return SuccessE;
    } else if (len >= 5) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        *(ps + 3) = *(rd.cpb + 3);
        *(ps + 4) = *(rd.cpb + 4);
        rd.cpb += 5;
        _rfd.p = ps + 5;
        _rfd.f = &DeserializerBase::loadBinary<1>;
    } else if (len >= 4) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        *(ps + 3) = *(rd.cpb + 3);
        rd.cpb += 4;
        _rfd.p = ps + 4;
        _rfd.f = &DeserializerBase::loadBinary<2>;
    } else if (len >= 3) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        rd.cpb += 3;
        _rfd.p = ps + 3;
        _rfd.f = &DeserializerBase::loadBinary<3>;
    } else if (len >= 2) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        rd.cpb += 2;
        _rfd.p = ps + 2;
        _rfd.f = &DeserializerBase::loadBinary<4>;
    } else if (len >= 1) {
        *(ps + 0) = *(rd.cpb + 0);
        rd.cpb += 1;
        _rfd.p = ps + 1;
        _rfd.f = &DeserializerBase::loadBinary<5>;
    }
    return WaitE;
}

template <>
ReturnValues DeserializerBase::loadBinary<7>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));
    solid_dbg(logger, Info, "");
    if (!rd.cpb)
        return SuccessE;
    const size_t len = rd.be - rd.cpb;
    char*        ps  = reinterpret_cast<char*>(_rfd.p);
    if (len >= 7) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        *(ps + 3) = *(rd.cpb + 3);
        *(ps + 4) = *(rd.cpb + 4);
        *(ps + 5) = *(rd.cpb + 5);
        *(ps + 6) = *(rd.cpb + 6);
        rd.cpb += 7;
        return SuccessE;
    } else if (len >= 6) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        *(ps + 3) = *(rd.cpb + 3);
        *(ps + 4) = *(rd.cpb + 4);
        *(ps + 5) = *(rd.cpb + 5);
        rd.cpb += 6;
        _rfd.p = ps + 6;
        _rfd.f = &DeserializerBase::loadBinary<1>;
    } else if (len >= 5) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        *(ps + 3) = *(rd.cpb + 3);
        *(ps + 4) = *(rd.cpb + 4);
        rd.cpb += 5;
        _rfd.p = ps + 5;
        _rfd.f = &DeserializerBase::loadBinary<2>;
    } else if (len >= 4) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        *(ps + 3) = *(rd.cpb + 3);
        rd.cpb += 4;
        _rfd.p = ps + 4;
        _rfd.f = &DeserializerBase::loadBinary<3>;
    } else if (len >= 3) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        rd.cpb += 3;
        _rfd.p = ps + 3;
        _rfd.f = &DeserializerBase::loadBinary<4>;
    } else if (len >= 2) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        rd.cpb += 2;
        _rfd.p = ps + 2;
        _rfd.f = &DeserializerBase::loadBinary<5>;
    } else if (len >= 1) {
        *(ps + 0) = *(rd.cpb + 0);
        rd.cpb += 1;
        _rfd.p = ps + 1;
        _rfd.f = &DeserializerBase::loadBinary<6>;
    }
    return WaitE;
}

template <>
ReturnValues DeserializerBase::loadBinary<8>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));
    solid_dbg(logger, Info, "");
    if (!rd.cpb)
        return SuccessE;
    const size_t len = rd.be - rd.cpb;
    char*        ps  = reinterpret_cast<char*>(_rfd.p);
    if (len >= 8) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        *(ps + 3) = *(rd.cpb + 3);
        *(ps + 4) = *(rd.cpb + 4);
        *(ps + 5) = *(rd.cpb + 5);
        *(ps + 6) = *(rd.cpb + 6);
        *(ps + 7) = *(rd.cpb + 7);
        rd.cpb += 8;
        return SuccessE;
    } else if (len >= 7) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        *(ps + 3) = *(rd.cpb + 3);
        *(ps + 4) = *(rd.cpb + 4);
        *(ps + 5) = *(rd.cpb + 5);
        *(ps + 6) = *(rd.cpb + 6);
        rd.cpb += 7;
        _rfd.p = ps + 7;
        _rfd.f = &DeserializerBase::loadBinary<1>;
    } else if (len >= 6) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        *(ps + 3) = *(rd.cpb + 3);
        *(ps + 4) = *(rd.cpb + 4);
        *(ps + 5) = *(rd.cpb + 5);
        rd.cpb += 6;
        _rfd.p = ps + 6;
        _rfd.f = &DeserializerBase::loadBinary<2>;
    } else if (len >= 5) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        *(ps + 3) = *(rd.cpb + 3);
        *(ps + 4) = *(rd.cpb + 4);
        rd.cpb += 5;
        _rfd.p = ps + 5;
        _rfd.f = &DeserializerBase::loadBinary<3>;
    } else if (len >= 4) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        *(ps + 3) = *(rd.cpb + 3);
        rd.cpb += 4;
        _rfd.p = ps + 4;
        _rfd.f = &DeserializerBase::loadBinary<4>;
    } else if (len >= 3) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        *(ps + 2) = *(rd.cpb + 2);
        rd.cpb += 3;
        _rfd.p = ps + 3;
        _rfd.f = &DeserializerBase::loadBinary<5>;
    } else if (len >= 2) {
        *(ps + 0) = *(rd.cpb + 0);
        *(ps + 1) = *(rd.cpb + 1);
        rd.cpb += 2;
        _rfd.p = ps + 2;
        _rfd.f = &DeserializerBase::loadBinary<6>;
    } else if (len >= 1) {
        *(ps + 0) = *(rd.cpb + 0);
        rd.cpb += 1;
        _rfd.p = ps + 1;
        _rfd.f = &DeserializerBase::loadBinary<7>;
    }
    return WaitE;
}

template <>
ReturnValues DeserializerBase::load<bool>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));
    solid_dbg(logger, Info, "");
    if (!rd.cpb)
        return SuccessE;
    const size_t len = rd.be - rd.cpb;
    bool*        pb  = reinterpret_cast<bool*>(_rfd.p);
    solid_dbg(logger, Info, "" << len << ' ' << (void*)rd.cpb);
    if (len >= 1) {
        *pb = ((*rd.cpb) == 1) ? true : false;
        rd.cpb += 1;
        return SuccessE;
    }
    return WaitE;
}

template <>
ReturnValues DeserializerBase::load<int8_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "");
    _rfd.s = sizeof(int8_t);
    _rfd.f = &DeserializerBase::loadBinary<1>;
    return loadBinary<1>(_rb, _rfd, nullptr);
}
template <>
ReturnValues DeserializerBase::load<uint8_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "");
    _rfd.s = sizeof(uint8_t);
    _rfd.f = &DeserializerBase::loadBinary<1>;
    return loadBinary<1>(_rb, _rfd, nullptr);
}
template <>
ReturnValues DeserializerBase::load<int16_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "");
    _rfd.s = sizeof(int16_t);
    _rfd.f = &DeserializerBase::loadBinary<2>;
    return loadBinary<2>(_rb, _rfd, nullptr);
}
template <>
ReturnValues DeserializerBase::load<uint16_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "");
    _rfd.s = sizeof(uint16_t);
    _rfd.f = &DeserializerBase::loadBinary<2>;
    return loadBinary<2>(_rb, _rfd, nullptr);
}
template <>
ReturnValues DeserializerBase::load<int32_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "");
    _rfd.s = sizeof(int32_t);
    _rfd.f = &DeserializerBase::loadBinary<4>;
    return loadBinary<4>(_rb, _rfd, nullptr);
}
template <>
ReturnValues DeserializerBase::load<uint32_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "");
    _rfd.s = sizeof(uint32_t);
    _rfd.f = &DeserializerBase::loadBinary<4>;
    return loadBinary<4>(_rb, _rfd, nullptr);
}

template <>
ReturnValues DeserializerBase::load<int64_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "");
    _rfd.s = sizeof(int64_t);
    _rfd.f = &DeserializerBase::loadBinary<8>;
    return loadBinary<8>(_rb, _rfd, nullptr);
}
template <>
ReturnValues DeserializerBase::load<uint64_t>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "");
    _rfd.s = sizeof(uint64_t);
    _rfd.f = &DeserializerBase::loadBinary<8>;
    return loadBinary<8>(_rb, _rfd, nullptr);
}
/*template <>
ReturnValues DeserializerBase::load<ulong>(Base &_rb, FncData &_rfd){
    solid_dbg(logger, Info, "");
    _rfd.s = sizeof(ulong);
    _rfd.f = &DeserializerBase::loadBinary;
    return ContinueE;
}*/
template <>
ReturnValues DeserializerBase::load<std::string>(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "load generic non pointer string");
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));
    if (!rd.cpb)
        return SuccessE;
    _rfd.f = &DeserializerBase::loadBinaryStringCheck;
    _rfd.d = 0;
    rd.fstk.push(FncData(&DeserializerBase::loadCross<uint64_t>, &_rfd.d, _rfd.n));
    return ContinueE;
}
ReturnValues DeserializerBase::loadBinaryStringCheck(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));

    if (!rd.cpb)
        return SuccessE;

    const uint64_t len = _rfd.d;

    if (len != InvalidSize()) {
        uint64_t crcsz;
        if (check_value_with_crc(crcsz, len)) {
            _rfd.d = crcsz;
        } else {
            rd.err = make_error(ERR_STRING_MAX_LIMIT);
            return FailureE;
        }
    }

    uint64_t ul = _rfd.d;

    if (ul < rd.lmts.stringlimit) {
        std::string* ps = reinterpret_cast<std::string*>(_rfd.p);

        ps->reserve(ul);

        _rfd.f = &DeserializerBase::loadBinaryString;

        return ContinueE;
    } else {
        solid_dbg(logger, Info, "error");
        rd.err = make_error(ERR_STRING_LIMIT);
        return FailureE;
    }
}

void dummy_string_check(std::string const& _rstr, const char* _pb, size_t _len) {}

StringCheckFncT pcheckfnc = &dummy_string_check;

ReturnValues DeserializerBase::loadBinaryString(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));

    if (!rd.cpb) {
        return SuccessE;
    }

    size_t   len = rd.be - rd.cpb;
    uint64_t ul  = _rfd.d;

    if (len > ul) {
        len = static_cast<size_t>(ul);
    }

    std::string* ps = reinterpret_cast<std::string*>(_rfd.p);

    pcheckfnc(*ps, rd.cpb, len);

    solid_dbg(logger, Info, (ps->capacity() - ps->size()) << ' ' << len << ' ' << trim_str(rd.cpb, len, 4, 4));

    ps->append(rd.cpb, len);

    rd.cpb += len;
    ul -= len;
    if (ul) {
        _rfd.d = ul;
        return WaitE;
    }
    solid_dbg(logger, Info, trim_str(*ps, 64, 64));
    return SuccessE;
}

ReturnValues DeserializerBase::loadStreamCheck(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));

    if (!rd.cpb)
        return SuccessE;

    if (_rfd.s > static_cast<uint64_t>(rd.lmts.streamlimit)) {
        solid_dbg(logger, Info, "error");
        rd.err = make_error(ERR_STREAM_LIMIT);
        return FailureE;
    }
    return SuccessE;
}
ReturnValues DeserializerBase::loadStreamBegin(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));

    if (!rd.cpb)
        return SuccessE;

    rd.streamerr.clear();
    rd.streamsz = 0;

    if (_rfd.p == nullptr) {
        rd.pop();
        rd.fstk.top().f = &DeserializerBase::loadDummyStream;
        rd.fstk.top().s = 0;
        return ContinueE;
    }

    if (_rfd.s != InvalidSize()) {
        std::ostream& ros = *reinterpret_cast<std::ostream*>(_rfd.p);
        ros.seekp(_rfd.s);
        if (
            static_cast<int64_t>(_rfd.s) != ros.tellp()) {
            rd.streamerr = make_error(ERR_STREAM_SEEK);
            rd.pop();
            rd.fstk.top().f = &DeserializerBase::loadDummyStream;
            rd.fstk.top().s = 0;
            return ContinueE;
        }
    }
    return SuccessE;
}

ReturnValues DeserializerBase::loadStream(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "");

    DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));

    if (!rd.cpb)
        return SuccessE;

    uint64_t towrite = rd.be - rd.cpb;

    if (towrite < 2) {
        return WaitE;
    }
    towrite -= 2;

    if (towrite > _rfd.s) {
        towrite = _rfd.s;
    }

    uint16_t sz(0);

    rd.cpb = loadValue(rd.cpb, sz);
    solid_dbg(logger, Info, "sz = " << sz);

    if (sz == 0xffff) { //error on storing side - the stream is incomplete
        solid_dbg(logger, Info, "error on storing side");
        rd.streamerr = make_error(ERR_STREAM_SENDER);
        return SuccessE;
    } else {
        //CRCValue<uint16_t> crcsz(CRCValue<uint16_t>::check_and_create(sz));
        uint16_t crcsz;
        if (check_value_with_crc(crcsz, sz)) {
            sz = crcsz;
        } else {
            rd.streamerr = rd.err = make_error(ERR_STREAM_CHUNK_MAX_LIMIT);
            solid_dbg(logger, Info, "crcval = " << crcsz << " towrite = " << towrite);
            return FailureE;
        }
    }
    if (towrite > sz)
        towrite = sz;
    solid_dbg(logger, Info, "towrite = " << towrite);
    if (towrite == 0) {
        return SuccessE;
    }

    if ((rd.streamsz + towrite) > rd.lmts.streamlimit) {
        solid_dbg(logger, Info, "ERR_STREAM_LIMIT");
        rd.streamerr = rd.err = make_error(ERR_STREAM_LIMIT);
        return FailureE;
    }

    std::ostream& ros = *reinterpret_cast<std::ostream*>(_rfd.p);

    ros.write(rd.cpb, towrite);

    uint64_t rv;

    if (ros.fail() || ros.eof()) {
        rv = -1;
    } else {
        rv = towrite;
    }

    rd.cpb += sz;

    if (_rfd.s != InvalidSize()) {
        _rfd.s -= towrite;
        solid_dbg(logger, Info, "_rfd.s = " << _rfd.s);
        if (_rfd.s == 0) {
            _rfd.f = &loadDummyStream;
            _rfd.s = rd.streamsz + sz;
        }
    }

    if (rv != towrite) {
        rd.streamerr = make_error(ERR_STREAM_WRITE);
        _rfd.f       = &loadDummyStream;
        _rfd.s       = rd.streamsz + sz;
    } else {
        rd.streamsz += rv;
    }
    solid_dbg(logger, Info, "streamsz = " << rd.streamsz);
    return ContinueE;
}

ReturnValues DeserializerBase::loadDummyStream(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    solid_dbg(logger, Info, "");

    DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));

    if (!rd.cpb)
        return SuccessE;

    ptrdiff_t towrite = rd.be - rd.cpb;

    if (towrite < 2) {
        return WaitE;
    }

    towrite -= 2;

    if (towrite > static_cast<ptrdiff_t>(_rfd.s)) {
        towrite = static_cast<ptrdiff_t>(_rfd.s);
    }
    uint16_t sz(0);

    rd.cpb = loadValue(rd.cpb, sz);

    solid_dbg(logger, Info, "sz = " << sz);

    if (sz == 0xffff) { //error on storing side - the stream is incomplete
        rd.streamerr = make_error(ERR_STREAM_SENDER);
        return SuccessE;
    } else if (sz == 0) {
        return SuccessE;
    } else {
        //CRCValue<uint16_t> crcsz(CRCValue<uint16_t>::check_and_create(sz));
        uint16_t crcsz;
        if (check_value_with_crc(crcsz, sz)) {
            sz = crcsz;
        } else {
            rd.streamerr = rd.err = make_error(ERR_STREAM_CHUNK_MAX_LIMIT);
            solid_dbg(logger, Info, "crcval = " << crcsz << " towrite = " << towrite);
            return FailureE;
        }
    }
    rd.cpb += sz;
    _rfd.s += sz;
    if (_rfd.s > rd.lmts.streamlimit) {
        solid_dbg(logger, Info, "ERR_STREAM_LIMIT");
        rd.streamerr = rd.err = make_error(ERR_STREAM_LIMIT);
        return FailureE;
    }
    return ContinueE;
}
ReturnValues DeserializerBase::loadUtf8(Base& _rb, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rb));
    solid_dbg(logger, Info, "");
    std::string* ps     = reinterpret_cast<std::string*>(_rfd.p);
    size_t       len    = rd.be - rd.cpb;
    size_t       slen   = cstring::nlen(rd.cpb, len);
    size_t       totlen = ps->size() + slen;
    solid_dbg(logger, Info, "len = " << len);
    if (totlen > rd.lmts.stringlimit) {
        rd.err = make_error(ERR_UTF8_LIMIT);
        return FailureE;
    }
    if (totlen > max_value_without_crc_32()) {
        rd.err = make_error(ERR_UTF8_MAX_LIMIT);
        return FailureE;
    }
    ps->append(rd.cpb, slen);
    rd.cpb += slen;
    if (slen == len) {
        return WaitE;
    }
    ++rd.cpb;
    return SuccessE;
}

/*
 * _rfd.s contains the size of the value data
 * estk.top contains the uint64_t value
 * _rfd.p contains the current index
 */
/*static*/ ReturnValues DeserializerBase::loadCrossContinue(Base& _rd, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rd));
    if (!rd.cpb)
        return SuccessE;

    const size_t len    = rd.be - rd.cpb;
    size_t       toread = rd.tmpstr.size() - _rfd.s;

    if (toread > len) {
        toread = len;
    }

    memcpy(const_cast<char*>(rd.tmpstr.data()) + _rfd.s, rd.cpb, toread);

    rd.cpb += toread;
    _rfd.s += toread;

    if (_rfd.s < rd.tmpstr.size()) {
        return WaitE;
    }
    return SuccessE;
}
template <>
/*static*/ ReturnValues DeserializerBase::loadCross<uint8_t>(Base& _rd, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rd));

    solid_dbg(logger, Info, "");

    if (!rd.cpb)
        return SuccessE;

    const size_t len = rd.be - rd.cpb;
    if (!len)
        return WaitE;

    const size_t vsz = cross::size(rd.cpb);
    uint8_t&     v   = *reinterpret_cast<uint8_t*>(_rfd.p);

    if (vsz <= len) {
        const char* p = binary::cross::load(rd.cpb, len, v);
        if (p) {
            rd.cpb = p;
            return SuccessE;
        } else {
            rd.err = make_error(ERR_CROSS_VALUE_SMALL);
            return FailureE;
        }
    } else {
        rd.tmpstr.resize(vsz);
        rd.tmpstr[0] = *rd.cpb;
        ++rd.cpb;
        //memcpy(const_cast<char*>(tmpstr.data()), rd.cpb, len);

        _rfd.f = loadCrossContinue;
        _rfd.s = 1;
        rd.fstk.push(_rfd);

        _rfd.f = loadCrossDone<uint8_t>;
        return ContinueE;
    }
}

template <>
/*static*/ ReturnValues DeserializerBase::loadCross<uint16_t>(Base& _rd, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rd));

    solid_dbg(logger, Info, "");

    if (!rd.cpb)
        return SuccessE;

    const size_t len = rd.be - rd.cpb;
    if (!len)
        return WaitE;

    const size_t vsz = cross::size(rd.cpb);
    uint16_t&    v   = *reinterpret_cast<uint16_t*>(_rfd.p);

    if (vsz <= len) {
        const char* p = binary::cross::load(rd.cpb, len, v);
        if (p) {
            rd.cpb = p;
            return SuccessE;
        } else {
            rd.err = make_error(ERR_CROSS_VALUE_SMALL);
            return FailureE;
        }
    } else {
        rd.tmpstr.resize(vsz);
        memcpy(const_cast<char*>(rd.tmpstr.data()), rd.cpb, len);
        rd.cpb += len;

        _rfd.f = loadCrossContinue;
        _rfd.s = len;
        rd.fstk.push(_rfd);

        _rfd.f = loadCrossDone<uint16_t>;
        return ContinueE;
    }
}

template <>
/*static*/ ReturnValues DeserializerBase::loadCross<uint32_t>(Base& _rd, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rd));

    solid_dbg(logger, Info, "");

    if (!rd.cpb)
        return SuccessE;

    const size_t len = rd.be - rd.cpb;
    if (!len)
        return WaitE;

    const size_t vsz = cross::size(rd.cpb);
    uint32_t&    v   = *reinterpret_cast<uint32_t*>(_rfd.p);

    if (vsz <= len) {
        const char* p = binary::cross::load(rd.cpb, len, v);
        if (p) {
            rd.cpb = p;
            return SuccessE;
        } else {
            rd.err = make_error(ERR_CROSS_VALUE_SMALL);
            return FailureE;
        }
    } else {
        rd.tmpstr.resize(vsz);
        memcpy(const_cast<char*>(rd.tmpstr.data()), rd.cpb, len);
        rd.cpb += len;

        _rfd.f = loadCrossContinue;
        _rfd.s = len;
        rd.fstk.push(_rfd);

        _rfd.f = loadCrossDone<uint32_t>;
        return ContinueE;
    }
}

template <>
/*static*/ ReturnValues DeserializerBase::loadCross<uint64_t>(Base& _rd, FncData& _rfd, void* /*_pctx*/)
{
    DeserializerBase& rd(static_cast<DeserializerBase&>(_rd));

    solid_dbg(logger, Info, "");

    if (!rd.cpb)
        return SuccessE;

    const size_t len = rd.be - rd.cpb;
    if (!len)
        return WaitE;

    const size_t vsz = cross::size(rd.cpb);
    uint64_t&    v   = *reinterpret_cast<uint64_t*>(_rfd.p);

    if (vsz <= len) {
        const char* p = binary::cross::load(rd.cpb, len, v);
        if (p) {
            rd.cpb = p;
            return SuccessE;
        } else {
            rd.err = make_error(ERR_CROSS_VALUE_SMALL);
            return FailureE;
        }
    } else if (vsz <= (sizeof(uint64_t) + 1)) {
        rd.tmpstr.resize(vsz);
        memcpy(const_cast<char*>(rd.tmpstr.data()), rd.cpb, len);
        rd.cpb += len;

        _rfd.f = loadCrossContinue;
        _rfd.s = len;
        rd.fstk.push(_rfd);

        _rfd.f = loadCrossDone<uint64_t>;
        return ContinueE;
    } else {
        rd.err = make_error(ERR_DESERIALIZE_VALUE);
        return FailureE;
    }
}
//========================================================================
//========================================================================
} //namespace binary
} //namespace serialization
} //namespace solid
