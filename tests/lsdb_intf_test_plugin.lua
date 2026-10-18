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
            -- start a ctx.bus timer counting ticks every period_us (1 ms
            -- accuracy: the sd-event default of 0 means 250 ms)
            StartTimer = {
               { direction='in', name='period_us', type='t' },
               handler = function(vt, period_us)
                  M.ticks = 0
                  M.timer = ctx.bus:add_periodic(period_us, 1000,
                     function() M.ticks = M.ticks + 1 end)
               end,
            },
            Ticks = {
               { direction='out', name='ticks', type='u' },
               handler = function(vt) return M.ticks or 0 end,
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
