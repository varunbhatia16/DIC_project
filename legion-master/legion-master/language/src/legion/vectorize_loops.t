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

-- Legion Loop Vectorizer
--
-- Attempts to vectorize the body of loops
--

local ast = require("legion/ast")
local log = require("legion/log")
local std = require("legion/std")

local min = math.min

-- vectorizer

local SIMD_REG_SIZE = 16

local V = {}
V.__index = V
function V:__tostring() return "vector" end
setmetatable(V, V)
local S = {}
S.__index = S
function S:__tostring() return "scalar" end
setmetatable(S, S)

local function join_fact(fact1, fact2)
  if fact1 == fact2 then return fact1 else return V end
end

local var_defs = {}
var_defs.__index = var_defs

local collect_undefined = {}

local flip_types = {}

function var_defs:new()
  local tbl = { __table = {} }
  setmetatable(tbl, self)
  return tbl
end

function var_defs:join(symbol, fact)
  assert(symbol.type)
  local fact_ = self.__table[symbol]
  local joined
  if fact_ then
    joined = join_fact(fact, fact_)
  else
    joined = fact
  end
  self.__table[symbol] = joined
end

function var_defs:get(symbol)
  return self.__table[symbol] or S
end

function var_defs:__tostring()
  local str = ""
  for symbol, fact in pairs(self.__table) do
    str = str .. tostring(symbol) .. " : " .. tostring(fact) .. "\n"
  end
  return str
end

function flip_types.block(simd_width, symbol, node)
  local stats = terralib.newlist()
  local function flip_type_each(stat)
    stats:insert(flip_types.stat(simd_width, symbol, stat))
  end
  node.stats:map(flip_type_each)
  return ast.typed.Block { stats = stats }
end

function flip_types.stat(simd_width, symbol, node)
  if node:is(ast.typed.StatBlock) then
    return ast.typed.StatBlock {
      block = flip_types.block(simd_width, symbol, node.block)
    }

  elseif node:is(ast.typed.StatVar) then
    local types = terralib.newlist()
    local values = terralib.newlist()

    node.types:map(function(ty)
      types:insert(flip_types.type(simd_width, ty))
    end)
    node.values:map(function(exp)
      values:insert(flip_types.exp(simd_width, symbol, exp))
    end)

    return ast.typed.StatVar {
      symbols = node.symbols,
      types = types,
      values = values,
      vectorize = true,
    }

  elseif node:is(ast.typed.StatAssignment) or
         node:is(ast.typed.StatReduce) then
    local lhs = terralib.newlist()
    local rhs = terralib.newlist()

    node.lhs:map(function(exp)
      lhs:insert(flip_types.exp(simd_width, symbol, exp))
    end)
    node.rhs:map(function(exp)
      rhs:insert(flip_types.exp(simd_width, symbol, exp))
    end)

    if node:is(ast.typed.StatAssignment) then
      return ast.typed.StatAssignment {
        lhs = lhs,
        rhs = rhs,
      }
    else -- node:is(ast.typed.StatReduce)
      return ast.typed.StatReduce {
        lhs = lhs,
        rhs = rhs,
        op = node.op,
      }
    end

  elseif node:is(ast.typed.StatForNum) then
    return ast.typed.StatForNum {
      symbol = node.symbol,
      values = node.values,
      block = flip_types.block(simd_width, symbol, node.block),
      parallel = node.parallel,
    }

  else
    assert(false, "unexpected node type " .. tostring(node:type()))
  end

  return res
end

function flip_types.exp(simd_width, symbol, node)
  local new_node = {}
  for k, v in pairs(node) do
    new_node[k] = v
  end
  if node:is(ast.typed.ExprFieldAccess) then
    new_node.value = flip_types.exp(simd_width, symbol, node.value)

  elseif node:is(ast.typed.ExprIndexAccess) then
    new_node.value = flip_types.exp(simd_width, symbol, node.value)

  elseif node:is(ast.typed.ExprBinary) then
    new_node.lhs = flip_types.exp(simd_width, symbol, new_node.lhs)
    new_node.rhs = flip_types.exp(simd_width, symbol, new_node.rhs)

    local expr_type
    if new_node.lhs.expr_type:isvector() then
      expr_type = new_node.lhs.expr_type
    else
      expr_type = new_node.rhs.expr_type
    end

    if (node.op == "min" or node.op == "max") and
       std.is_minmax_supported(expr_type) then
      local args = terralib.newlist()
      args:insert(new_node.lhs)
      args:insert(new_node.rhs)
      local fn = std["v" .. node.op](expr_type)
      local fn_type = ({expr_type, expr_type} -> expr_type).type
      local fn_node = ast.typed.ExprFunction {
        expr_type = fn_type,
        value = fn,
      }
      return ast.typed.ExprCall {
        fn = fn_node,
        args = args,
        expr_type = expr_type,
      }
    end

  elseif node:is(ast.typed.ExprUnary) then
    new_node.rhs = flip_types.exp(simd_width, symbol, new_node.rhs)

  elseif node:is(ast.typed.ExprCtor) then
    new_node.fields = node.fields:map(
      function(field)
        return flip_types.exp(simd_width, symbol, field)
      end)

  elseif node:is(ast.typed.ExprCtorRecField) then
    new_node.value = flip_types.exp(simd_width, symbol, node.value)

  elseif node:is(ast.typed.ExprCall) then
    new_node.args = node.args:map(
      function(arg)
        return flip_types.exp(simd_width, symbol, arg)
      end)
    new_node.fn = flip_types.exp_function(simd_width, node.fn)

  elseif node:is(ast.typed.ExprCast) then
    new_node.arg = flip_types.exp(simd_width, symbol, node.arg)
    new_node.fn = ast.typed.ExprFunction {
      expr_type = flip_types.type(simd_width, node.fn.expr_type),
      value = flip_types.type(simd_width, node.fn.value),
    }

  elseif node:is(ast.typed.ExprID) then
    new_node.value.type = new_node.expr_type

  elseif node:is(ast.typed.ExprConstant) then

  else
    assert(false, "unexpected node type " .. tostring(node:type()))

  end
  if node.fact == V and
    not (node:is(ast.typed.ExprID) and node.value == symbol) then
    new_node.expr_type = flip_types.type(simd_width, new_node.expr_type)
  end
  return node:type()(new_node)
end

function flip_types.exp_function(simd_width, node)
  local elmt_type = node.expr_type.returntype
  local vector_type = vector(elmt_type, simd_width)

  local value = std.convert_math_op(node.value, vector_type)
  local expr_type = flip_types.type(simd_width, node.expr_type)

  return ast.typed.ExprFunction {
    expr_type = expr_type,
    value = value,
  }
end

function flip_types.type(simd_width, ty)
  if ty:isprimitive() then
    return vector(ty, simd_width)
  elseif ty:isarray() then
    return (vector(ty.type, simd_width))[ty.N]
  elseif std.is_ptr(ty) then
    return std.vptr(simd_width,
                    ty.points_to_type,
                    unpack(ty.points_to_region_symbols))
  elseif ty:isstruct() then
    return std.sov(ty, simd_width)
  elseif ty:isfunction() then
    local params = ty.parameters:map(
      function(ty)
        flip_types.type(simd_width, ty)
      end)
    local returntype = flip_types.type(simd_width, ty.returntype)
    return (params -> returntype).type
  else
    assert(false, "unexpected type " .. tostring(ty))
  end
end

local min_simd_width = {}

function min_simd_width.block(reg_size, node)
  local simd_width = reg_size
  node.stats:map(function(stat)
    local simd_width_ = min_simd_width.stat(reg_size, stat)
    simd_width = math.min(simd_width, simd_width_)
  end)
  return simd_width
end

function min_simd_width.stat(reg_size, node)
  if node:is(ast.typed.StatBlock) then
    return min_simd_width.block(reg_size, node.block)

  elseif node:is(ast.typed.StatVar) then
    local simd_width = reg_size
    node.types:map(function(type)
      simd_width = min(simd_width, min_simd_width.type(reg_size, type))
    end)
    node.values:map(function(value)
      simd_width = min(simd_width, min_simd_width.exp(reg_size, value))
    end)
    return simd_width

  elseif node:is(ast.typed.StatAssignment) or
         node:is(ast.typed.StatReduce) then
    local simd_width = reg_size
    node.lhs:map(function(lh)
      simd_width = min(simd_width, min_simd_width.exp(reg_size, lh))
    end)
    node.rhs:map(function(rh)
      simd_width = min(simd_width, min_simd_width.exp(reg_size, rh))
    end)
    return simd_width

  elseif node:is(ast.typed.StatForNum) then
    return min_simd_width.block(reg_size, node.block)

  else
    assert(false, "unexpected node type " .. tostring(node:type()))
  end
end

function min_simd_width.exp(reg_size, node)
  local simd_width = reg_size
  if node.fact == V then
    simd_width = min_simd_width.type(reg_size, node.expr_type)
  end

  if node:is(ast.typed.ExprID) then

  elseif node:is(ast.typed.ExprFieldAccess) then
    if not node.value:is(ast.typed.ExprID) then
      simd_width = min(simd_width, min_simd_width.exp(reg_size, node.value))
    end

  elseif node:is(ast.typed.ExprIndexAccess) then
    simd_width = min(simd_width, min_simd_width.exp(reg_size, node.value))

  elseif node:is(ast.typed.ExprUnary) then
    simd_width = min_simd_width.exp(reg_size, node.rhs)

  elseif node:is(ast.typed.ExprBinary) then
    simd_width = min(min_simd_width.exp(reg_size, node.lhs),
                     min_simd_width.exp(reg_size, node.rhs))

  elseif node:is(ast.typed.ExprCtor) then
    for _, field in pairs(node.fields) do
      simd_width = min(simd_width, min_simd_width.exp(reg_size,field))
    end

  elseif node:is(ast.typed.ExprCtorRecField) then
    simd_width = min_simd_width.exp(reg_size, node.value)

  elseif node:is(ast.typed.ExprConstant) then

  elseif node:is(ast.typed.ExprCall) then
    for _, arg in pairs(node.args) do
      simd_width = min(simd_width, min_simd_width.exp(reg_size, arg))
    end

  elseif node:is(ast.typed.ExprCast) then
    simd_width = min(simd_width, min_simd_width.exp(reg_size, node.arg))

  else
    assert(false, "unexpected node type " .. tostring(node:type()))

  end

  return simd_width
end

function min_simd_width.type(reg_size, ty)
  if std.is_ptr(ty) then
    return reg_size / sizeof(uint32)
  elseif ty:isarray() then
    return reg_size / sizeof(ty.type)
  elseif ty:isstruct() then
    local simd_width = reg_size
    for _, entry in pairs(ty.entries) do
      local entry_type = entry[2] or entry.type
      simd_width = min(simd_width, min_simd_width.type(reg_size, entry_type))
    end
    return simd_width
  else
    return reg_size / sizeof(ty)
  end
end

local vectorize = {}
function vectorize.stat_for_list(node)
  local simd_width = min_simd_width.block(SIMD_REG_SIZE, node.block)
  assert(simd_width >= 1)
  local body = flip_types.block(simd_width, node.symbol, node.block)
  return ast.typed.StatForListVectorized {
    symbol = node.symbol,
    value = node.value,
    block = body,
    orig_block = node.block,
    vector_width = simd_width,
  }
end

-- vectorizability check. each ast node is annotated by a 'vectorizable' field
-- that denotes whether the node can be vectorized.

local annotate_vectorizability = {}
local error_prefix = "loop vectorization failed: loop body has "

function annotate_vectorizability.block(vars, node)
  node.vectorizable = true
  for i, stat in ipairs(node.stats) do
    if node.vectorizable then
      annotate_vectorizability.stat(vars, stat)
      node.vectorizable = stat.vectorizable
      if not node.vectorizable then
        node.error_msg = stat.error_msg
      end
    else break
    end
  end
end

function annotate_vectorizability.stat(vars, node)
  node.vectorizable = true

  if node:is(ast.typed.StatBlock) then
    annotate_vectorizability.block(vars, node.block)
    node.vectorizable = node.block.vectorizable
    node.error_msg = node.block.error_msg

  elseif node:is(ast.typed.StatVar) then
    for i, symbol in pairs(node.symbols) do
      if not node.vectorizable then break end

      local ty = node.types[i]
      if not (ty:isarray() or annotate_vectorizability.type(ty)) then
        node.vectorizable = false
        node.error_msg = error_prefix .. "a non-primitive variable declaration"
        break
      end

      if #node.values > 0 then
        local value = node.values[i]
        annotate_vectorizability.exp(vars, value)
        node.vectorizable = value.vectorizable
        if not node.vectorizable then
          node.error_msg = value.error_msg
        else
          vars:join(symbol, V)
        end
      else
        vars:join(symbol, V)
      end
    end

  elseif node:is(ast.typed.StatAssignment) or
         node:is(ast.typed.StatReduce) then
    for i, rh in pairs(node.rhs) do
      local lh = node.lhs[i]
      if not (annotate_vectorizability.type(lh.expr_type) and
              annotate_vectorizability.type(rh.expr_type)) then
        node.vectorizable = false
        node.error_msg = error_prefix ..
          "an assignment between non-scalar expressions"
        break
      end
      if node.vectorizable then
        annotate_vectorizability.exp(vars, lh)
        annotate_vectorizability.exp(vars, rh)
        node.vectorizable = lh.vectorizable and rh.vectorizable
        if not node.vectorizable then
          node.error_msg = rawget(lh, "error_msg") or rawget(rh, "error_msg")
        end
        if lh.fact == S and rh.fact == V then
          node.vectorizable = false
          node.error_msg = error_prefix ..
            "an assignment of a non-scalar expression to a scalar expression"
          break
        end
      else break
      end
    end

  elseif node:is(ast.typed.StatForNum) then
    for _, value in pairs(node.values) do
      annotate_vectorizability.exp(vars, value)
      node.vectorizable = value.vectorizable
      if not node.vectorizable then
        node.error_msg = value.error_msg
        break
      end
      if value.fact ~= S then
        node.vectorizable = false
        node.error_msg = error_prefix ..  "a non-scalar loop condition"
        break
      end
      vars:join(node.symbol, S)
      annotate_vectorizability.block(vars, node.block)
    end

  else
    node.vectorizable = false

    if node:is(ast.typed.StatIf) then
      node.error_msg = error_prefix .. "an if statement"

    elseif node:is(ast.typed.StatWhile) then
      node.error_msg = error_prefix .. "an inner loop"

    elseif node:is(ast.typed.StatForList) then
      node.error_msg = error_prefix .. "an inner loop"

    elseif node:is(ast.typed.StatRepeat) then
      node.error_msg = error_prefix .. "an inner loop"

    elseif node:is(ast.typed.StatVarUnpack) then
      node.error_msg = error_prefix .. "an unpack statement"

    elseif node:is(ast.typed.StatReturn) then
      node.error_msg = error_prefix .. "a return statement"

    elseif node:is(ast.typed.StatBreak) then
      node.error_msg = error_prefix .. "a break statement"

    --elseif node:is(ast.typed.StatReduce) then
    --  node.error_msg = error_prefix .. "a reduce statement"

    elseif node:is(ast.typed.StatExpr) then
      node.error_msg = error_prefix .. "an expression as a statement"

    else
      assert(false, "unexpected node type " .. tostring(node:type()))
    end
  end
end

-- function 'annotate_vectorizability.exp' additionally annotates
-- an expression node with a dataflow fact
function annotate_vectorizability.exp(vars, node)
  node.vectorizable = true

  if node:is(ast.typed.ExprID) then
    vars:join(node.value, vars:get(node.value))
    node.fact = vars:get(node.value)

  elseif node:is(ast.typed.ExprFieldAccess) then
    annotate_vectorizability.exp(vars, node.value)
    node.vectorizable = node.value.vectorizable
    if not node.vectorizable then
      node.error_msg = node.value.error_msg
    else
      node.fact = node.value.fact
    end

  elseif node:is(ast.typed.ExprIndexAccess) then
    if not node.value:is(ast.typed.ExprID) then
      node.vectorizable = false
      node.error_msg = "loop vectorization failed: array access should be in a form of 'a[exp]'"
      return
    end

    annotate_vectorizability.exp(vars, node.value)
    annotate_vectorizability.exp(vars, node.index)

    if node.index.fact ~= S then
      node.vectorizable = false
      node.error_msg = error_prefix + "an array access with a non-scalar index"
      return
    end

    node.vectorizable = node.value.vectorizable and node.index.vectorizable
    if not node.vectorizable then
      node.error_msg =
        rawget(node.value, "error_msg") or rawget(node.index, "error_msg")
    else
      node.fact = join_fact(node.value.fact, node.index.fact)
    end

  elseif node:is(ast.typed.ExprUnary) then
    annotate_vectorizability.exp(vars, node.rhs)
    node.vectorizable = node.rhs.vectorizable
    if not node.vectorizable then
      node.error_msg = node.rhs.error_msg
    else
      node.fact = node.rhs.fact
    end

  elseif node:is(ast.typed.ExprBinary) then
    if not annotate_vectorizability.binary_op(node.op, node.expr_type) then
      node.vectorizable = false
      node.error_msg = error_prefix .. "an unsupported binary operator"
      return
    end

    annotate_vectorizability.exp(vars, node.lhs)
    annotate_vectorizability.exp(vars, node.rhs)

    node.vectorizable = node.lhs.vectorizable and node.rhs.vectorizable
    if not node.vectorizable then
      node.error_msg =
        rawget(node.lhs, "error_msg") or
        rawget(node.rhs, "error_msg")
    else
      node.fact = join_fact(node.lhs.fact, node.rhs.fact)
    end

  elseif node:is(ast.typed.ExprCtor) then
    node.fact = S
    for _, field in pairs(node.fields) do
      annotate_vectorizability.exp(vars, field)
      node.vectorizable = field.vectorizable
      if not node.vectorizable then
        node.error_msg = field.error_msg
        break
      else
        node.fact = join_fact(node.fact, field.fact)
      end
    end

  elseif node:is(ast.typed.ExprCtorRecField) then
    annotate_vectorizability.exp(vars, node.value)
    node.vectorizable = node.value.vectorizable
    if not node.vectorizable then
      node.error_msg = node.value.error_msg
    else
      node.fact = node.value.fact
      -- copy the expr_type field from its child to
      -- make it consistent with other nodes
      node.expr_type = node.value.expr_type
    end

  elseif node:is(ast.typed.ExprConstant) then
    node.fact = S

  elseif node:is(ast.typed.ExprCall) then
    annotate_vectorizability.exp_call(vars, node)

  elseif node:is(ast.typed.ExprCast) then
    if not (node.expr_type:isprimitive()) then
      node.vectorizable = false
      node.error_msg = error_prefix .. "an unsupported casting expression"
      return
    else
      annotate_vectorizability.exp(vars, node.arg)
      node.fact = node.arg.fact
    end

  else
    node.vectorizable = false
    if node:is(ast.typed.ExprMethodCall) then
      node.error_msg = error_prefix .. "a method call"

    --elseif node:is(ast.typed.ExprCast) then
    --  node.error_msg = error_prefix .. "a cast"

    --elseif node:is(ast.typed.ExprCtor) then
    --  node.error_msg = error_prefix .. "a constructor"

    elseif node:is(ast.typed.ExprCtorListField) then
      node.error_msg = error_prefix .. "a list constructor"

    --elseif node:is(ast.typed.ExprCtorRecField) then
    --  node.error_msg = error_prefix .. "a record constructor"

    elseif node:is(ast.typed.ExprRawContext) then
      node.error_msg = error_prefix .. "a raw expression"

    elseif node:is(ast.typed.ExprRawFields) then
      node.error_msg = error_prefix .. "a raw expression"

    elseif node:is(ast.typed.ExprRawPhysical) then
      node.error_msg = error_prefix .. "a raw expression"

    elseif node:is(ast.typed.ExprRawRuntime) then
      node.error_msg = error_prefix .. "a raw expression"

    elseif node:is(ast.typed.ExprIsnull) then
      node.error_msg = error_prefix .. "an isnull expression"

    elseif node:is(ast.typed.ExprNew) then
      node.error_msg = error_prefix .. "a new expression"

    elseif node:is(ast.typed.ExprNull) then
      node.error_msg = error_prefix .. "a null expression"

    elseif node:is(ast.typed.ExprDynamicCast) then
      node.error_msg = error_prefix .. "a dynamic cast"

    elseif node:is(ast.typed.ExprStaticCast) then
      node.error_msg = error_prefix .. "a static cast"

    elseif node:is(ast.typed.ExprRegion) then
      node.error_msg = error_prefix .. "a region expression"

    elseif node:is(ast.typed.ExprPartition) then
      node.error_msg = error_prefix .. "a patition expression"

    elseif node:is(ast.typed.ExprCrossProduct) then
      node.error_msg = error_prefix .. "a cross product operation"

    elseif node:is(ast.typed.ExprFunction) then
      node.error_msg = error_prefix .. "a function reference"

    elseif node:is(ast.typed.ExprDeref) then
      node.error_msg = error_prefix .. "a pointer dereference"

    else
      assert(false, "unexpected node type " .. tostring(node:type()))
    end
  end
end

local predefined_functions = {}

function annotate_vectorizability.exp_call(vars, node)
  assert(node:is(ast.typed.ExprCall))
  node.vectorizable = false
  node.error_msg = error_prefix .. "an unsupported function call"

  if std.is_math_op(node.fn.value) then
    node.fact = S
    for _, arg in pairs(node.args) do
      annotate_vectorizability.exp(vars, arg)
      if not arg.vectorizable then
        node.error_msg = arg.error_msg
        node.fact = nil
        return
      else
        node.fact = join_fact(node.fact, arg.fact)
      end
    end
    node.vectorizable = true
    node.error_msg = nil
  end
end

function annotate_vectorizability.binary_op(op, arg_type)
  if op == "max" or op == "min" then
    if not (arg_type == float or arg_type == double) then
      return false
    else
      return true
    end
  end
  return true
end

-- check if the type is admissible to vectorizer.
-- this check might be too restrictive though...
-- need to be more careful about what the vectorizer can support
function annotate_vectorizability.type(ty)
  if ty:isprimitive() or std.is_ptr(ty) then
    return true
  elseif ty:isstruct() then
    for _, entry in pairs(ty.entries) do
      local entry_type = entry[2] or entry.type
      if not entry_type:isprimitive() then
        return false
      end
    end
    return true
  else
    return false
  end
end

-- visitor for each statement type
local vectorize_loops = {}

function vectorize_loops.block(node)
  return ast.typed.Block {
    stats = node.stats:map(
      function(stat) return vectorize_loops.stat(stat) end),
  }
end

function vectorize_loops.stat_if(node)
  return ast.typed.StatIf {
    cond = node.cond,
    then_block = vectorize_loops.block(node.then_block),
    elseif_blocks = node.elseif_blocks:map(
      function(block) return vectorize_loops.stat_elseif(block) end),
    else_block = vectorize_loops.block(node.else_block),
  }
end

function vectorize_loops.stat_elseif(node)
  return ast.typed.StatElseif {
    cond = node.cond,
    block = vectorize_loops.block(node.block),
  }
end

function vectorize_loops.stat_while(node)
  return ast.typed.StatWhile {
    cond = node.cond,
    block = vectorize_loops.block(node.block),
  }
end

function vectorize_loops.stat_for_num(node)
  return ast.typed.StatForNum {
    symbol = node.symbol,
    values = node.values,
    block = vectorize_loops.block(node.block),
    parallel = node.parallel,
  }
end

function vectorize_loops.stat_for_list(node)
  local function mk_stat_for_list(body)
    return ast.typed.StatForList {
      symbol = node.symbol,
      value = node.value,
      block = body,
      vectorize = node.vectorize,
    }
  end

  if node.vectorize ~= "demand" then
    return mk_stat_for_list(vectorize_loops.block(node.block))
  end

  local body = node.block
  local vars = var_defs:new()
  vars:join(node.symbol, V)
  -- annotate each ast node with its vectorizability
  annotate_vectorizability.block(vars, body)
  if body.vectorizable then
    return vectorize.stat_for_list(node)
  else
    log.warn(body.error_msg)
    return mk_stat_for_list(body)
  end
end

function vectorize_loops.stat_repeat(node)
  return ast.typed.StatRepeat {
    block = vectorize_loops.block(node.block),
    until_cond = node.until_cond,
  }
end

function vectorize_loops.stat_block(node)
  return ast.typed.StatBlock {
    block = vectorize_loops.block(node.block)
  }
end

function vectorize_loops.stat(node)
  if node:is(ast.typed.StatIf) then
    return vectorize_loops.stat_if(node)

  elseif node:is(ast.typed.StatWhile) then
    return vectorize_loops.stat_while(node)

  elseif node:is(ast.typed.StatForNum) then
    return vectorize_loops.stat_for_num(node)

  elseif node:is(ast.typed.StatForList) then
    return vectorize_loops.stat_for_list(node)

  elseif node:is(ast.typed.StatRepeat) then
    return vectorize_loops.stat_repeat(node)

  elseif node:is(ast.typed.StatBlock) then
    return vectorize_loops.stat_block(node)

  elseif node:is(ast.typed.StatIndexLaunch) then
    return node

  elseif node:is(ast.typed.StatVar) then
    return node

  elseif node:is(ast.typed.StatVarUnpack) then
    return node

  elseif node:is(ast.typed.StatReturn) then
    return node

  elseif node:is(ast.typed.StatBreak) then
    return node

  elseif node:is(ast.typed.StatAssignment) then
    return node

  elseif node:is(ast.typed.StatReduce) then
    return node

  elseif node:is(ast.typed.StatExpr) then
    return node

  elseif node:is(ast.typed.StatMapRegions) then
    return node

  elseif node:is(ast.typed.StatUnmapRegions) then
    return node

  else
    assert(false, "unexpected node type " .. tostring(node:type()))
  end
end

function vectorize_loops.stat_task(node)
  local body = vectorize_loops.block(node.body)

  return ast.typed.StatTask {
    name = node.name,
    params = node.params,
    return_type = node.return_type,
    privileges = node.privileges,
    constraints = node.constraints,
    body = body,
    config_options = node.config_options,
    prototype = node.prototype,
  }
end

function vectorize_loops.stat_top(node)
  if node:is(ast.typed.StatTask) then
    return vectorize_loops.stat_task(node)
  else
    return node
  end
end

function vectorize_loops.entry(node)
  return vectorize_loops.stat_top(node)
end

return vectorize_loops
