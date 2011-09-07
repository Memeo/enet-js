/* enet.js -- node wrapper for enet.
   Copyright (C) 2011 Memeo, Inc. */


var util = require('util');
var timers = require('timers');
var events = require('events');
var IOWatcher = process.binding('io_watcher').IOWatcher;
var enetnat = require('./enetnat');

module.exports.Address = enetnat.Address;
module.exports.Peer = enetnat.Peer;
module.exports.Packet = enetnat.Packet;
module.exports.NatHost = enetnat.Host;

function Host()
{
    events.EventEmitter.call(this);
    var self = this;

    if (arguments.length == 2)
    {
        self.host = new enetnat.Host(arguments[0], arguments[1]);
    }
    else if (arguments.length == 3)
    {
        self.host = new enetnat.Host(arguments[0], arguments[1], arguments[2]);
    }
    else if (arguments.length == 4)
    {
        self.host = new enetnat.Host(arguments[0], arguments[1], arguments[2],
            arguments[3]);
    }
    else if (arguments.length == 5)
    {
        self.host = new enetnat.Host(arguments[0], arguments[1], arguments[2],
            arguments[3], arguments[4]);
    }
    else
    {
        throw Error('expected between 2 and 5 arguments')
    }
    self.watcher = new IOWatcher();
    self.runloop = function() {
        try
        {
            var event = self.host.service(0);
            while (event != null)
            {
                switch (event.type())
                {
                case enetnat.Event.TYPE_NONE:
                    break;
                    
                case enetnat.Event.TYPE_CONNECT:
                    self.emit('connect', event.peer(), event.data());
                    break;
                    
                case enetnat.Event.TYPE_DISCONNECT:
                    self.emit('disconnect', event.peer(), event.data());
                    break;
                    
                case enetnat.Event.TYPE_RECEIVE:
                    self.emit('message', event.peer(), event.packet(), event.channelID(), event.data());
                    break;
                }

                event = self.host.service(0);
            }
        }
        catch (e)
        {
            self.emit('error', e);
        }
    };
    self.watcher.callback = self.runloop;
    self.watcher.host = self;
    self.watcher_running = false;
}

util.inherits(Host, events.EventEmitter);

Host.prototype.start_watcher = function()
{
    if (!this.watcher_running)
    {
        this.watcher.set(this.host.fd(), true, false);
        this.watcher.start();
        this.intervalId = timers.setInterval(this.runloop, 100);
        this.watcher_running = true;
    }
}

Host.prototype.stop_watcher = function()
{
    if (this.watcher_running)
    {
        this.watcher.stop();
        timers.clearInterval(this.intervalId);
        this.watcher_running = false;
    }
}

// Now for some convenience methods, which just pass along to the native
// host object.

Host.prototype.connect = function()
{
    return this.host.connect.apply(this.host, arguments);
}

Host.prototype.broadcast = function()
{
    return this.host.broadcast.apply(this.host, arguments);
}

Host.prototype.address = function()
{
    return this.host.address();
}

Host.prototype.peerCount = function()
{
    return this.host.peerCount();
}

Host.prototype.channelLimit = function()
{
    return this.host.channelLimit();
}

Host.prototype.setChannelLimit = function(limit)
{
    return this.host.setChannelLimit(limit);
}

Host.prototype.incomingBandwidth = function()
{
    return this.host.incomingBandwidth();
}

Host.prototype.outgoingBandwidth = function()
{
    return this.host.outgoingBandwidth();
}

Host.prototype.setIncomingBandwidth = function(ibw)
{
    return this.host.setIncomingBandwidth(ibw);
}

Host.prototype.setOutgoingBandwidth = function(obw)
{
    return this.host.setOutgoingBandwidth(obw);
}

Host.prototype.flush = function()
{
    return this.host.flush();
}

Host.prototype.checkEvents = function()
{
    return this.host.checkEvents();
}

Host.prototype.service = function(timeout)
{
    return this.host.service(timeout);
}

module.exports.Host = Host;