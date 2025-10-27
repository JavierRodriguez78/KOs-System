#include "services/service_manager.hpp"

namespace kos {
namespace services {
void* g_fs_ptr = nullptr;

void* get_fs() {
    return g_fs_ptr;
}
} // namespace services
} // namespace kos
