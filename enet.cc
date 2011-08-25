/* enet.cc -- node.js to enet wrapper.
   Copyright (C) 2011 Memeo, Inc. */
   
#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <node_events.h>
#include <enet/enet.h>
#include <cstring>

#ifdef DEBUG
#define debug(fmt, args...) fprintf(stderr, fmt, ##args)
#else
#define debug(fmt, args...)
#endif

#define MY_NODE_DEFINE_CONSTANT(target, name, value)                            \
       (target)->Set(v8::String::NewSymbol(name),                               \
                     v8::Integer::New(value),                                   \
                     static_cast<v8::PropertyAttribute>(v8::ReadOnly|v8::DontDelete))

namespace enet
{
    
class Host;
class Peer;

class Packet : public node::ObjectWrap
{
private:
    friend class Host;
    friend class Peer;
    ENetPacket *packet;
    bool isSent;
    
public:
    Packet(const void *data, const size_t dataLength, enet_uint32 flags)
        : isSent(false)
    {
        packet = enet_packet_create(data, dataLength, flags);
        debug(stderr, "%p Packet(%p, %d, %x) -- %p\n", this, data, dataLength, flags, packet);
    }
    
    Packet(enet_uint32 flags)
        : isSent(false)
    {
        packet = enet_packet_create(NULL, 0, flags);
        debug(stderr, "%p Packet(%x) -- %p\n", this, flags, packet);
    }
    
    Packet() : packet(0), isSent(false)
    {
        debug(stderr, "%p Packet() -- %p\n", this, packet);
    }
    
    ~Packet()
    {
        debug(stderr, "%p ~Packet() -- %p\n", this, packet);
        if (packet && !isSent)
        {
            enet_packet_destroy(packet);
        }
    }
    
    static v8::Persistent<v8::FunctionTemplate> s_ct;
    
    static void Init(v8::Handle<v8::Object> target)
    {
        v8::HandleScope scope;
        v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(New);
        s_ct = v8::Persistent<v8::FunctionTemplate>::New(t);
        s_ct->InstanceTemplate()->SetInternalFieldCount(1);
        s_ct->SetClassName(v8::String::NewSymbol("Packet"));
        NODE_SET_PROTOTYPE_METHOD(s_ct, "data", Data);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "setData", SetData);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "flags", Flags);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "setFlags", SetFlags);
        MY_NODE_DEFINE_CONSTANT(s_ct, "FLAG_RELIABLE", ENET_PACKET_FLAG_RELIABLE);
        MY_NODE_DEFINE_CONSTANT(s_ct, "FLAG_UNSEQUENCED", ENET_PACKET_FLAG_UNSEQUENCED);
        MY_NODE_DEFINE_CONSTANT(s_ct, "FLAG_NO_ALLOCATE", ENET_PACKET_FLAG_NO_ALLOCATE);
        MY_NODE_DEFINE_CONSTANT(s_ct, "FLAG_UNRELIABLE_FRAGMENT", ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT);
        target->Set(v8::String::NewSymbol("Packet"), s_ct->GetFunction());
    }
    
    static v8::Handle<v8::Value> New(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Packet *packet = NULL;
        enet_uint32 flags = 0;
        if (args.Length() > 1)
        {
            if (args[0]->IsInt32())
            {
                flags = (enet_uint32) args[0]->Int32Value();
            }
            else if (args[0]->IsUint32())
            {
                flags = args[0]->Uint32Value();
            }
        }
        if (args.Length() > 0)
        {
            if (args[0]->IsObject())
            {
                // Assume it is a Buffer.
                size_t length = node::Buffer::Length(args[0]->ToObject());
                packet = new Packet(node::Buffer::Data(args[0]->ToObject()), length, flags);
            }
            else if (args[0]->IsString())
            {
                v8::String::Utf8Value utf8(args[0]);
                packet = new Packet(*utf8, utf8.length(), flags);
            }
        }
        if (args.Length() == 0)
            packet = new Packet();
        if (packet != NULL)
        {
            packet->Wrap(args.This());
        }
        return args.This();
    }
    
    static v8::Handle<v8::Value> WrapPacket(ENetPacket *p)
    {
        v8::Local<v8::Object> o = s_ct->InstanceTemplate()->NewInstance();
        Packet *packet = node::ObjectWrap::Unwrap<Packet>(o);
        packet->packet = enet_packet_create(p->data, p->dataLength, p->flags);
        return o;
    }
    
    static v8::Handle<v8::Value> Data(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Packet *packet = node::ObjectWrap::Unwrap<Packet>(args.This());
        if (packet->isSent)
        {
            return v8::ThrowException(v8::Exception::Error(v8::String::New("packet has been sent and is now invalid")));
        }
        node::Buffer *slowBuf = node::Buffer::New(packet->packet->dataLength);
        ::memcpy((void *) node::Buffer::Data(slowBuf), packet->packet->data,
            packet->packet->dataLength);
        v8::Local<v8::Object> globalObj = v8::Context::GetCurrent()->Global();
        v8::Local<v8::Function> bufferConstructor = v8::Local<v8::Function>::Cast(globalObj->Get(v8::String::New("Buffer")));
        v8::Handle<v8::Value> constructorArgs[3] = { slowBuf->handle_, v8::Integer::New(packet->packet->dataLength), v8::Integer::New(0) };
        v8::Local<v8::Object> actualBuffer = bufferConstructor->NewInstance(3, constructorArgs);
        return scope.Close(actualBuffer);
    }
    
    static v8::Handle<v8::Value> Flags(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Packet *packet = node::ObjectWrap::Unwrap<Packet>(args.This());
        if (packet->isSent)
        {
            return v8::ThrowException(v8::Exception::Error(v8::String::New("packet has been sent and is now invalid")));
        }
        return scope.Close(v8::Uint32::New(packet->packet->flags));
    }
    
    static v8::Handle<v8::Value> SetData(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Packet *packet = node::ObjectWrap::Unwrap<Packet>(args.This());
        if (packet->isSent)
        {
            return v8::ThrowException(v8::Exception::Error(v8::String::New("packet has been sent and is now invalid")));
        }
        if (args.Length() > 0)
        {
            if (args[0]->IsObject())
            {
                // Assume it is a Buffer.
                size_t length = node::Buffer::Length(args[0]->ToObject());
                enet_packet_resize(packet->packet, length);
                ::memcpy(packet->packet->data, node::Buffer::Data(args[0]->ToObject()), length);
            }
            else if (args[0]->IsString())
            {
                v8::String::Utf8Value utf8(args[0]);
                enet_packet_resize(packet->packet, utf8.length());
                ::memcpy(packet->packet->data, *utf8, utf8.length());
            }
        }
        else
        {
            enet_packet_resize(packet->packet, 0);
        }
        v8::Local<v8::Value> result;
        return scope.Close(result);
    }
    
    static v8::Handle<v8::Value> SetFlags(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Packet *packet = node::ObjectWrap::Unwrap<Packet>(args.This());
        if (packet->isSent)
        {
            return v8::ThrowException(v8::Exception::Error(v8::String::New("packet has been sent and is now invalid")));
        }
        if (args.Length() > 0)
        {
            if (args[0]->IsInt32())
            {
                packet->packet->flags = (enet_uint32) args[0]->Int32Value();
            }
            else if (args[0]->IsUint32())
            {
                packet->packet->flags = args[0]->Uint32Value();
            }
        }
        v8::Local<v8::Value> result;
        return scope.Close(result);
    }
};

class Address: public node::ObjectWrap
{
private:
    friend class Host;
    ENetAddress address;
    
public:
    Address()
    {
        ::memset(&address, 0, sizeof(ENetAddress));
    }
    
    Address(uint32_t host, enet_uint16 port)
    {
        address.host = host;
        address.port = port;
    }
    
    Address(const char *addrstr)
    {
        char *s = ::strdup(addrstr);
        char *chr = ::strrchr(s, ':');
        if (chr != NULL)
        {
            *chr = '\0';
            address.port = atoi(chr + 1);
        }
        enet_address_set_host(&address, (const char *) s);
        ::free(s);
    }
    
    Address(const char *addrstr, enet_uint16 port)
    {
        enet_address_set_host(&address, addrstr);
        address.port = port;        
    }
    
    Address(ENetAddress addr) : address(addr) { }
    
    Address(Address *addr) : address(addr->address) { }
    
    static v8::Persistent<v8::FunctionTemplate> s_ct;
    
    static void Init(v8::Handle<v8::Object> target)
    {
        v8::HandleScope scope;
        v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(New);
        s_ct = v8::Persistent<v8::FunctionTemplate>::New(t);
        s_ct->InstanceTemplate()->SetInternalFieldCount(1);
        s_ct->SetClassName(v8::String::NewSymbol("Address"));
        // host -- the IP address, as an integer.
        NODE_SET_PROTOTYPE_METHOD(s_ct, "host", Host);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "setHost", SetHost);
        // port -- the port number as an integer.
        NODE_SET_PROTOTYPE_METHOD(s_ct, "port", Port);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "setPort", SetPort);
        // hostname -- the hostname associated with the address, if any
        // set looks up the address via DNS
        NODE_SET_PROTOTYPE_METHOD(s_ct, "hostname", Hostname);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "setHostname", SetHostname);
        // address -- the IP address in dotted-decimal format
        NODE_SET_PROTOTYPE_METHOD(s_ct, "address", GetAddress);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "setAddress", SetHostname); // uses the same function internally.
        MY_NODE_DEFINE_CONSTANT(s_ct, "HOST_ANY", ENET_HOST_ANY);
        MY_NODE_DEFINE_CONSTANT(s_ct, "HOST_BROADCAST", ENET_HOST_BROADCAST);
        MY_NODE_DEFINE_CONSTANT(s_ct, "PORT_ANY", ENET_PORT_ANY);
        target->Set(v8::String::NewSymbol("Address"), s_ct->GetFunction());
    }
    
    static v8::Handle<v8::Value> New(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Address *addr = NULL;
        if (args.Length() == 1)
        {
            if (args[0]->IsString())
            {
                v8::String::AsciiValue val(args[0]);
                addr = new Address(*val);
            }
            else if (args[0]->IsUint32())
            {
                addr = new Address(args[0]->Uint32Value(), ENET_PORT_ANY);
            }
            else if (args[0]->IsInt32())
            {
                addr = new Address((uint32_t) args[0]->Int32Value(), ENET_PORT_ANY);
            }
        }
        else if (args.Length() == 2)
        {
            if (args[0]->IsString())
            {
                v8::String::AsciiValue val(args[0]);
                if (args[1]->IsUint32())
                {
                    addr = new Address(*val, (enet_uint16) args[1]->Uint32Value());
                }
                else if (args[1]->IsInt32())
                {
                    addr = new Address(*val, (enet_uint16) args[1]->Int32Value());
                }
            }
            else if (args[0]->IsUint32())
            {
                uint32_t val = args[0]->Uint32Value();
                if (args[1]->IsUint32())
                {
                    addr = new Address(val, (enet_uint16) args[1]->Uint32Value());
                }
                else if (args[1]->IsInt32())
                {
                    addr = new Address(val, (enet_uint16) args[1]->Int32Value());
                }                
            }
        }
        else
        {
            addr = new Address();
        }
        if (addr != NULL)
        {
            addr->Wrap(args.This());
        }
        else
        {
            return v8::ThrowException(v8::String::New("invalid argument"));
        }
        return scope.Close(args.This());
    }
    
    static v8::Handle<v8::Value> WrapAddress(ENetAddress address)
    {
        v8::Handle<v8::Object> o = s_ct->InstanceTemplate()->NewInstance();
        Address *a = node::ObjectWrap::Unwrap<Address>(o);
        a->address = address;
        return o;        
    }
    
    static v8::Handle<v8::Value> Host(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Address *address = node::ObjectWrap::Unwrap<Address>(args.This());
        return scope.Close(v8::Uint32::New(address->address.host));
    }
    
    static v8::Handle<v8::Value> Port(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Address *address = node::ObjectWrap::Unwrap<Address>(args.This());
        return scope.Close(v8::Int32::New(address->address.port));
    }
    
    static v8::Handle<v8::Value> Hostname(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        char buffer[256];
        Address *address = node::ObjectWrap::Unwrap<Address>(args.This());
        if (enet_address_get_host(&(address->address), buffer, 256) == 0)
            return scope.Close(v8::String::New(buffer));
        v8::Handle<v8::Value> ret;
        return scope.Close(ret);
    }
    
    static v8::Handle<v8::Value> GetAddress(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        char buffer[256];
        Address *address = node::ObjectWrap::Unwrap<Address>(args.This());
        if (enet_address_get_host_ip(&(address->address), buffer, 256) == 0)
            return scope.Close(v8::String::New(buffer));
        v8::Handle<v8::Value> ret;
        return scope.Close(ret);        
    }
    
    static v8::Handle<v8::Value> SetHost(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        v8::Handle<v8::Value> ret;
        Address *address = node::ObjectWrap::Unwrap<Address>(args.This());
        if (args[0]->IsUint32())
        {
            address->address.host = args[0]->Uint32Value();
        }
        scope.Close(ret);
    }

    static v8::Handle<v8::Value> SetPort(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        v8::Handle<v8::Value> ret;
        Address *address = node::ObjectWrap::Unwrap<Address>(args.This());
        if (args[0]->IsInt32())
        {
            address->address.port = (enet_uint16) args[0]->Int32Value();
        }
        scope.Close(ret);
    }
    
    static v8::Handle<v8::Value> SetHostname(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        v8::Handle<v8::Value> ret;
        Address *address = node::ObjectWrap::Unwrap<Address>(args.This());
        bool success = false;
        if (args[0]->IsString())
        {
            v8::String::Utf8Value utf8(args[0]);
            if (enet_address_set_host(&(address->address), *utf8) == 0)
                success = true;
        }
        scope.Close(v8::Boolean::New(success));
    }
};

class Peer : public node::ObjectWrap
{
private:
     ENetPeer *peer;

public:
    Peer(ENetPeer *peer) : peer(peer) { }
    
    ~Peer() { }
    
    static v8::Persistent<v8::FunctionTemplate> s_ct;
    
    static void Init(v8::Handle<v8::Object> target)
    {
        v8::HandleScope scope;
        v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New();
        s_ct = v8::Persistent<v8::FunctionTemplate>::New(t);
        s_ct->InstanceTemplate()->SetInternalFieldCount(1);
        s_ct->SetClassName(v8::String::NewSymbol("Peer"));
        NODE_SET_PROTOTYPE_METHOD(s_ct, "send", Send);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "receive", Receive);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "reset", Reset);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "ping", Ping);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "disconnectNow", DisconnectNow);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "disconnect", Disconnect);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "disconnectLater", DisconnectLater);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "address", GetAddress);
        target->Set(v8::String::NewSymbol("Peer"), s_ct->GetFunction());
    }
    
    static v8::Handle<v8::Value> New(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Peer *peer = new Peer(NULL);
        peer->Wrap(args.This());
        return scope.Close(args.This());
    }
    
    static v8::Handle<v8::Value> WrapPeer(ENetPeer *p)
    {
        Peer *peer = new Peer(p);
        v8::Local<v8::Object> o = s_ct->InstanceTemplate()->NewInstance();
        peer->Wrap(o);
        return o;
    }
    
    static v8::Handle<v8::Value> Send(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Peer *peer = node::ObjectWrap::Unwrap<Peer>(args.This());
        if (args.Length() != 2 || !args[0]->IsInt32() || !args[1]->IsObject())
        {
            return v8::ThrowException(v8::Exception::Error(v8::String::New("send requires two arguments, channel number, packet")));
        }
        enet_uint8 channel = (enet_uint8) args[0]->Int32Value();
        Packet *packet = node::ObjectWrap::Unwrap<Packet>(args[1]->ToObject());
        if (enet_peer_send(peer->peer, channel, packet->packet) < 0)
        {
            return v8::ThrowException(v8::Exception::Error(v8::String::New("enet.Peer.send error")));
        }
        packet->isSent = true;
        return v8::Undefined();
    }
    
    static v8::Handle<v8::Value> Receive(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Peer *peer = node::ObjectWrap::Unwrap<Peer>(args.This());
        enet_uint8 channelID = 0;
        v8::Local<v8::Array> result = v8::Array::New(2);
        ENetPacket *packet = enet_peer_receive(peer->peer, &channelID);
        if (packet == NULL)
            return scope.Close(v8::Null());
        result->Set(0, v8::Int32::New(channelID));
        v8::Handle<v8::Value> wrapper = Packet::WrapPacket(packet);
        result->Set(1, wrapper);
        return scope.Close(result);
    }
    
    static v8::Handle<v8::Value> Reset(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Peer *peer = node::ObjectWrap::Unwrap<Peer>(args.This());
        enet_peer_reset(peer->peer);
        return scope.Close(v8::Undefined());
    }
    
    static v8::Handle<v8::Value> Ping(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Peer *peer = node::ObjectWrap::Unwrap<Peer>(args.This());
        enet_peer_ping(peer->peer);
        return scope.Close(v8::Undefined());        
    }
    
    static v8::Handle<v8::Value> DisconnectNow(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Peer *peer = node::ObjectWrap::Unwrap<Peer>(args.This());
        enet_uint32 data = 0;
        if (args.Length() > 0)
            data = args[0]->Uint32Value();
        enet_peer_disconnect_now(peer->peer, data);
        return scope.Close(v8::Undefined());        
    }

    static v8::Handle<v8::Value> Disconnect(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Peer *peer = node::ObjectWrap::Unwrap<Peer>(args.This());
        enet_uint32 data = 0;
        if (args.Length() > 0)
            data = args[0]->Uint32Value();
        enet_peer_disconnect(peer->peer, data);
        return scope.Close(v8::Undefined());        
    }

    static v8::Handle<v8::Value> DisconnectLater(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Peer *peer = node::ObjectWrap::Unwrap<Peer>(args.This());
        enet_uint32 data = 0;
        if (args.Length() > 0)
            data = args[0]->Uint32Value();
        enet_peer_disconnect_later(peer->peer, data);
        return scope.Close(v8::Undefined());        
    }
    
    static v8::Handle<v8::Value> GetAddress(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Peer *peer = node::ObjectWrap::Unwrap<Peer>(args.This());
        return scope.Close(Address::WrapAddress(peer->peer->address));
    }
};

class Event : node::ObjectWrap
{
private:
    ENetEvent event;
    
public:
    Event(ENetEvent event) : event(event)
    {
    }
    
    static v8::Persistent<v8::FunctionTemplate> s_ct;
    
    static void Init(v8::Handle<v8::Object> target)
    {
        v8::HandleScope scope;
        v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New();
        s_ct = v8::Persistent<v8::FunctionTemplate>::New(t);
        s_ct->InstanceTemplate()->SetInternalFieldCount(1);
        s_ct->SetClassName(v8::String::NewSymbol("Event"));
        NODE_SET_PROTOTYPE_METHOD(s_ct, "type", Type);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "peer", GetPeer);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "channelID", ChannelID);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "data", Data);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "packet", GetPacket);
        MY_NODE_DEFINE_CONSTANT(s_ct, "TYPE_NONE", ENET_EVENT_TYPE_NONE);
        MY_NODE_DEFINE_CONSTANT(s_ct, "TYPE_CONNECT", ENET_EVENT_TYPE_CONNECT);
        MY_NODE_DEFINE_CONSTANT(s_ct, "TYPE_DISCONNECT", ENET_EVENT_TYPE_DISCONNECT);
        MY_NODE_DEFINE_CONSTANT(s_ct, "TYPE_RECEIVE", ENET_EVENT_TYPE_RECEIVE);
        target->Set(v8::String::NewSymbol("Event"), s_ct->GetFunction());
    }
    
    static v8::Handle<v8::Value> WrapEvent(ENetEvent e)
    {
        Event *event = new Event(e);
        v8::Handle<v8::Object> o = s_ct->InstanceTemplate()->NewInstance();
        event->Wrap(o);
        return o;
    }
    
    static v8::Handle<v8::Value> Type(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Event *e = node::ObjectWrap::Unwrap<Event>(args.This());
        return scope.Close(v8::Int32::New(e->event.type));
    }
    
    static v8::Handle<v8::Value> GetPeer(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Event *e = node::ObjectWrap::Unwrap<Event>(args.This());
        if (e->event.peer == NULL)
            return scope.Close(v8::Null());
        v8::Handle<v8::Value> result = Peer::WrapPeer(e->event.peer);
        return scope.Close(result);
    }
    
    static v8::Handle<v8::Value> ChannelID(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Event *e = node::ObjectWrap::Unwrap<Event>(args.This());
        return scope.Close(v8::Int32::New(e->event.channelID));        
    }
    
    static v8::Handle<v8::Value> Data(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Event *e = node::ObjectWrap::Unwrap<Event>(args.This());
        return scope.Close(v8::Uint32::New(e->event.data));
    }
    
    static v8::Handle<v8::Value> GetPacket(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Event *e = node::ObjectWrap::Unwrap<Event>(args.This());
        if (e->event.packet == NULL)
            return scope.Close(v8::Null());
        v8::Handle<v8::Value> result = Packet::WrapPacket(e->event.packet);
        return scope.Close(result);
    }
};

class Host : node::EventEmitter
{
private:
    ENetHost *host;
    Address *address;
    size_t peerCount;
    size_t channelLimit;
    enet_uint32 incomingBandwidth;
    enet_uint32 outgoingBandwidth;
    
public:
    Host(Address *address_, size_t peerCount, size_t channelLimit, enet_uint32 incomingBandwidth, enet_uint32 outgoingBandwidth)
        : address(0), peerCount(peerCount), channelLimit(channelLimit),
          incomingBandwidth(incomingBandwidth), outgoingBandwidth(outgoingBandwidth)
    {
        ENetAddress *addr = NULL;
        if (address_ != NULL)
        {
            addr = &(address_->address);
            address = new Address(*addr);
        }
        host = enet_host_create(addr, peerCount, channelLimit, incomingBandwidth, outgoingBandwidth);
        if (host == NULL)
        {
            throw "failed to create host";
        }
    }
    
    ~Host()
    {
        enet_host_destroy(host);
        if (address != NULL)
        {
            delete address;
        }
    }
    
    static v8::Persistent<v8::FunctionTemplate> s_ct;
    
    static void Init(v8::Handle<v8::Object> target)
    {
        v8::HandleScope scope;
        v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(New);
        s_ct = v8::Persistent<v8::FunctionTemplate>::New(t);
        s_ct->InstanceTemplate()->SetInternalFieldCount(1);
        s_ct->SetClassName(v8::String::NewSymbol("Host"));
        NODE_SET_PROTOTYPE_METHOD(s_ct, "connect", Connect);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "broadcast", Broadcast);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "address", GetAddress);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "peerCount", PeerCount);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "channelLimit", ChannelLimit);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "setChannelLimit", SetChannelLimit);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "incomingBandwidth", IncomingBandwidth);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "outgoingBandwidth", OutgoingBandwidth);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "setBandwidthLimit", SetBandwidthLimit);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "flush", Flush);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "checkEvents", CheckEvents);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "service", Service);
        NODE_SET_PROTOTYPE_METHOD(s_ct, "fd", FD);
        target->Set(v8::String::NewSymbol("Host"), s_ct->GetFunction());
    }
    
    static v8::Handle<v8::Value> New(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        if (args.Length() < 2)
            return v8::ThrowException(v8::String::New("constructor takes at least two arguments"));
        Address *addr = node::ObjectWrap::Unwrap<Address>(args[0]->ToObject());
        size_t peerCount = args[1]->Int32Value();
        size_t channelCount = 0;
        enet_uint32 incomingBW = 0;
        enet_uint32 outgoingBW = 0;
        if (args.Length() > 2)
            channelCount = args[2]->Int32Value();
        if (args.Length() > 3)
            incomingBW = args[3]->Uint32Value();
        if (args.Length() > 4)
            outgoingBW = args[4]->Uint32Value();
        try
        {
            Host *host = new Host(addr, peerCount, channelCount, incomingBW, outgoingBW);
            host->Wrap(args.This());
            return scope.Close(args.This());
        }
        catch (...)
        {
            return v8::ThrowException(v8::Exception::Error(v8::String::New("could not create host")));
        }
    }
    
    static v8::Handle<v8::Value> Connect(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Host *host = node::ObjectWrap::Unwrap<Host>(args.This());
        if (args.Length() < 2 || !args[0]->IsObject() || !args[1]->IsInt32())
            return v8::ThrowException(v8::Exception::Error(v8::String::New("invalid argument")));
        Address *address = node::ObjectWrap::Unwrap<Address>(args[0]->ToObject());
        size_t channelCount = args[1]->Int32Value();
        enet_uint32 data = 0;
        if (args.Length() > 2)
        {
            if (!args[2]->IsUint32())
                return v8::ThrowException(v8::Exception::Error(v8::String::New("invalid data argument")));
            data = args[2]->Uint32Value();
        }
        ENetPeer *ep = enet_host_connect(host->host,
            (const ENetAddress *) &(address->address), channelCount, data);
        if (ep == NULL)
            return v8::Null();
        return scope.Close(Peer::WrapPeer(ep));
    }
    
    static v8::Handle<v8::Value> Broadcast(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Host *host = node::ObjectWrap::Unwrap<Host>(args.This());
        enet_uint8 channelID = args[0]->Int32Value();
        Packet *packet = node::ObjectWrap::Unwrap<Packet>(args[1]->ToObject());
        enet_host_broadcast(host->host, channelID, packet->packet);
        return v8::Undefined();
    }
    
    static v8::Handle<v8::Value> GetAddress(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Host *host = node::ObjectWrap::Unwrap<Host>(args.This());
        v8::Handle<v8::Value> result = Address::WrapAddress(host->address->address);
        return scope.Close(result);
    }
    
    static v8::Handle<v8::Value> PeerCount(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Host *host = node::ObjectWrap::Unwrap<Host>(args.This());
        return scope.Close(v8::Int32::New(host->peerCount));
    }

    static v8::Handle<v8::Value> ChannelLimit(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Host *host = node::ObjectWrap::Unwrap<Host>(args.This());
        return scope.Close(v8::Int32::New(host->channelLimit));
    }
    
    static v8::Handle<v8::Value> SetChannelLimit(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Host *host = node::ObjectWrap::Unwrap<Host>(args.This());
        size_t newLimit = args[0]->Int32Value();
        enet_host_channel_limit(host->host, newLimit);
        host->channelLimit = newLimit;
        return v8::Undefined();
    }

    static v8::Handle<v8::Value> IncomingBandwidth(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Host *host = node::ObjectWrap::Unwrap<Host>(args.This());
        return scope.Close(v8::Uint32::New(host->incomingBandwidth));
    }

    static v8::Handle<v8::Value> OutgoingBandwidth(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Host *host = node::ObjectWrap::Unwrap<Host>(args.This());
        return scope.Close(v8::Uint32::New(host->outgoingBandwidth));
    }
    
    static v8::Handle<v8::Value> SetBandwidthLimit(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Host *host = node::ObjectWrap::Unwrap<Host>(args.This());
        enet_uint32 inbw = args[0]->Uint32Value();
        enet_uint32 outbw = args[1]->Uint32Value();
        enet_host_bandwidth_limit(host->host, inbw, outbw);
        host->incomingBandwidth = inbw;
        host->outgoingBandwidth = outbw;
        return v8::Undefined();
    }
    
    static v8::Handle<v8::Value> Flush(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Host *host = node::ObjectWrap::Unwrap<Host>(args.This());
        enet_host_flush(host->host);
        return v8::Undefined();
    }
    
    static v8::Handle<v8::Value> CheckEvents(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Host *host = node::ObjectWrap::Unwrap<Host>(args.This());
        ENetEvent event;
        int ret = enet_host_check_events(host->host, &event);
        if (ret < 0)
            return v8::ThrowException(v8::String::New("error checking events"));
        if (ret < 1)
            return v8::Null();
        v8::Handle<v8::Value> result = Event::WrapEvent(event);
        return scope.Close(result);
    }
    
    static v8::Handle<v8::Value> Service(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Host *host = node::ObjectWrap::Unwrap<Host>(args.This());
        enet_uint32 timeout = 0;
        if (args.Length() > 0)
            timeout = args[0]->Uint32Value();
        ENetEvent event;
        int ret = enet_host_service(host->host, &event, timeout);
        if (ret < 0)
            return v8::ThrowException(v8::String::New("error servicing host"));
        if (ret < 1)
            return v8::Null();
        v8::Handle<v8::Value> result = Event::WrapEvent(event);
        return scope.Close(result);        
    }
    
    static v8::Handle<v8::Value> FD(const v8::Arguments& args)
    {
        v8::HandleScope scope;
        Host *host = node::ObjectWrap::Unwrap<Host>(args.This());
        return scope.Close(v8::Int32::New(host->host->socket));
    }
};

}

v8::Persistent<v8::FunctionTemplate> enet::Packet::s_ct;
v8::Persistent<v8::FunctionTemplate> enet::Address::s_ct;
v8::Persistent<v8::FunctionTemplate> enet::Peer::s_ct;
v8::Persistent<v8::FunctionTemplate> enet::Event::s_ct;
v8::Persistent<v8::FunctionTemplate> enet::Host::s_ct;

extern "C"
{
    void init(v8::Handle<v8::Object> target)
    {
        enet::Packet::Init(target);
        enet::Address::Init(target);
        enet::Event::Init(target);
        enet::Host::Init(target);
        enet::Peer::Init(target);
        
        enet_initialize();
    }
    
    NODE_MODULE(enetnat, init);
}