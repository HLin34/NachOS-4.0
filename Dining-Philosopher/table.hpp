#ifndef TABLE_HPP
#define TABLE_HPP

#include <pthread.h>
#include <vector>

class Table {
public:
    explicit Table(int n);
    // Reserve/release both adjacent forks in FIFO request order.
    void wait(int id);
    void signal(int id);
    // Retained diagnostic counter; it no longer controls scheduling.
    int getCurrentTurn();
    void updateCurrentTurn();
    ~Table();

    Table(const Table&) = delete;
    Table& operator=(const Table&) = delete;
private:
    struct Request {
        Table *table;
        int id;
        Request *previous;
        Request *next;
        bool queued;
    };
    static void FinishWait(void *request);
    void remove(Request *request);
    void validate(int id) const;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    const int participants;
    std::vector<bool> occupied;
    std::vector<int> state; // 0 idle, 1 queued, 2 owns a fork reservation
    Request *head;
    Request *tail;
    int currentTurn;
};
#endif // TABLE_HPP
