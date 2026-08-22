#include <iostream>
#include <string>

namespace {

void print_prompt()
{
    std::cout << "serodb > ";
}

} // namespace

int main()
{
    std::string input;

    while (true) {
        print_prompt();

        if (!std::getline(std::cin, input)) {
            break;
        }

        if (input == ".exit") {
            break;
        }
    }

    return 0;
}
