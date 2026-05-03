-- webgraph.lua - luablock: React Flow + ELK visualization of ubx blocks
--
-- Provides a tiny HTTP server (default port 8888) that serves an
-- interactive graph of the current block instances and connections.
-- Only depends on luasocket and json.lua (both available in ubx env).
--
-- Usage: instantiate as a luablock, optionally set the 'port' config.
-- The block must be stepped (e.g. by ptrig or thread=1 in luablock cfg).

local ffi = require("ffi")
local ubx = require("ubx")
local utils = require("utils")

local _json_ok, json   = pcall(require, "json")
local _sock_ok, socket = pcall(require, "socket")
local _missing_deps = {}
if not _json_ok   then _missing_deps[#_missing_deps+1] = "json (json.lua)" end
if not _sock_ok   then _missing_deps[#_missing_deps+1] = "socket (luasocket)" end

local srv = nil
local node_ref = nil  -- cached node pointer (set in start)

-- ============================================================
-- Graph data extraction
-- ============================================================

-- Collect all iblock configs by name for connection metadata
local function iblock_cfg(nd, iblock_name)
   local b = ubx.block_get(nd, iblock_name)
   if b == nil then return {} end
   local bt = ubx.block_totab(b)
   local cfg = {}
   for _, c in ipairs(bt.configs) do
      cfg[c.name] = c.value
   end
   return cfg
end

-- Build JSON-serialisable graph from the current node state
local function build_graph(nd)
   -- ensure all module-registered types (e.g. ptrig_period) are declared in FFI
   ubx.ffi_load_types(nd)
   local instances = ubx.blocks_map(nd, ubx.block_totab, ubx.is_instance)

   -- Separate cblocks and iblocks
   local cblocks, iblocks = {}, {}
   for _, b in ipairs(instances) do
      if b.block_type == "cblock" then
         cblocks[#cblocks+1] = b
      else
         iblocks[b.name] = b
      end
   end

   -- Build cblock nodes
   local nodes = {}
   for _, b in ipairs(cblocks) do
      local ports = {}
      for _, p in ipairs(b.ports) do
         ports[#ports+1] = {
            name      = p.name,
            in_type   = p.in_type_name,
            in_len    = p.in_data_len,
            out_type  = p.out_type_name,
            out_len   = p.out_data_len,
         }
      end

      local configs = {}
      for _, c in ipairs(b.configs) do
         configs[#configs+1] = {
            name  = c.name,
            type  = c.type_name,
            value = utils.tab2str(c.value),
         }
      end

      nodes[#nodes+1] = {
         id             = b.name,
         name           = b.name,
         prototype      = b.prototype or "",
         state          = b.state,
         attrs          = b.attrs,
         ports          = ports,
         configs        = configs,
         stat_num_steps = b.stat_num_steps,
      }
   end

   -- Build edges: walk outgoing connections of every cblock port.
   -- Each outgoing entry is an iblock name; find cblocks whose port
   -- has that iblock in their incoming list.
   --
   -- incoming_map[iblock_name] = { {block, port, in_type, in_len}, ... }
   local incoming_map = {}
   for _, b in ipairs(cblocks) do
      for _, p in ipairs(b.ports) do
         for _, ib in ipairs(p.connections.incoming) do
            if not incoming_map[ib] then incoming_map[ib] = {} end
            incoming_map[ib][#incoming_map[ib]+1] = {
               block = b.name,
               port  = p.name,
               type  = p.in_type_name,
               len   = p.in_data_len,
            }
         end
      end
   end

   local edges = {}
   local seen = {}
   for _, b in ipairs(cblocks) do
      for _, p in ipairs(b.ports) do
         for _, ib in ipairs(p.connections.outgoing) do
            local targets = incoming_map[ib] or {}
            -- Get buffer_len from iblock config
            local icfg = iblock_cfg(nd, ib)
            local buf_len = icfg.buffer_len or 1
            for _, tgt in ipairs(targets) do
               local eid = b.name..":"..p.name.."->"..ib.."->"..tgt.block..":"..tgt.port
               if not seen[eid] then
                  seen[eid] = true
                  edges[#edges+1] = {
                     id           = eid,
                     iblock       = ib,
                     source_block = b.name,
                     source_port  = p.name,
                     target_block = tgt.block,
                     target_port  = tgt.port,
                     type_name    = p.out_type_name or tgt.type or "",
                     data_len     = p.out_data_len or tgt.len or 1,
                     buffer_len   = buf_len,
                  }
               end
            end
         end
      end
   end

   -- Trigger edges from ptrig/trig chain0 configs
   local triggers = {}
   local trig_protos = { ["ubx/ptrig"]=true, ["ubx/trig"]=true }
   for _, b in ipairs(cblocks) do
      if b.prototype and trig_protos[b.prototype] then
         for _, c in ipairs(b.configs) do
            if c.name == "chain0" and type(c.value) == "table" then
               -- data_tolua returns a plain table when len==1; wrap it so ipairs works
               local chain = (c.value.b ~= nil) and { c.value } or c.value
               for step, entry in ipairs(chain) do
                  if type(entry) == "table" and type(entry.b) == "string" then
                     local tgt = entry.b:gsub("^#", "")
                     triggers[#triggers+1] = {
                        id        = b.name.."->trig->"..tgt.."@"..step,
                        from      = b.name,
                        to        = tgt,
                        step      = step,
                        num_steps = entry.num_steps,
                        every     = entry.every,
                     }
                  end
               end
            end
         end
      end
   end

   return json.encode({ nodes = nodes, edges = edges, triggers = triggers })
end

-- ============================================================
-- HTML / frontend
-- ============================================================

-- Inline the full single-page app; JS libs loaded from CDN.
-- The page polls /api/graph every 3 s and re-runs ELK layout.
local HTML = [[<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<title>ubx node graph</title>
<link rel="stylesheet" href="https://unpkg.com/@xyflow/react@12/dist/style.css">
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: monospace; background: #1a1a2e; color: #eee; }
#root { width: 100vw; height: 100vh; }

/* React Flow overrides */
.react-flow__node { font-family: monospace; }

/* Block node */
.ubx-block {
  background: #16213e;
  border: 2px solid #0f3460;
  border-radius: 6px;
  min-width: 200px;
  font-size: 11px;
}
.ubx-block.active    { border-color: #4ade80; }
.ubx-block.inactive  { border-color: #f87171; }
.ubx-block.preinit   { border-color: #60a5fa; }

.block-header {
  background: #0f3460;
  padding: 4px 8px;
  border-radius: 4px 4px 0 0;
}
.block-name  { font-weight: bold; font-size: 13px; color: #e2e8f0; }
.block-proto { display: block; color: #94a3b8; font-size: 10px; }
.block-state { display: block; color: #64748b; font-size: 10px; }
.block-steps { display: block; color: #64748b; font-size: 10px; }

.block-configs {
  padding: 3px 8px;
  border-bottom: 1px solid #1e3a5f;
}
.cfg-row { color: #94a3b8; font-size: 10px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; max-width: 190px; }
.cfg-key  { color: #7dd3fc; }

.block-ports { padding: 2px 0; overflow: visible; }
.port-row {
  position: relative;
  display: flex;
  align-items: center;
  padding: 1px 8px;
  min-height: 22px;
  overflow: visible;
}
.port-name { color: #e2e8f0; flex: 1; text-align: center; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; font-size: 11px; }

/* React Flow handles styled as direction boxes */
.react-flow__handle {
  width: 24px !important;
  height: 16px !important;
  border-radius: 2px !important;
  background: #0f2744 !important;
  border: 1px solid #7dd3fc !important;
  font-size: 9px !important;
  font-family: monospace !important;
  color: #7dd3fc !important;
  display: flex !important;
  align-items: center !important;
  justify-content: center !important;
  cursor: default !important;
}
.react-flow__handle-left  { left:  0 !important; }
.react-flow__handle-right { right: 0 !important; }

</style>
</head>
<body>
<div id="root"></div>

<script type="importmap">
{
  "imports": {
    "react":              "https://esm.sh/react@18",
    "react-dom":          "https://esm.sh/react-dom@18",
    "react-dom/client":   "https://esm.sh/react-dom@18/client",
    "react/jsx-runtime":  "https://esm.sh/react@18/jsx-runtime",
    "@xyflow/react":      "https://esm.sh/@xyflow/react@12?external=react,react-dom",
    "elkjs/lib/elk.bundled.js": "https://esm.sh/elkjs@0.9.3/lib/elk.bundled.js",
    "htm":                "https://esm.sh/htm@3"
  }
}
</script>

<script type="module" defer>
import { createElement, useState, useEffect, useCallback, useRef } from 'react';
import { createRoot } from 'react-dom/client';
import {
  ReactFlow, Background, Controls, MiniMap,
  useNodesState, useEdgesState,
  Handle, Position, ReactFlowProvider,
} from '@xyflow/react';
import ELK from 'elkjs/lib/elk.bundled.js';
import htm from 'htm';

const html = htm.bind(createElement);

// ---------- ELK instance ----------
const elk = new ELK();

const ELK_OPTS = {
  'elk.algorithm':                             'layered',
  'elk.direction':                             'RIGHT',
  'elk.spacing.nodeNode':                      '80',
  'elk.layered.spacing.nodeNodeBetweenLayers': '160',
};

// Approximate node height for ELK — exact value doesn't need to match rendering
const NODE_W   = 220;
const PORT_H   = 22;
const HEADER_H = 54;
const CFG_H    = 16;

function nodeHeight(b) {
  return HEADER_H + b.configs.length * CFG_H + b.ports.length * PORT_H + 8;
}

// ---------- Custom node ----------
function UbxBlock({ id, data }) {
  const { name, prototype, state, configs, ports, stat_num_steps } = data;

  const portLabel = (p) => {
    const parts = [];
    if (p.in_type)  parts.push('◄ ' + p.in_type  + (p.in_len  > 1 ? '['+p.in_len+']'  : ''));
    if (p.out_type) parts.push(p.out_type + (p.out_len > 1 ? '['+p.out_len+']' : '') + ' ►');
    return p.name + (parts.length ? '  ' + parts.join('  ') : '');
  };

  const trigSrc = id + ':_trig:out';
  const trigTgt = id + ':_trig:in';

  return html`
    <div className=${'ubx-block ' + state}>
      <${Handle} type="source" id=${trigSrc} position=${Position.Top}
        style=${{ opacity:0, pointerEvents:'none', width:1, height:1 }}/>
      <${Handle} type="target" id=${trigTgt} position=${Position.Bottom}
        style=${{ opacity:0, pointerEvents:'none', width:1, height:1 }}/>
      <div className="block-header">
        <span className="block-name">${name}</span>
        <span className="block-proto">${prototype}</span>
        <span className="block-state">${state}${stat_num_steps != null ? '  steps:' + stat_num_steps : ''}</span>
      </div>
      ${configs.length > 0 && html`
        <div className="block-configs">
          ${configs.map(c => html`
            <div key=${c.name} className="cfg-row">
              <span className="cfg-key">${c.name}</span>: ${c.value}
            </div>`)}
        </div>`}
      <div className="block-ports">
        ${ports.map(p => {
          const isIn  = !!p.in_type;
          const isOut = !!p.out_type;
          return html`
          <div key=${p.name} className="port-row">
            ${isIn  && html`<${Handle} type="target" id=${id+':'+p.name+':in'}
              position=${Position.Left}>${isOut ? '[<>]' : '[>]'}</${Handle}>`}
            <span className="port-name" title=${portLabel(p)}>${p.name}</span>
            ${isOut && html`<${Handle} type="source" id=${id+':'+p.name+':out'}
              position=${Position.Right}>${isIn ? '[<>]' : '[>]'}</${Handle}>`}
          </div>`;
        })}
      </div>
    </div>`;
}

const NODE_TYPES = { ubxBlock: UbxBlock };

// Shared label style for all edges (React Flow renders string labels in SVG)
const EDGE_LABEL_STYLE    = { fill: '#94a3b8', fontSize: 9, fontFamily: 'monospace' };
const EDGE_LABEL_BG_STYLE = { fill: 'rgba(15,20,40,0.9)', stroke: '#2a4a70', strokeWidth: 1 };

// ---------- Layout ----------
// Only called when graph topology changes. Trigger blocks are placed
// manually below the centroid of the blocks they trigger.
async function applyLayout(graphData) {
  const triggers = graphData.triggers || [];

  // Run ELK on data-flow blocks and edges only
  const elkNodes = graphData.nodes.map(b => ({
    id: b.id,
    width: NODE_W,
    height: nodeHeight(b),
  }));

  const elkEdges = graphData.edges.map(e => ({
    id: e.id,
    sources: [e.source_block],
    targets: [e.target_block],
  }));

  const layout = await elk.layout({
    id: 'root',
    layoutOptions: ELK_OPTS,
    children: elkNodes,
    edges: elkEdges,
  });

  // Place trigger blocks below the centroid of what they trigger.
  // Trigger blocks may chain (ptrig → trig → data-blocks), so process
  // them in topological order: leaf triggers (targeting only data-flow
  // blocks) first, then higher-level triggers whose targets include
  // already-placed trigger blocks.
  //
  // allPos tracks the final {x, y, width, height} for every block;
  // starts with ELK positions and is updated as trigger blocks are placed.
  const allPos = new Map();
  layout.children.forEach(n => allPos.set(n.id, { x: n.x, y: n.y, width: n.width, height: n.height }));

  const triggerBlockIds = new Set(triggers.map(t => t.from));
  const trigsByFrom = new Map();
  triggers.forEach(t => {
    if (!trigsByFrom.has(t.from)) trigsByFrom.set(t.from, []);
    trigsByFrom.get(t.from).push(t);
  });

  const placed = new Set();
  let changed = true;
  while (changed) {
    changed = false;
    triggerBlockIds.forEach(fromId => {
      if (placed.has(fromId)) return;
      // Can only place this block once all trigger blocks it depends on are placed
      const targets = (trigsByFrom.get(fromId) || []).map(t => t.to);
      if (targets.some(to => triggerBlockIds.has(to) && !placed.has(to))) return;

      let minX = Infinity, maxX = -Infinity, maxBottom = 0;
      targets.forEach(to => {
        const p = allPos.get(to);
        if (!p) return;
        minX      = Math.min(minX, p.x);
        maxX      = Math.max(maxX, p.x + p.width);
        maxBottom = Math.max(maxBottom, p.y + p.height);
      });
      if (minX !== Infinity) {
        const b = graphData.nodes.find(n => n.id === fromId);
        allPos.set(fromId, {
          x: (minX + maxX) / 2 - NODE_W / 2,
          y: maxBottom + 60,
          width:  NODE_W,
          height: nodeHeight(b),
        });
      }
      placed.add(fromId);
      changed = true;
    });
  }

  const nodes = graphData.nodes.map(b => {
    const p = allPos.get(b.id);
    return {
      id: b.id,
      type: 'ubxBlock',
      position: { x: p ? p.x : 0, y: p ? p.y : 0 },
      width:  NODE_W,
      height: nodeHeight(b),
      data: b,
    };
  });

  const dataEdges = graphData.edges.map(e => ({
    id: e.id,
    source: e.source_block,
    sourceHandle: e.source_block + ':' + e.source_port + ':out',
    target: e.target_block,
    targetHandle: e.target_block + ':' + e.target_port + ':in',
    label: e.type_name + (e.data_len > 1 ? '['+e.data_len+']' : '') + ' buf:' + e.buffer_len,
    labelStyle: EDGE_LABEL_STYLE,
    labelBgStyle: EDGE_LABEL_BG_STYLE,
    labelBgPadding: [3, 5],
    labelBgBorderRadius: 3,
    style: { stroke: '#7dd3fc' },
  }));

  const trigEdges = triggers.map(t => {
    const parts = ['#' + t.step];
    if (t.num_steps && t.num_steps > 1) parts.push('n:' + t.num_steps);
    if (t.every     && t.every     > 1) parts.push('e:' + t.every);
    return {
      id: t.id,
      source: t.from,
      sourceHandle: t.from + ':_trig:out',
      target: t.to,
      targetHandle: t.to + ':_trig:in',
      label: parts.join(' '),
      labelStyle: { ...EDGE_LABEL_STYLE, fill: '#fb923c' },
      labelBgStyle: EDGE_LABEL_BG_STYLE,
      labelBgPadding: [3, 5],
      labelBgBorderRadius: 3,
      style: { stroke: '#fb923c', strokeDasharray: '6 3' },
      markerEnd: { type: 'arrowclosed', color: '#fb923c' },
    };
  });

  return { nodes, edges: [...dataEdges, ...trigEdges] };
}

// Stable topology key: changes only when blocks/connections are added/removed
function topoKey(data) {
  return [
    ...data.nodes.map(n => n.id).sort(),
    '|',
    ...data.edges.map(e => e.id).sort(),
    '|',
    ...(data.triggers || []).map(t => t.id).sort(),
  ].join(',');
}

// ---------- App ----------
function App() {
  const [nodes, setNodes, onNodesChange] = useNodesState([]);
  const [edges, setEdges, onEdgesChange] = useEdgesState([]);
  const [status, setStatus] = useState('');
  const [autoRefresh, setAutoRefresh] = useState(false);
  const timerRef  = useRef(null);
  const topoRef   = useRef(null);  // last seen topology key

  const refresh = useCallback(async () => {
    try {
      const res = await fetch('/api/graph');
      if (!res.ok) throw new Error('HTTP ' + res.status);
      const data = await res.json();
      const key = topoKey(data);

      if (key !== topoRef.current) {
        // Topology changed: full re-layout, resets positions
        topoRef.current = key;
        const { nodes: n, edges: e } = await applyLayout(data);
        setNodes(n);
        setEdges(e);
      } else {
        // Same topology: update block data (state, configs) without moving nodes
        setNodes(prev => prev.map(node => {
          const b = data.nodes.find(n => n.id === node.id);
          return b ? { ...node, data: b } : node;
        }));
      }

      setStatus('blocks: ' + data.nodes.length + '  edges: ' + data.edges.length
                + '  updated: ' + new Date().toLocaleTimeString());
    } catch (err) {
      setStatus('error: ' + err.message);
    }
  }, []);

  // Load once on mount
  useEffect(() => { refresh(); }, []);

  // Auto-refresh interval — managed separately from the initial load
  useEffect(() => {
    if (!autoRefresh) return;
    timerRef.current = setInterval(refresh, 3000);
    return () => clearInterval(timerRef.current);
  }, [autoRefresh, refresh]);

  const btnStyle = (active) => ({
    background: active ? '#1e5a3f' : '#1e3a5f',
    color: active ? '#4ade80' : '#e2e8f0',
    border: 'none', cursor: 'pointer', padding: '2px 10px', borderRadius: 4,
  });

  return html`
    <div style=${{ width: '100vw', height: '100vh', display: 'flex', flexDirection: 'column' }}>
      <div style=${{ padding: '4px 12px', background: '#0f3460', color: '#94a3b8',
                     fontSize: 11, display: 'flex', alignItems: 'center',
                     justifyContent: 'space-between' }}>
        <span style=${{ color: '#7dd3fc', fontWeight: 'bold' }}>ubx node graph</span>
        <span>${status}</span>
        <div style=${{ display: 'flex', gap: '6px' }}>
          <button onClick=${refresh} style=${btnStyle(false)}>refresh</button>
          <button onClick=${() => setAutoRefresh(a => !a)} style=${btnStyle(autoRefresh)}>
            auto ${autoRefresh ? 'on' : 'off'}
          </button>
        </div>
      </div>
      <div style=${{ flex: 1 }}>
        <${ReactFlow}
          nodes=${nodes} edges=${edges}
          onNodesChange=${onNodesChange} onEdgesChange=${onEdgesChange}
          nodeTypes=${NODE_TYPES}
          fitView=${true}
          minZoom=${0.1}>
          <${Background} color="#0f3460" gap=${20}/>
          <${Controls}/>
          <${MiniMap} nodeColor=${() => '#0f3460'} maskColor="rgba(10,20,40,0.7)"/>
        </${ReactFlow}>
      </div>
    </div>`;
}

createRoot(document.getElementById('root')).render(
  html`<${ReactFlowProvider}><${App}/></${ReactFlowProvider}>`
);
</script>
</body>
</html>
]]

-- ============================================================
-- HTTP server (LuaSocket, single-threaded, non-blocking accept)
-- ============================================================

local function send_response(client, status, ctype, body)
   local hdr = table.concat({
      "HTTP/1.1 "..status,
      "Content-Type: "..ctype,
      "Content-Length: "..#body,
      "Cache-Control: no-cache",
      "Access-Control-Allow-Origin: *",
      "Connection: close",
      "", "",
   }, "\r\n")
   client:send(hdr)
   client:send(body)
end

local function handle_request(client, nd)
   client:settimeout(1)
   local req = client:receive("*l")
   if not req then return end

   -- drain headers
   while true do
      local ln = client:receive("*l")
      if not ln or ln == "" then break end
   end

   local path = req:match("^%u+ (/[^ ]*)")
   if not path then return end

   if path == "/" then
      send_response(client, "200 OK", "text/html; charset=utf-8", HTML)
   elseif path == "/api/graph" then
      local ok, result = pcall(build_graph, nd)
      if ok then
         send_response(client, "200 OK", "application/json", result)
      else
         send_response(client, "500 Internal Server Error", "text/plain", tostring(result))
      end
   else
      send_response(client, "404 Not Found", "text/plain", "not found")
   end
end

-- ============================================================
-- luablock lifecycle hooks
-- ============================================================

function init(block)
   if #_missing_deps > 0 then
      print("webgraph: missing dependencies: " .. table.concat(_missing_deps, ", "))
      return false
   end
   return true
end

function start(block)
   block = ffi.cast("ubx_block_t*", block)
   node_ref = block.nd

   -- Read 'port' config if set
   local pcfg = ubx.block_config_get(block, "port")
   local p = 8888
   if pcfg ~= nil then
      local v = ubx.data_tolua(pcfg.value)
      if type(v) == "number" and v > 0 then p = v end
   end

   local err
   srv, err = socket.bind("*", p)
   if not srv then
      print("webgraph: socket.bind failed: "..(err or "?"))
      return false
   end
   srv:settimeout(0)  -- non-blocking accept
   print("webgraph: listening on http://localhost:"..p)
   return true
end

function step(block)
   if not srv then return end
   node_ref = ffi.cast("ubx_block_t*", block).nd
   local client = srv:accept()
   if not client then return end  -- no pending connection
   local ok, err = pcall(handle_request, client, node_ref)
   if not ok then print("webgraph: handler error: "..tostring(err)) end
   client:close()
end

function stop(block)
   if srv then srv:close(); srv = nil end
   print("webgraph: stopped")
end

function cleanup(block) end
