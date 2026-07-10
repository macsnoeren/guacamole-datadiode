#pragma once

#include "../../shared/include/parser/opcode_parser.h"
#include <cstddef>
#include <string>

/**
 * @brief Fakes the `ack` the browser waits for when the guard drops an oversized
 *        clipboard blob.
 *
 * A clipboard paste is `clipboard,<idx>,<mime>` then one or more
 * `blob,<idx>,<base64>` then `end,<idx>`; the browser waits for guacd to `ack`
 * each blob before the write completes. The guard drops any clipboard blob whose
 * payload exceeds the cap, so guacd never sees it and never acks — stalling the
 * browser's clipboard stream. This watches the forward (browser → guacd) stream
 * and, for each oversized clipboard blob (exactly the ones the guard will drop),
 * produces the matching success `ack` to send back to the browser, so the paste
 * fails cleanly instead of hanging.
 *
 * It reuses the shared `OpcodeParser` framer (allowing every opcode, tolerating
 * large payloads); it only observes and never alters the forward stream — the
 * guard remains what actually drops the blob. The stream tracking mirrors
 * `GuardOpcodeParser`, but acts (emits an ack) instead of denying.
 */
class ClipboardAckFaker : public OpcodeParser {
  public:
    /**
     * @brief Feed forward bytes; returns any `ack` instructions to send back to
     *        the browser (concatenated), or "" if none.
     */
    std::string Feed(const char *data, size_t len);

  protected:
    bool OnInstructionBegin(const GuacElement &instr) override;
    bool OnArgument(const GuacElement &arg) override;
    bool ToleratesOversizedElements() override { return true; }

  private:
    std::string current_opcode;
    int clipboard_sidx = -1;        // currently open clipboard stream, or -1
    size_t current_arg = 0;         // 1-based index of the argument being parsed
    bool blob_on_clipboard = false; // the current blob is on the clipboard stream
    std::string acks;               // acks produced during the current Feed()
};
