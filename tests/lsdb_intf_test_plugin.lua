-- Minimal lsdb-intf plugin used by test_lsdb_intf.lua
local M = {}

function M.init(ctx)
   M.ctx = ctx
   return {
      path = "/testplugin",
      intf = {
         name = "org.test.plugin",
         methods = {
            Echo = {
               { direction='in',  name='msg',   type='s' },
               { direction='out', name='reply',  type='s' },
               handler = function(vt, msg) return msg end,
            },
            Fail = {
               handler = function(vt) error("plugin handler failed") end,
            },
            NodeName = {
               { direction='out', name='name', type='s' },
               handler = function(vt) return ctx.nd:get_name() end,
            },
            -- load usc with the number t as parameter T via the plugin API
            LoadUSCParam = {
               { direction='in', name='usc', type='s' },
               { direction='in', name='t',   type='d' },
               handler = function(vt, usc, t) ctx.api.load_usc_lua(usc, { T=t }) end,
            },
         },
      },
   }
end

function M.cleanup()
   M.cleanup_called = true
end

return M
