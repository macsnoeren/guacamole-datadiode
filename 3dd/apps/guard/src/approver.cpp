#include "../include/approver.h"
#include <iostream>

ApprovalResult Approver::HandleRequest(const std::string &request_id) {
    // PoC operator stand-in: honour the runtime global approve/deny switch.
    if (!approve_.load(std::memory_order_relaxed)) {
        std::cout << "Approver: DENY request " << request_id << std::endl;
        return {false, "operator denied the request"};
    }

    // [ISSUE] MS: request_id is not checked. You state this is NUL-free, but that is only
    //             true for the local hex generator. Nothing is checking this value when 
    //             received from the UDP channel (external). An attacker could place bytes
    //             that fake log messages or try to escape the terminal system.
    std::cout << "Approver: APPROVE request " << request_id << std::endl;
    return {true, ""};
}
