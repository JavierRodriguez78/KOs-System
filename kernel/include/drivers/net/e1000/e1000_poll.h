#pragma once

extern "C" {
    // Poll E1000 RX ring for received packets
    void e1000_rx_poll();
}
