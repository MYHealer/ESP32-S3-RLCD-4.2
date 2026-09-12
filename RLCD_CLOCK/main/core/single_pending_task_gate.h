// 为延后任务保留原类型名，底层复用通用单所有者原子门。
#pragma once

#include "atomic_ownership_gate.h"

using SinglePendingTaskGate = AtomicOwnershipGate;
