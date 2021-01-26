/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

#include "Internal.h"
#include <list>


/// This exception is thrown by MustKnowUsed if the used selector was not set.
class fsNoSelectorUsedException : public std::exception
{
public:
    fsNoSelectorUsedException() throw()
    {
        return;
    }
    
    virtual ~fsNoSelectorUsedException() throw()
    {
        return;
    }
    
    virtual const char* what() const throw()
    {
        return "No selector was used! (Meaning the format was not recogized.)";
    }
};

/// A template class to derive selectors for different types of formats.
template<class T>
class fsFormatSelector : public T
{
public:
    fsFormatSelector()
    {
        return;
    }

    virtual ~fsFormatSelector()
    {
        return;
    }

    /// Just a typedef for the format type as a pointer (const).
    typedef shared_ptr<const T> fsConstFormat;

    /// Just a typedef for the format type as a pointer (non-const).
    typedef shared_ptr<T> fsFormat;

    /// The type for a list of pointers to formats.
    typedef std::list<fsFormat> fsFormatList;

    /// Add an item to the list.
    inline void SelectorListAdd(fsFormat Format)
    {
        if (Format)
        {
            m_Formats.push_back(Format);
        }
        return;
    }

    /// Add multiple items to the list.
    inline void SelectorListAdd(fsFormat* Formats, unsigned int Count)
    {
        for (unsigned int i = 0; i < Count; i++)
        {
            if (Formats[i])
            {
                m_Formats.push_back(Formats[i]);
            }
        }
        return;
    }

    /// Set which format should be preferred over the rest.
    inline void SelectorListPrefer(fsFormat Format)
    {
        if (Format)
        {
            m_Formats.push_front(Format);
        }
        return;
    }

    /// Get the list (const).
    inline const fsFormatList& SelectorList() const
    {
        return m_Formats;
    }

    /// Get the list (non-const).
    inline fsFormatList& SelectorList()
    {
        return m_Formats;
    }

    /// Get the one that is actually being used (const).
    inline fsConstFormat SelectorUsed() const
    {
        return m_SelectorUsed;
    }

    /// Get the one that is actually being used (non-const).
    inline fsFormat SelectorUsed()
    {
        return m_SelectorUsed;
    }

protected:
    /// This function throws an exception if we're not using a selector.
    inline void MustKnowUsed() const
    {
        if (!m_SelectorUsed)
        {
            throw (fsNoSelectorUsedException());
        }
        return;
    }

    /// Set the selector to use. Call after recognizing which format it is in.
    inline void SetSelectorUsed(fsFormat Use)
    {
        m_SelectorUsed = Use;
        return;
    }

    /// Shortcut to get the one that is actually being used (const).
    inline fsConstFormat SU() const
    {
        MustKnowUsed();
        return m_SelectorUsed;
    }

    /// Shortcut to get the one that is actually being used (const).
    inline fsFormat SU()
    {
        MustKnowUsed();
        return m_SelectorUsed;
    }

private:
    fsFormatList m_Formats;
    fsFormat m_SelectorUsed;
};
