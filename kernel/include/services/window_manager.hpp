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

    // Mouse polling control: 0=never, 1=until first packet, 2=always
    static void SetMousePollMode(uint8_t mode);

    // Spawn a new graphical terminal window. This will rebind the global
    // Terminal renderer to the new window, clear its buffer, and print a
    // fresh prompt. Returns the new window id or 0 on failure.
    static uint32_t SpawnTerminal();
};

}}

#endif
