#pragma once
#include <atomic>
namespace iu::player {
inline std::atomic<uint64_t> submitted_frames{0};
}
