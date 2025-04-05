/*
Copyright (C) 2025 Maurice Snoeren

This program is free software: you can redistribute it and/or modify it under the terms of 
the GNU General Public License as published by the Free Software Foundation, version 3.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program.
If not, see https://www.gnu.org/licenses/.
*/
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <queue>
#include <mutex>

template<typename T>
class queueThreadSafe {
private:
    std::queue<T> queueData;
    std::mutex mutexQueue;

public:
    queueThreadSafe () {

    }

    ~queueThreadSafe () {

    }

    void pop () {
        this->mutexQueue.lock();
        this->queueData.pop();
        this->mutexQueue.unlock();
    }

    T front () {
        this->mutexQueue.lock();
        T data = this->queueData.front();
        this->mutexQueue.unlock();

        return data;
    }

    void push (T data) {
        this->mutexQueue.lock();
        this->queueData.push(data);
        this->mutexQueue.unlock();
    }

    bool empty() {
        this->mutexQueue.lock();
        bool e = this->queueData.empty();
        this->mutexQueue.unlock();

        return e;
    }

};