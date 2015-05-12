-- Copyright 2015 Stanford University
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.

-- Legion Symbol Table

local log = require("legion/log")

local symbol_table = {}
symbol_table.__index = symbol_table

symbol_table.new_global_scope = function(lua_env)
  local st = setmetatable({}, symbol_table)
  st.lua_env = lua_env
  st.local_env = {}
  st.combined_env = setmetatable({}, {
      __index = function(_, index)
        local value = st.local_env[index] or st.lua_env[index]
        if not value then
          log.error("name '" .. tostring(index) .. "' is undefined or nil")
        end
        return value
      end,
      __newindex = function()
        log.error("cannot create global variables")
      end,
  })
  return st
end

function symbol_table:new_local_scope()
  local st = setmetatable({}, symbol_table)
  st.lua_env = self.lua_env
  st.local_env = setmetatable({}, { __index = self.local_env })
  st.combined_env = setmetatable({}, {
      __index = function(_, index)
        local value = st.local_env[index] or st.lua_env[index]
        if not value then
          log.error("name '" .. tostring(index) .. "' is undefined or nil")
        end
        return value
      end,
      __newindex = function()
        log.error("cannot create global variables")
      end,
  })
  return st
end

function symbol_table:lookup(index)
  return self.combined_env[index]
end

function symbol_table:insert(index, value)
  if rawget(self.local_env, index) then
    log.error("name '" .. tostring(index) .. "' is already defined")
  end
  rawset(self.local_env, index, value)
  return value
end

function symbol_table:env()
  return self.combined_env
end

return symbol_table
