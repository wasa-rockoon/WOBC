#include "process.h"

namespace process {

void Process::startProcess(kernel::Kernel* kernel) { 
    kernel_ = kernel; 
    onStart(); 
}
}