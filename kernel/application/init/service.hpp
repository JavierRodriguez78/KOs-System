// InitD service: runs startup commands from /etc/init.d/rc.local once after boot
#pragma once

#include <services/service.hpp>

namespace kos {
	namespace services {

		// Simple init.d runner. It executes commands listed in /etc/init.d/rc.local
		// one per line, during service startup (synchronously) before the shell,
		// emulating a PID 1 style boot sequence.
		class InitDService : public IService {
		public:
			virtual const char* Name() const override { return "INITD"; }
			virtual bool Start() override; // Run init.d synchronously
			virtual void Tick() override {} // no periodic work
			virtual uint32_t TickIntervalMs() const override { return 0; }
			virtual bool DefaultEnabled() const override { return true; }

		private:
			// no state needed; Start() runs once
		};

	} // namespace services
} // namespace kos

