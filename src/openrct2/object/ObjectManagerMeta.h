/*****************************************************************************
 * Copyright (c) 2014-2022 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "ObjectEntryIndex.h"

enum class ObjectType : uint8_t;

namespace OpenRCT2::ObjectManager
{
    namespace Details
    {
        const void* GetMetaAny(ObjectType type, ObjectEntryIndex id);
    }
    template<typename T> const T* GetMeta(ObjectEntryIndex id)
    {
        return reinterpret_cast<const T*>(Details::GetMetaAny(T::kType, id));
    }
} // namespace OpenRCT2::ObjectManager
