#include "philosopher.hpp"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <vector>

int main() {
    std::vector<std::unique_ptr<Fork> > forks;
    std::unique_ptr<Table> table;
    std::vector<std::unique_ptr<Philosopher> > philosophers;
    size_t started = 0;
    int result = EXIT_SUCCESS;
    try {
        for (int i = 0; i < PHILOSOPHERS; i++) forks.emplace_back(new Fork);
        table.reset(new Table(PHILOSOPHERS));
        for (int i = 0; i < PHILOSOPHERS; i++)
            philosophers.emplace_back(new Philosopher(i, forks[i].get(), forks[(i + 1) % PHILOSOPHERS].get(), table.get()));
        for (; started < philosophers.size(); started++) philosophers[started]->start();
        sleep(RUNTIME);
    } catch (const std::exception &error) {
        fprintf(stderr, "Unable to run all philosophers: %s\n", error.what());
        result = EXIT_FAILURE;
    }
    for (size_t i = 0; i < started; i++) {
        int status = philosophers[i]->cancel();
        if (status != 0) {
            fprintf(stderr, "Cancellation request failed: %d\n", status);
            std::_Exit(EXIT_FAILURE); // Never destroy objects while workers may still run.
        }
    }
    for (size_t i = 0; i < started; i++) {
        int status = philosophers[i]->join();
        if (status != 0) {
            fprintf(stderr, "Join failed: %d\n", status);
            std::_Exit(EXIT_FAILURE);
        }
    }
    puts("All philosophers have stopped.");
    return result; // Shared resources are destroyed only after successful joins.
}
