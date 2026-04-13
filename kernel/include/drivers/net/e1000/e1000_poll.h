#pragma once

extern "C" {
    // Poll E1000 RX ring for received packets
    void e1000_rx_poll();

    // One-shot E1000 RX hardware register snapshot for diagnostics
    void e1000_rx_snapshot();
}
