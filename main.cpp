#include "TIL.hpp"
#include <iostream>

int main() {
#if defined(_WIN32)
    // On Windows, enable UTF-8 output.
    SetConsoleOutputCP(CP_UTF8);
#endif

    // 1. Open a terminal input session.
    til::Input input;
    if (!input.IsInitialized()) {
        std::cerr<<"Failed to initialize input.\n";
        return 1;
    }

    std::cout<<"Type something. Press Escape to exit."<<std::endl;

    while (true) {
        // 2. Wait for input and update available events.
        if (!input.Wait()) {
            std::cerr<<"Failed to read input.\n";
            return 1;
        }
        if (input.GetLastError() == til::InputError::QueueFull) {
            std::cerr<<"Queue full: some events were lost.\n";
        }

        // 3. Read events one at a time, in arrival order.
        til::InputEvent evento;
        while (input.GetEvent(evento)) {
            if (evento.type == til::InputEvent::Type::Text) {
                std::cout<<"Text: "<<evento.text<<std::endl;
            } else if (evento.type == til::InputEvent::Type::Key) {
                if (evento.key == til::Key::Escape) {
                    // Destroying input restores the terminal before returning.
                    return 0;
                }
                if (evento.key == til::Key::Enter) {
                    std::cout<<"You pressed Enter."<<std::endl;
                }
            }
        }
    }
}
