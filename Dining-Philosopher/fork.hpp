#ifndef FORK_HPP
#define FORK_HPP

#include <pthread.h>

class Fork {
public:
    Fork();
    void wait();
    void signal(); 
    ~Fork();
private:
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int value;
};



#endif // FORK_HPP