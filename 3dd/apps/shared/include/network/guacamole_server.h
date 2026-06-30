#pragma once

#include <netinet/in.h>
#include <stdlib.h>
#include <mutex>
#include <string>
#include <unordered_map>

// Forward-declared so includers don't pull in <openssl/ssl.h>. SSL_CTX and SSL
// are typedefs for `struct ssl_ctx_st` / `struct ssl_st`; the .cpp uses the real
// OpenSSL types.
struct ssl_ctx_st;
struct ssl_st;

/**
 * @brief A TCP server that accepts and serves multiple simultaneous clients
 *
 * Owns only the listening socket. Accepted client sockets are handed back to the
 * caller as raw fds; the caller (via a ChannelTable) decides their lifetime.
 * Receive/Send/Shutdown/Close all operate on a caller-supplied fd, so the same
 * server instance can be used from multiple threads, one per client.
 *
 * TLS is an optional, maintainer-toggled mode (see util/tls.h). When enabled,
 * Initialize() builds a server SSL_CTX and each accepted connection is wrapped
 * in its own SSL object; when disabled, ssl_ctx stays null and every operation
 * is plaintext — byte-for-byte the original behaviour. The per-connection SSL
 * object is only ever used from the single thread that owns the fd.
 */
class GuacamoleServer {
  private:
    std::string host;
    int recv_port;
    int listen_fd = -1;

    bool tls_on = false;          // whether this server speaks TLS
    ssl_ctx_st *ssl_ctx = nullptr; // shared server context (null in plaintext mode)

    // Per-connection SSL objects, keyed by fd. The accept thread inserts on
    // Accept(); the connection's owning thread looks them up for I/O and erases
    // on Close(). Guarded by ssl_mtx since accept and reader threads both touch
    // the map (though any one SSL object is only ever used by its owner thread).
    std::mutex ssl_mtx;
    std::unordered_map<int, ssl_st *> ssl_by_fd;

    /**
     * @brief Builds the server SSL_CTX and loads the cert/key
     * @return 0 on success, nonzero on failure (caller must abort startup)
     */
    int InitializeTls();

    /**
     * @brief Looks up the SSL object bound to fd, or nullptr if none/plaintext
     */
    ssl_st *SslFor(int fd);

  public:
    // Receive() returns this when the TLS layer has no application data yet
    // (handshake in progress or a partial record): the caller should retry,
    // not treat it as a closed connection.
    static constexpr int RETRY = -2;

    GuacamoleServer(std::string host, int recv_port)
        : host(host), recv_port(recv_port) {}

    /**
     * @brief Closes the listening socket and frees the TLS context
     */
    ~GuacamoleServer();

    /**
     * @brief Attempts to bind to the server address and listen on it, and (when
     * TLS is enabled) build the server SSL_CTX
     * @return 0 on success, nonzero on failure
     */
    int Initialize();

    /**
     * @brief Whether this server was initialized in TLS mode
     */
    bool TlsEnabled() const { return tls_on; }

    /**
     * @brief Waits for a client to connect (blocking)
     * @return The accepted client fd, or -1 on failure
     */
    int Accept();

    /**
     * @brief Waits up to timeout_ms for a client fd to become readable
     * @return 1 if readable (data or EOF), 0 on timeout, -1 on error
     */
    int WaitReadable(int fd, int timeout_ms);

    /**
     * @brief Whether the TLS layer has a buffered record ready to decrypt.
     *
     * After SSL_read consumes a record, more may already sit in OpenSSL's buffer
     * without the socket being readable, so poll() alone would miss them. Always
     * false in plaintext mode. The caller treats a true result like readability.
     */
    bool HasPending(int fd);

    /**
     * @brief Receives traffic from a client fd into buffer (blocking)
     * @return Bytes received, 0 if the client closed, RETRY if the TLS layer has
     *         no application data yet (retry), -1 on error
     */
    int Receive(int fd, char buffer[], size_t len);

    /**
     * @brief Sends all bytes in buffer to a client fd
     * @return Amount of bytes sent, or -1 on error
     */
    ssize_t Send(int fd, const char *buffer, size_t len);

    /**
     * @brief Half-closes a client fd, waking any blocking Receive on it
     */
    void Shutdown(int fd);

    /**
     * @brief Closes a client fd
     */
    void Close(int fd);
};
