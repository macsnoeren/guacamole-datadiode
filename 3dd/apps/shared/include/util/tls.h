#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

/**
 * @brief Maintainer switch for the gmlbroker <-> web-server TLS leg.
 *
 * Off by default so the PoC/demo keeps running in plaintext with no certs
 * configured. Set GMLBROKER_TLS=1 (or on/true/yes) to make gmlbroker present a
 * TLS server certificate to the Guacamole web server, which must then be
 * configured with `guacd-ssl: true`. Read once at startup: both ends agree on
 * the mode statically, so there is no per-connection or live toggle.
 */
inline bool tls_enabled() {
    const char *env = std::getenv("GMLBROKER_TLS");
    if (!env)
        return false;
    std::string v(env);
    std::transform(v.begin(), v.end(), v.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return v == "1" || v == "on" || v == "true" || v == "yes";
}

/**
 * @brief Path to the PEM server certificate chain gmlbroker presents.
 */
inline std::string tls_cert_path() {
    const char *env = std::getenv("GMLBROKER_TLS_CERT");
    return env ? std::string(env) : std::string("/etc/gmlbroker/tls/cert.pem");
}

/**
 * @brief Path to the PEM private key matching the certificate.
 */
inline std::string tls_key_path() {
    const char *env = std::getenv("GMLBROKER_TLS_KEY");
    return env ? std::string(env) : std::string("/etc/gmlbroker/tls/key.pem");
}
