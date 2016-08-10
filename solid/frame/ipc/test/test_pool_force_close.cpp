#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aiolistener.hpp"
#include "solid/frame/aio/aiotimer.hpp"
#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/aio/openssl/aiosecurecontext.hpp"
#include "solid/frame/aio/openssl/aiosecuresocket.hpp"

#include "solid/frame/ipc/ipcservice.hpp"
#include "solid/frame/ipc/ipcconfiguration.hpp"
#include "solid/frame/ipc/ipcprotocol_serialization_v1.hpp"


#include <thread>
#include <mutex>
#include <condition_variable>
#include "solid/system/exception.hpp"

#include "solid/system/debug.hpp"

#include <iostream>

using namespace std;
using namespace solid;

typedef frame::Scheduler<frame::aio::Reactor>	AioSchedulerT;
typedef frame::aio::openssl::Context			SecureContextT;

namespace{

struct InitStub{
	size_t		size;
	ulong		flags;
};

InitStub initarray[] = {
	{8192000, 0},
	{8024000, 0},
	{8048000, 0},
	{8096000, 0},
	{8192000, 0},
	{16384000, 0},
	{8192000, 0}/*,
	{8024000, 0},
	{8048000, 0},
	{8096000, 0},
	{8192000, 0},
	{16384000, 0}*/
};

std::string						pattern;
const size_t					initarraysize = sizeof(initarray)/sizeof(InitStub);

std::atomic<size_t>				crtwriteidx(0);
std::atomic<size_t>				crtreadidx(0);
std::atomic<size_t>				crtbackidx(0);
std::atomic<size_t>				crtackidx(0);
std::atomic<size_t>				writecount(0);

size_t							connection_count(0);

bool							running = true;
std::mutex						mtx;
std::condition_variable			cnd;
frame::ipc::Service				*pipcclient = nullptr;
std::atomic<uint64_t>				transfered_size(0);
std::atomic<size_t>				transfered_count(0);
frame::ipc::RecipientId			recipinet_id;

size_t real_size(size_t _sz){
	//offset + (align - (offset mod align)) mod align
	return _sz + ((sizeof(uint64_t) - (_sz % sizeof(uint64_t))) % sizeof(uint64_t));
}

struct Message: frame::ipc::Message{
	uint32_t							idx;
    std::string						str;
	bool							serialized;
	
	Message(uint32_t _idx):idx(_idx), serialized(false){
		idbg("CREATE ---------------- "<<(void*)this<<" idx = "<<idx);
		init();
		
	}
	Message(): serialized(false){
		idbg("CREATE ---------------- "<<(void*)this);
	}
	~Message(){
		idbg("DELETE ---------------- "<<(void*)this);
// 		if(not serialized and not this->isBackOnSender()){
// 			SOLID_THROW("Message not serialized.");
// 		}
	}

	template <class S>
	void serialize(S &_s, frame::ipc::ConnectionContext &_rctx){
		_s.push(str, "str");
		_s.push(idx, "idx");
		
		if(S::IsSerializer){
			serialized = true;
		}
		if(isOnPeer()){
			++crtreadidx;
			idbg(crtreadidx);
			if(crtreadidx == 1){
				
				pipcclient->forceCloseConnectionPool(
					recipinet_id,
					[](frame::ipc::ConnectionContext &_rctx){
						idbg("------------------");
						unique_lock<mutex> lock(mtx);
						running = false;
						cnd.notify_one();
					}
				);
			
			}
		}
	}
	
	void init(){
		const size_t	sz = real_size(initarray[idx % initarraysize].size);
		str.resize(sz);
		const size_t	count = sz / sizeof(uint64_t);
		uint64_t			*pu  = reinterpret_cast<uint64_t*>(const_cast<char*>(str.data()));
		const uint64_t	*pup = reinterpret_cast<const uint64_t*>(pattern.data());
		const size_t	pattern_size = pattern.size() / sizeof(uint64_t);
		for(uint64_t i = 0; i < count; ++i){
			pu[i] = pup[(idx + i) % pattern_size];//pattern[i % pattern.size()];
		}
	}
	
	bool check()const{
		const size_t	sz = real_size(initarray[idx % initarraysize].size);
		idbg("str.size = "<<str.size()<<" should be equal to "<<sz);
		if(sz != str.size()){
			return false;
		}
		//return true;
		const size_t	count = sz / sizeof(uint64_t);
		const uint64_t	*pu = reinterpret_cast<const uint64_t*>(str.data());
		const uint64_t	*pup = reinterpret_cast<const uint64_t*>(pattern.data());
		const size_t	pattern_size = pattern.size() / sizeof(uint64_t);
		
		for(uint64_t i = 0; i < count; ++i){
			if(pu[i] != pup[(i + idx) % pattern_size]){
				SOLID_THROW("Message check failed.");
				return false;
			}
		}
		return true;
	}
	
};

void client_connection_stop(frame::ipc::ConnectionContext &_rctx){
	idbg(_rctx.recipientId()<<" error: "<<_rctx.error().message());
	if(_rctx.isConnectionActive()){
		++connection_count;
	}
}

void client_connection_start(frame::ipc::ConnectionContext &_rctx){
	idbg(_rctx.recipientId());
}

void server_connection_stop(frame::ipc::ConnectionContext &_rctx){
	idbg(_rctx.recipientId()<<" error: "<<_rctx.error().message());
}

void server_connection_start(frame::ipc::ConnectionContext &_rctx){
	idbg(_rctx.recipientId());
}


void client_complete_message(
	frame::ipc::ConnectionContext &_rctx,
	std::shared_ptr<Message> &_rsent_msg_ptr, std::shared_ptr<Message> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	idbg(_rctx.recipientId()<<" error: "<<_rerror.message());
	
	if(_rsent_msg_ptr.get()){
		SOLID_CHECK(_rerror);
		++crtackidx;
	}
	SOLID_CHECK(not _rrecv_msg_ptr);
}

void server_complete_message(
	frame::ipc::ConnectionContext &_rctx,
	std::shared_ptr<Message> &_rsent_msg_ptr, std::shared_ptr<Message> &_rrecv_msg_ptr,
	ErrorConditionT const &_rerror
){
	SOLID_CHECK(false);
}

}//namespace

int test_pool_force_close(int argc, char **argv){
#ifdef SOLID_HAS_DEBUG
	Debug::the().levelMask("ew");
	Debug::the().moduleMask("frame_ipc:ew any:ew");
	Debug::the().initStdErr(false, nullptr);
	//Debug::the().initFile("test_clientserver_basic", false);
#endif
	
	size_t max_per_pool_connection_count = 1;
	
	if(argc > 1){
		max_per_pool_connection_count = atoi(argv[1]);
		if(max_per_pool_connection_count == 0){
			max_per_pool_connection_count = 1;
		}
		if(max_per_pool_connection_count > 100){
			max_per_pool_connection_count = 100;
		}
	}
	
	for(int j = 0; j < 1; ++j){
		for(int i = 0; i < 127; ++i){
			int c = (i + j) % 127;
			if(isprint(c) and !isblank(c)){
				pattern += static_cast<char>(c);
			}
		}
	}
	
	size_t	sz = real_size(pattern.size());
	
	if(sz > pattern.size()){
		pattern.resize(sz - sizeof(uint64_t));
	}else if(sz < pattern.size()){
		pattern.resize(sz);
	}
	
	{
		AioSchedulerT			sch_client;
		AioSchedulerT			sch_server;
			
			
		frame::Manager			m;
		frame::ipc::ServiceT	ipcserver(m);
		frame::ipc::ServiceT	ipcclient(m);
		ErrorConditionT			err;
		
		frame::aio::Resolver	resolver;
		
		err = sch_client.start(1);
		
		if(err){
			edbg("starting aio client scheduler: "<<err.message());
			return 1;
		}
		
		err = sch_server.start(1);
		
		if(err){
			edbg("starting aio server scheduler: "<<err.message());
			return 1;
		}
		
		err = resolver.start(1);
		
		if(err){
			edbg("starting aio resolver: "<<err.message());
			return 1;
		}
		
		std::string		server_port;
		
		{//ipc server initialization
			frame::ipc::serialization_v1::Protocol	*proto = new frame::ipc::serialization_v1::Protocol;
			frame::ipc::Configuration				cfg(sch_server, proto);
			
			proto->registerType<Message>(
				server_complete_message
			);
			
			//cfg.recv_buffer_capacity = 1024;
			//cfg.send_buffer_capacity = 1024;
			
			cfg.connection_stop_fnc = server_connection_stop;
			cfg.connection_start_incoming_fnc = server_connection_start;
			cfg.connection_start_state = frame::ipc::ConnectionState::Active;
			
			cfg.listener_address_str = "0.0.0.0:0";
			
			cfg.writer.max_message_count_multiplex = 4;
			
			err = ipcserver.reconfigure(std::move(cfg));
			
			if(err){
				edbg("starting server ipcservice: "<<err.message());
				return 1;
			}
			
			{
				std::ostringstream oss;
				oss<<ipcserver.configuration().listenerPort();
				server_port = oss.str();
				idbg("server listens on port: "<<server_port);
			}
		}
		
		{//ipc client initialization
			frame::ipc::serialization_v1::Protocol	*proto = new frame::ipc::serialization_v1::Protocol;
			frame::ipc::Configuration				cfg(sch_client, proto);
			
			proto->registerType<Message>(
				client_complete_message
			);
			
			//cfg.recv_buffer_capacity = 1024;
			//cfg.send_buffer_capacity = 1024;
			
			cfg.connection_stop_fnc = client_connection_stop;
			cfg.connection_start_outgoing_fnc = client_connection_start;
			cfg.connection_start_state = frame::ipc::ConnectionState::Active;
			
			cfg.pool_max_active_connection_count = max_per_pool_connection_count;
			
			cfg.name_resolve_fnc = frame::ipc::InternetResolverF(resolver, server_port.c_str()/*, SocketInfo::Inet4*/);
			
			cfg.writer.max_message_count_multiplex = 2;
			
			err = ipcclient.reconfigure(std::move(cfg));
			
			if(err){
				edbg("starting client ipcservice: "<<err.message());
				return 1;
			}
		}
		
		pipcclient  = &ipcclient;
		
		const size_t		start_count = initarraysize;
		
		writecount = start_count;//
		{
			std::vector<frame::ipc::MessagePointerT>	msg_vec;
			ErrorConditionT								err;
			
			for(size_t i = 0; i < start_count; ++i){
				msg_vec.push_back(frame::ipc::MessagePointerT(new Message(i)));
			}
			{
				std::vector<frame::ipc::MessagePointerT>::iterator it = msg_vec.begin();
				
				{
					++crtwriteidx;
					ipcclient.sendMessage(
						"localhost", *it, recipinet_id, 0
					);
				}
				
				++it;
				
				for(; crtwriteidx < start_count; ++it){
					++crtwriteidx;
					err = ipcclient.sendMessage(
						recipinet_id, *it, 0
					);
					if(err){
						edbg("Message not sent: "<<err.message());
						++crtackidx;
					}
				}
			}
		}
		
		unique_lock<mutex>	lock(mtx);
		
		while(running){
			cnd.wait(lock);
			bool b = true;//cnd.wait(lock, abstime);
			if(!b){
				//timeout expired
				SOLID_THROW("Process is taking too long.");
			}
		}
		
		if(crtwriteidx != crtackidx){
			SOLID_THROW("Not all messages were completed");
		}
		
		//m.stop();
	}
	
	
	std::cout<<"Transfered size = "<<(transfered_size * 2)/1024<<"KB"<<endl;
	std::cout<<"Transfered count = "<<transfered_count<<endl;
	std::cout<<"Connection count = "<<connection_count<<endl;
	
	return 0;
}
