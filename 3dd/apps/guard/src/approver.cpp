#include "../include/approver.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

ApprovalResult Approver::HandleRequest(const std::string &request_id) {
    // PoC operator stand-in: deny everything when asked to, otherwise approve.
    const char *policy = std::getenv("GUARD_APPROVE");
    if (policy && std::strcmp(policy, "deny") == 0) {
        std::cout << "Approver: DENY request " << request_id << std::endl;
        return {false, "operator denied the request"};
    }

    std::cout << "Approver: APPROVE request " << request_id << std::endl;
    return {true, ""};
}
