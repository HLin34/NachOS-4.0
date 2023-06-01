#ifndef TABLE_HPP
#define TABLE_HPP

#include <pthread.h>

class Table {
public:
    Table(int n);
    void wait();
    void signal();
    int getCurrentTurn();     // Starvation
    void updateCurrentTurn(); // Starvation
    ~Table();

private:
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int value;
    int currentTurn;          // Starvation
};



#endif // TABLE_HPP