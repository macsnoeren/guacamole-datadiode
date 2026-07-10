#pragma once

#include <atomic>

/*
 * @brief Process-wide run flag, cleared by the SIGINT handler.
 *
 * Defined in main.cpp; every handler loop polls it to know when to stop.
 */
extern std::atomic<bool> running;
