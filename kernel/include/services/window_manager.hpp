#ifndef KOS_SERVICES_WINDOW_MANAGER_HPP
#define KOS_SERVICES_WINDOW_MANAGER_HPP

#include <services/service.hpp>

namespace kos { namespace services {

class WindowManager : public IService {
public:
    virtual const char* Name() const override { return "WINMAN"; }
    virtual bool Start() override;
    virtual void Tick() override;
    virtual bool DefaultEnabled() const override { return true; }
    virtual uint32_t TickIntervalMs() const override { return 33; } // ~30 FPS for demo
};

}}

#endif
