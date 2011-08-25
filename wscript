def set_options(opt):
    opt.tool_options("compiler_cxx")
    
def configure(conf):
    conf.check_tool("compiler_cxx")
    conf.check_tool("node_addon")
    conf.check(lib='enet', uselib_store='enet', mandatory = True)
    
def build(bld):
    obj = bld.new_task_gen('cxx', 'shlib', 'node_addon')
    obj.target = 'enetnat'
    obj.source = 'enet.cc'
    obj.uselib = 'enet'