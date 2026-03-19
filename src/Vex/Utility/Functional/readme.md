std::move_only_function is not fully implemented in certain toolchains (eg: https://github.com/llvm/llvm-project/issues/105157)
This is used as a stopgap until it is finally implemented, we cannot use traditional std::function for our purposes as it does not support the storage of std::unique_ptr in captures.

Sourced from https://github.com/zhihaoy/nontype_functional.