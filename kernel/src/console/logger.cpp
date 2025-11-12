#include <console/logger.hpp>

namespace kos { 
	namespace console {
		bool Logger::s_debugEnabled = false;
		bool Logger::s_mutedTTY = false;
	}
}

// Most implementations are inline in the header for simplicity.
