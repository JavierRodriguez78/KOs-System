#include <console/ncurses.hpp>

extern "C" int main() {
    using namespace kos::console::nc;
    init();
    set_color(15, 0);
    clear();
    uint32_t rows, cols; getmaxyx(rows, cols);
    draw_box(2, 2, (cols>20?20:cols-4), (rows>10?10:rows-4));
    move(4, 4);
    addstr("ncurses demo running");
    move(4, 6);
    addstr("Press reboot to exit");
    refresh();
    return 0;
}
