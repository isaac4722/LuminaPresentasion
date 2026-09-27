// src/core/src/Version.cpp — Implementación de Version

#include "fusion/Version.h"

namespace fusion {

const char* VersionString() {
    static const char k[] = "1.0.0";
    return k;
}
const char* AhpVersionString() {
    static const char k[] = "ahp.v1";
    return k;
}
const char* IpcVersionString() {
    static const char k[] = "ipc.v1";
    return k;
}

} // namespace fusion
