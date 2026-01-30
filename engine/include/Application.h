#pragma once

namespace eng
{

    class Application
    {
    public:
        virtual bool Init() = 0;
        // delta time in seconds
        virtual void Update(float DeltaTime) = 0;
        virtual void Destroy() = 0;

        void SetNeedsToBeClosed(bool value);
        bool NeedsToBeClosed() const;

    private:
        bool m_needsToBeClosed = false;
    };

}
