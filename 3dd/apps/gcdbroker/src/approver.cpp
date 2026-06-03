#include "../include/approver.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

ApprovalResult Approver::HandleRequest(const std::string &protocol,
                                       const std::string &connect) {
    (void)connect; // reserved for a richer operator prompt later

    // PoC operator stand-in: deny everything when asked to, otherwise approve.
    const char *policy = std::getenv("GCD_APPROVE");
    if (policy && std::strcmp(policy, "deny") == 0) {
        std::cout << "Approver: DENY " << protocol << " request" << std::endl;
        return {false, "operator denied the request"};
    }

    std::cout << "Approver: APPROVE " << protocol << " request" << std::endl;
    return {true, ""};
}
