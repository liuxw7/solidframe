#include "gammaserver.hpp"
#include "../gammamessages.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"
#include "solid/serialization/v1/typeidmap.hpp"
#include "solid/system/log.hpp"

using namespace solid;
using namespace std;

namespace gamma_server {

template <class M>
void complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<M>&              _rsent_msg_ptr,
    std::shared_ptr<M>&              _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror)
{
    solid_dbg(generic_logger, Info, "");
    solid_check(!_rerror);
    if (_rrecv_msg_ptr) {
        solid_check(!_rsent_msg_ptr);
        ErrorConditionT err = _rctx.service().sendResponse(_rctx.recipientId(), std::move(_rrecv_msg_ptr));

        solid_check(!err, "Connection id should not be invalid! " << err.message());
    }
    if (_rsent_msg_ptr) {
        solid_check(!_rrecv_msg_ptr);
    }
}

struct MessageSetup {
    template <class T>
    void operator()(ProtocolT& _rprotocol, solid::TypeToType<T> _rt2t, const TypeIdT& _rtid)
    {
        _rprotocol.registerMessage<T>(complete_message<T>, _rtid);
    }
};

void register_messages(ProtocolT& _rprotocol)
{
    gamma_protocol::protocol_setup(MessageSetup(), _rprotocol);
}

} // namespace gamma_server
