/*
 * Copyright (C) 2004 Ivo Danihelka (ivo@danihelka.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "Environ.h"

#include "Log.h"
#include "BaseMsg.h"
#include "NameException.h"
#include "StringTool.h"

#include <stdio.h>

//-----------------------------------------------------------------
/**
 * Free all remain messages for watchers.
 */
Environ::~Environ()
{
    t_watchers::iterator end = m_watchers.end();
    for (t_watchers::iterator i = m_watchers.begin(); i != end; ++i) {
        delete i->second;
    }
}
//-----------------------------------------------------------------
/**
 * Save params.
 * @param file where to store params, this file will be overwritten
 */
    void
Environ::store(const Path &file)
{
    FILE *config = fopen(file.getNative().c_str(), "w");
    if (config) {
        fputs("-- this file is automatically generated\n", config);

        t_values::iterator end = m_values.end();
        for (t_values::iterator i = m_values.begin(); i != end; ++i) {
            fprintf(config, "setParam(\"%s\", \"%s\")\n",
                    i->first.c_str(), i->second.c_str());
        }

        fclose(config);
    }
    else {
        LOG_WARNING(ExInfo("cannot save config")
                .addInfo("file", file.getNative()));
    }
}
//-----------------------------------------------------------------
/**
 * Set param.
 * Notice watchers.
 * When watcher is not available, it will be removed.
 *
 * @param name param name
 * @param value param value
 */
    void
Environ::setParam(const std::string &name, const std::string &value)
{
    LOG_DEBUG(ExInfo("setParam")
            .addInfo("param", name)
            .addInfo("value", value));
    if (m_values[name] != value) {
        m_values[name] = value;

        t_watchers::iterator it = m_watchers.find(name);
        if (m_watchers.end() != it) {
            t_watchers::size_type count = m_watchers.count(name);
            for (t_watchers::size_type i = 0; i < count; ++i) {
                t_watchers::iterator cur_it = it++;
                try {
                    cur_it->second->sendClone();
                }
                catch (NameException &e) {
                    LOG_WARNING(e.info());
                    delete cur_it->second;
                    m_watchers.erase(cur_it);
                }
            }
        }
    }
}
//-----------------------------------------------------------------
/**
 * Store this integer value like string param.
 * @param name param name
 * @param value param value
 */
    void
Environ::setParam(const std::string &name, long value)
{
    setParam(name, StringTool::toString(value));
}
//-----------------------------------------------------------------
/**
 * Return value.
 * Implicit value is "".
 *
 * @param name param name
 * @param implicit default value = ""
 * @return value or implicit value
 */
    std::string
Environ::getParam(const std::string &name,
                const std::string &implicit)
{
    std::string result = implicit;

    t_values::iterator it = m_values.find(name);
    if (m_values.end() != it) {
        result = it->second;
    }
    return result;
}
//-----------------------------------------------------------------
/**
 * Return number value.
 * Implicit value is zero.
 *
 * @param name param name
 * @param implicit default value = 0
 * @return number or implicit
 */
    int
Environ::getAsInt(const std::string &name,
                int implicit)
{
    std::string value = getParam(name);
    bool ok;
    int result = StringTool::readInt(value.c_str(), &ok);
    if (!ok) {
        result = implicit;
    }
    return result;
}
//-----------------------------------------------------------------
/**
 * Multiple watcher can watch param change.
 * @param name param name
 * @param msg message to raise
 */
    void
Environ::addWatcher(const std::string &name, BaseMsg *msg)
{
    m_watchers.insert(std::pair<std::string,BaseMsg*>(name, msg));
    LOG_DEBUG(ExInfo("add watcher")
            .addInfo("param", name)
            .addInfo("msg", msg->toString()));
}
//-----------------------------------------------------------------
std::string
Environ::toString() const
{
    ExInfo info("environ");
    t_values::const_iterator end = m_values.end();
    for (t_values::const_iterator i = m_values.begin(); i != end; ++i) {
        info.addInfo(i->first, i->second);
    }
    return info.info();
}


