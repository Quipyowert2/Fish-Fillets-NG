#ifndef HEADER_RESOURCEPACK_H
#define HEADER_RESOURCEPACK_H

#include "Log.h"
#include "Random.h"
#include "INamed.h"
#include "ResourceException.h"

#include <string>
#include <vector>
#include <map>

/**
 * Share resources.
 */
template <class T>
class ResourcePack : public INamed {
    public:
    typedef std::vector<T> t_range;
    protected:
    typedef std::multimap<std::string,T> t_reses;
    typedef typename t_reses::iterator t_resIterator;
    t_reses m_reses;
    virtual void unloadRes(T res) = 0;

    public:
    //NOTE: we cannot call virtual functions from desctructor,
    // call removeAll before delete
    virtual ~ResourcePack() {}
    //-----------------------------------------------------------------
    /**
     * Free all resources.
     * NOTE: we cannot call virtual functions from desctructor
     */
    void removeAll()
        {
            t_resIterator end = m_reses.end();
            for (t_resIterator item = m_reses.begin(); item != end; ++item) {
                unloadRes(item->second);
            }
            m_reses.clear();
        }
    //-----------------------------------------------------------------
    /**
     * Unload all resources with this name.
     */
    void removeRes(const std::string &name)
        {
            std::pair<t_resIterator, t_resIterator> range =
                m_reses.equal_range(name);
            while (range.first != range.second) {
                unloadRes(range.first->second);
                ++(range.first);
            }
            m_reses.erase(name);
            LOG_DEBUG(ExInfo("removed resources")
                    .addInfo("name", name));
        }

    //-----------------------------------------------------------------
    /**
     * Store resource under this name.
     */
    void addRes(const std::string &name, T res)
    {
        m_reses.insert(
                std::pair<std::string,T>(name, res));
    }
    //-----------------------------------------------------------------
    /**
     * Get resource with this name.
     */
    T getRes(const std::string &name, int rank=0)
    {
        std::pair<t_resIterator, t_resIterator> range =
            m_reses.equal_range(name);
        for (int i = 0; i < rank && range.first != range.second; ++i) {
            ++(range.first);
        }
        if (range.second == range.first) {
            throw ResourceException(ExInfo("no such resource at index")
                    .addInfo("name", name)
                    .addInfo("index", rank)
                    .addInfo("pack", toString()));
        }
        return range.first->second;
    }
    //-----------------------------------------------------------------
    /**
     * Get all resources with this name.
     * NOTE: range can be empty.
     */
    t_range getRange(const std::string &name)
    {
        t_range result;
        std::pair<t_resIterator, t_resIterator> range =
            m_reses.equal_range(name);
        while (range.first != range.second) {
            result.push_back(range.first->second);
            range.first++;
        }

        return result;
    }
    //-----------------------------------------------------------------
    /**
     * Get resource at random index.
     */
    T getRandomRes(const std::string &name)
    {
        typename t_reses::size_type count = m_reses.count(name);
        if (count == 0) {
            throw ResourceException(ExInfo("no such resource")
                    .addInfo("name", name)
                    .addInfo("pack", toString()));
        }

        return getRes(name, Random::randomInt(count));
    }
    //-----------------------------------------------------------------
    /**
     * Count resources with this name.
     */
    int countRes(const std::string &name)
    {
        return m_reses.count(name);
    }
    //-----------------------------------------------------------------
    std::string toString()
    {
            ExInfo available_res = ExInfo("resources")
                .addInfo("name", getName());

            t_resIterator end = m_reses.end();
            for (t_resIterator item = m_reses.begin(); item != end; ++item) {
                available_res.addInfo("key", item->first);
            }
            return std::string(available_res.what());
    }

};

#endif

