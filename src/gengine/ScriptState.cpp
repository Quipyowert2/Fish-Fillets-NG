/*
 * Copyright (C) 2004 Ivo Danihelka (ivo@danihelka.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "ScriptState.h"

#include "Log.h"
#include "ExInfo.h"
#include "ScriptException.h"
#include "Path.h"

extern "C" {
#include "lualib.h"
#include "lauxlib.h"
}

#include "def-script.h"

//-----------------------------------------------------------------
ScriptState::ScriptState()
{
    m_state = lua_open();
    luaopen_base(m_state);
    luaopen_string(m_state);
    luaopen_math(m_state);
    luaopen_table(m_state);
}
//-----------------------------------------------------------------
ScriptState::~ScriptState()
{
    lua_close(m_state);
}
//-----------------------------------------------------------------
/**
 * Process script on stack.
 * @param error script load status
 * @param params number of params
 * @param returns number of values on return
 *
 * @throws ScriptException when script is bad
 */
    void
ScriptState::callStack(int error, int params, int returns)
{
    if (0 == error) {
        error = lua_pcall(m_state, params, returns, 0);
    }

    if (error) {
        const char *msg = lua_tostring(m_state, -1);
        if (NULL == msg) {
            msg = "(error with no message)";
        }
        lua_pop(m_state, 1);
        throw ScriptException(ExInfo("script failure")
                .addInfo("error", msg));
    }
}
//-----------------------------------------------------------------
/**
 * Process script file.
 * @param file script
 */
    void
ScriptState::doFile(const Path &file)
{
    int error = luaL_loadfile(m_state, file.getNative().c_str());
    callStack(error);
}
//-----------------------------------------------------------------
/**
 * Process string.
 * @param input script
 */
    void
ScriptState::doString(const std::string &input)
{
    int error = luaL_loadbuffer(m_state, input.c_str(), input.size(),
            input.c_str());
    callStack(error);
}
//-----------------------------------------------------------------
    void
ScriptState::registerFunc(const char *name, lua_CFunction func)
{
    lua_register(m_state, name, func);
}

//-----------------------------------------------------------------
/**
 * Call "bool function(param)" function.
 * @param funcRef function index at registry
 * @param param integer parametr
 * @return boolean result from function
 * @throws ScriptException when function is bad
 */
bool
ScriptState::callCommand(int funcRef, int param)
{
    lua_rawgeti(m_state, LUA_REGISTRYINDEX, funcRef);
    lua_pushnumber(m_state, param);
    callStack(0, 1, 1);

    if (0 == lua_isboolean(m_state, -1)) {
        const char *type = lua_typename(m_state, lua_type(m_state, -1));
        lua_pop(m_state, 1);
        throw ScriptException(ExInfo("script failure - boolean excepted")
                .addInfo("got", type));
    }
    return lua_toboolean(m_state, -1);
}
//-----------------------------------------------------------------
/**
 * Remove function from registry.
 * @param funcRef function index at registry
 */
void
ScriptState::unref(int funcRef)
{
    luaL_unref(m_state, LUA_REGISTRYINDEX, funcRef);
}

//-----------------------------------------------------------------
/**
 * Register light userdata for lua script.
 */
void
ScriptState::registerLeader(void *leader)
{
    lua_pushstring(m_state, script_getLeaderName());
    lua_pushlightuserdata(m_state, leader);
    lua_rawset(m_state, LUA_REGISTRYINDEX);
}

