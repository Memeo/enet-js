/* enet.js -- node wrapper for enet.
   Copyright (C) 2011 Memeo, Inc. */


var util = require('util');
var timers = require('timers');
var events = require('events');
var IOWatcher = process.binding('io_watcher').IOWatcher;
var enetnat = require('enetnat');

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

module.exports.Host = Host;