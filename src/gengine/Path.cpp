/*
 * Copyright (C) 2004 Ivo Danihelka (ivo@danihelka.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "Path.h"

#include "OptionAgent.h"
#include "Log.h"

#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"

//-----------------------------------------------------------------
Path::Path(const boost::filesystem::path &file)
{
    m_path = file;
}
//-----------------------------------------------------------------
/**
 * Try return user data path,
 * otherwise return system data path.
 * NOTE: OptionAgent must be initialized before this
 *
 * @param file path to file
 * @param writeable whether we want write to the file
 * @return path to user or system file
 */
Path
Path::dataPath(const std::string &file, bool writeable)
{
    Path datapath = dataUserPath(file);

    const char *mode = "rb";
    if (writeable) {
        mode = "wb";
        LOG_DEBUG(ExInfo("creating path")
                .addInfo("path", datapath.getNative()));
        boost::filesystem::create_directories(
                datapath.m_path.branch_path());
    }

    FILE *try_open = fopen(datapath.getNative().c_str(), mode);
    if (NULL == try_open) {
        LOG_DEBUG(ExInfo("no user file")
                .addInfo("file", datapath.getNative()));
        datapath = dataSystemPath(file);
    }
    else {
        fclose(try_open);
    }

    return datapath;
}
//-----------------------------------------------------------------
Path
Path::dataReadPath(const std::string &file)
{
    return dataPath(file, false);
}
//-----------------------------------------------------------------
Path
Path::dataWritePath(const std::string &file)
{
    return dataPath(file, true);
}
//-----------------------------------------------------------------
/**
 * Return path to system file.
 * Path does not need to exist.
 */
Path
Path::dataSystemPath(const std::string &file)
{
    boost::filesystem::path datafile =
        OptionAgent::agent()->getParam("systemdir");
    datafile /= file;
    return Path(datafile);
}
//-----------------------------------------------------------------
/**
 * Return path to user file.
 * Path does not need to exist.
 */
Path
Path::dataUserPath(const std::string &file)
{
    boost::filesystem::path datafile =
        OptionAgent::agent()->getParam("userdir");
    datafile /= file;
    return Path(datafile);
}
//-----------------------------------------------------------------
std::string
Path::getNative() const
{
    return m_path.native_file_string();
}
//-----------------------------------------------------------------
std::string
Path::getBasename() const
{
    return m_path.leaf();
}

