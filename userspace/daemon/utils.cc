#include <iostream>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);

    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

int get_pid_of_ppid(int ppid) {
	std::string command = "ps --ppid " + std::to_string(ppid) + " -o pid=";
    std::string output = exec(command.c_str());

	int pid = -1;

    if (!output.empty()) {
        std::cout << "Processes with PPID: " << ppid << std::endl;
        std::cout << output << std::endl;
		pid = std::stoi(output);
    } else {
        std::cout << "No child processes found with PPID: " << ppid << std::endl;
    }

    return pid;
}

#if 0
int main() {
	int pid = get_pid_of_ppid(125520);

	std::cout << pid << std::endl;

	return 0;
}
#endif
