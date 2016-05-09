// frame/ipc/ipcprotocol.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_IPCSERIALIZATION_HPP
#define SOLID_FRAME_IPC_IPCSERIALIZATION_HPP


#include "frame/ipc/ipcmessage.hpp"
#include "frame/ipc/ipcerror.hpp"
#include "system/function.hpp"


namespace solid{
namespace frame{
namespace ipc{

struct ConnectionContext;

template <class Req>
struct message_complete_traits;

template <class Req, class Res>
struct message_complete_traits<void(*)(ConnectionContext&, DynamicPointer<Req> &, DynamicPointer<Res>&, ErrorConditionT const&)>{
	typedef Req send_type;
	typedef Res recv_type;
};

template <class Req, class Res>
struct message_complete_traits<void(ConnectionContext&, DynamicPointer<Req> &, DynamicPointer<Res>&, ErrorConditionT const&)>{
	typedef Req send_type;
	typedef Res recv_type;
};

template <class C, class Req, class Res>
struct message_complete_traits<void(C::*)(ConnectionContext&, DynamicPointer<Req> &, DynamicPointer<Res>&, ErrorConditionT const&)>{
	typedef Req send_type;
	typedef Res recv_type;
};

template <class C, class Req, class Res>
struct message_complete_traits<void(C::*)(ConnectionContext&, DynamicPointer<Req> &, DynamicPointer<Res>&, ErrorConditionT const&) const>{
	typedef Req send_type;
	typedef Res recv_type;
};

template <class C, class R>
struct message_complete_traits<R(C::*)>: public message_complete_traits<R(C&)>{
};

template <class F>
struct message_complete_traits{
private:
	using call_type = message_complete_traits<decltype(&F::operator())>;
public:
	using send_type = typename call_type::send_type;
	using recv_type = typename call_type::recv_type;
};



template <class F, class Req, class Res>
struct CompleteHandler{
	F		f;
	
	CompleteHandler(F _f):f(_f){}
	
	void operator()(
		ConnectionContext &_rctx,
		MessagePointerT &_rreq_msg_ptr,
		MessagePointerT &_rres_msg_ptr,
		ErrorConditionT const &_err
	){
		Req					*prequest = dynamic_cast<Req*>(_rreq_msg_ptr.get());
		DynamicPointer<Req>	req_msg_ptr(prequest);
		
		Res					*presponse = dynamic_cast<Res*>(_rres_msg_ptr.get());
		DynamicPointer<Res>	res_msg_ptr(presponse);
		
		ErrorConditionT		error(_err);

		if(not error and req_msg_ptr.get() and not prequest){
			error = error_service_bad_cast_request;
		}
		
		if(not error and res_msg_ptr.get() and not presponse){
			error = error_service_bad_cast_response;
		}
		
		f(_rctx, req_msg_ptr, res_msg_ptr, error);
	}
};

using MessageCompleteFunctionT	= FUNCTION<void(
	ConnectionContext &, MessagePointerT &, MessagePointerT &, ErrorConditionT const &
)>;

struct TypeStub{
	MessageCompleteFunctionT	complete_fnc;
};

class Deserializer{
public:
	virtual ~Deserializer();
	virtual void push(MessagePointerT &_rmsgptr) = 0;
	virtual int run(ConnectionContext &, const char* _pdata, size_t _data_len) = 0;
	virtual ErrorConditionT error() const = 0;
	
	virtual bool empty() const = 0;
	virtual void clear() = 0;
};

using DeserializerPointerT	= std::unique_ptr<Deserializer>;

class Serializer{
public:
	virtual ~Serializer();
	virtual void push(MessagePointerT &_rmsgptr, const size_t _msg_type_idx) = 0;
	virtual int run(ConnectionContext &, char* _pdata, size_t _data_len) = 0;
	virtual ErrorConditionT error() const = 0;
	
	virtual bool empty() const = 0;
	virtual void clear() = 0;
};

using SerializerPointerT	= std::unique_ptr<Serializer>;

class Protocol{
public:
	enum{
		MaxPacketDataSize = 1024 * 64,
	};

	
	virtual ~Protocol();
	
	virtual char* storeValue(char *_pd, uint8  _v) const = 0;
	virtual char* storeValue(char *_pd, uint16 _v) const = 0;
	virtual char* storeValue(char *_pd, uint32 _v) const = 0;
	virtual char* storeValue(char *_pd, uint64 _v) const = 0;
	
	virtual const char* loadValue(const char *_ps, uint8  &_val) const = 0;
	virtual const char* loadValue(const char *_ps, uint16 &_val) const = 0;
	virtual const char* loadValue(const char *_ps, uint32 &_val) const = 0;
	virtual const char* loadValue(const char *_ps, uint64 &_val) const = 0;
	
	virtual size_t typeIndex(const Message *_pmsg) const = 0;
	
	virtual const TypeStub& operator[](const size_t _idx) const = 0;
	
	virtual SerializerPointerT   createSerializer() const = 0;
	virtual DeserializerPointerT createDeserializer() const = 0;
	
	virtual void reset(Serializer &) const = 0;
	virtual void reset(Deserializer &) const = 0;
	
	virtual size_t minimumFreePacketDataSize() const = 0;
};

using ProtocolPointerT = std::shared_ptr<Protocol>;

}//namespace ipc
}//namespace frame
}//namespace solid

#endif
