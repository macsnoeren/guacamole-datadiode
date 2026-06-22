"""nettest flow registry. Each flow is a callable(ctx) -> result dict living in
its own module; the web UI builds one button per entry, so adding a flow is one
module plus one line here. Shared protocol constants and helpers live in
flows.common.
"""

from .custom import run_custom

TESTS = {
    "custom": run_custom,
}
