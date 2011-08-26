import os

def set_options(opt):
    opt.tool_options("compiler_cxx")
    opt.add_option("--enet-prefix", dest="enet_prefix",
        help="Set enet install prefix.")
    
def configure(conf):
    import Options
    if Options.options.enet_prefix != None:
        print "adding", os.path.join(Options.options.enet_prefix, "lib"), "to LINKFLAGS"
        conf.env.LINKFLAGS += ["-L" + os.path.join(Options.options.enet_prefix, "lib")]
        print conf.env
    conf.check_tool("compiler_cxx")
    conf.check_tool("node_addon")
    conf.check(lib='enet', uselib_store='enet', mandatory=True)
    
def build(bld):
    obj = bld.new_task_gen('cxx', 'shlib', 'node_addon')
    import Options
    if Options.options.enet_prefix != None:
        obj.env.append_value("_CXXINCFLAGS", "-I" + os.path.join(Options.options.enet_prefix, "include"))
        obj.env.append_value("LINKFLAGS", "-L" + os.path.join(Options.options.enet_prefix, "lib"))
    obj.target = 'enetnat'
    obj.source = 'enet.cc'
    obj.uselib = 'enet'