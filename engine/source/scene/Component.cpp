#include "scene/Component.h"

namespace eng
{

    GameObject *eng::Component::GetOwner()
    {
        return m_owner;
    }

}