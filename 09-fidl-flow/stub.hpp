#pragma once
#include <cstddef>
#include <sys/neutrino.h>

// Called only for application messages, after system-message handling.
bool dispatch_add(rcvid_t rcvid, const void* data, std::size_t received,
                  std::size_t sent);
