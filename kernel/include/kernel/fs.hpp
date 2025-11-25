#pragma once
#include <common/types.hpp>
namespace kos { namespace fs { class Filesystem; } }

namespace kos { namespace kernel {
// Scan ATA devices and mount a filesystem; returns true if mounted and sets g_fs_ptr.
bool ScanAndMountFilesystems();
}
}
