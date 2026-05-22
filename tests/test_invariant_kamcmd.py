import pytest
import ctypes
import os
import sys

# Simulate the sockaddr_un sun_path buffer size constraint
# On Linux, sun_path is typically 108 bytes
SUN_PATH_MAX = 108

def safe_copy_to_sun_path(reply_socket: str) -> bytes:
    """
    Simulates the behavior that SHOULD happen when copying reply_socket
    into mysun.sun_path. Returns the bytes that would be written, or
    raises ValueError if the input exceeds the buffer size (secure behavior).
    """
    encoded = reply_socket.encode('utf-8') if isinstance(reply_socket, str) else reply_socket
    # Secure implementation: reject or truncate oversized input
    if len(encoded) >= SUN_PATH_MAX:
        raise ValueError(
            f"reply_socket length {len(encoded)} exceeds sun_path buffer size {SUN_PATH_MAX - 1}"
        )
    return encoded


def validate_sun_path_length(reply_socket: str) -> bool:
    """
    Returns True if the socket path is within safe bounds,
    False if it would cause a buffer overflow.
    """
    encoded = reply_socket.encode('utf-8') if isinstance(reply_socket, str) else reply_socket
    # Must fit within SUN_PATH_MAX including null terminator
    return len(encoded) < SUN_PATH_MAX


def simulate_strcpy_with_bounds_check(dest_buffer_size: int, src: str) -> bytes:
    """
    Simulates strcpy with bounds checking.
    Raises BufferError if src exceeds dest_buffer_size (including null terminator).
    """
    encoded = src.encode('utf-8') if isinstance(src, str) else src
    if len(encoded) + 1 > dest_buffer_size:
        raise BufferError(
            f"Source string length {len(encoded)} + null terminator exceeds "
            f"destination buffer size {dest_buffer_size}"
        )
    result = bytearray(dest_buffer_size)
    result[:len(encoded)] = encoded
    result[len(encoded)] = 0  # null terminator
    return bytes(result)


@pytest.mark.parametrize("payload", [
    # Exactly at boundary (should be rejected - no room for null terminator)
    "A" * 108,
    # 2x the buffer size
    "A" * 216,
    # 10x the buffer size
    "A" * 1080,
    # 100x the buffer size
    "A" * 10800,
    # Path-like oversized input
    "/tmp/" + "a" * 200,
    # Path with special characters exceeding buffer
    "/var/run/" + "x" * 150,
    # Unicode-heavy oversized path
    "/tmp/" + "ñ" * 60,  # multi-byte chars
    # Null bytes embedded in oversized string
    "/tmp/" + "A" * 103 + "\x00" + "B" * 50,
    # Format string characters in oversized input
    "%s" * 60,
    # Path traversal in oversized input
    "../" * 40 + "evil",
    # Exactly one byte over the limit
    "A" * 109,
    # Two bytes over the limit
    "A" * 110,
    # Large realistic-looking socket path
    "/var/run/kamailio/" + "socket_" + "x" * 100,
    # Mixed content oversized
    "/tmp/kam_" + "0123456789abcdef" * 10,
    # Whitespace-padded oversized
    " " * 200,
    # Newline-embedded oversized
    "A" * 50 + "\n" + "B" * 60,
    # Shell metacharacters in oversized input
    "$(echo " + "A" * 110 + ")",
    # Null-byte at start with oversized content
    "\x00" + "A" * 110,
    # Very large input (stress test)
    "Z" * 65536,
])
def test_sun_path_buffer_overflow_prevention(payload):
    """
    Invariant: Buffer reads/writes to sun_path must never exceed the declared
    buffer length of SUN_PATH_MAX (108 bytes on Linux). Any input that would
    cause strcpy to write beyond this boundary must be rejected or safely
    truncated before the copy operation. No out-of-bounds access should occur.
    """
    # Encode payload to get actual byte length (important for multi-byte chars)
    if isinstance(payload, str):
        encoded_payload = payload.encode('utf-8', errors='replace')
    else:
        encoded_payload = payload

    # INVARIANT CHECK 1: Validate that oversized inputs are detected
    # Any payload >= SUN_PATH_MAX bytes should be flagged as unsafe
    if len(encoded_payload) >= SUN_PATH_MAX:
        # The safe validation function must return False for oversized inputs
        assert not validate_sun_path_length(payload), (
            f"SECURITY VIOLATION: validate_sun_path_length returned True for "
            f"payload of length {len(encoded_payload)} which exceeds "
            f"SUN_PATH_MAX={SUN_PATH_MAX}"
        )

        # INVARIANT CHECK 2: safe_copy_to_sun_path must raise an error
        # (not silently overflow) for oversized inputs
        with pytest.raises((ValueError, BufferError, OverflowError)):
            safe_copy_to_sun_path(payload)

        # INVARIANT CHECK 3: simulate_strcpy_with_bounds_check must raise
        # BufferError for inputs that don't fit in the destination buffer
        with pytest.raises(BufferError):
            simulate_strcpy_with_bounds_check(SUN_PATH_MAX, payload)

    else:
        # For safe-sized inputs, operations should succeed
        result = safe_copy_to_sun_path(payload)
        assert len(result) < SUN_PATH_MAX, (
            f"Result length {len(result)} must be less than SUN_PATH_MAX={SUN_PATH_MAX}"
        )

        buf = simulate_strcpy_with_bounds_check(SUN_PATH_MAX, payload)
        assert len(buf) == SUN_PATH_MAX, (
            f"Buffer should be exactly SUN_PATH_MAX={SUN_PATH_MAX} bytes"
        )
        # Verify null terminator is present within bounds
        assert b'\x00' in buf, "Buffer must contain null terminator"
        null_pos = buf.index(b'\x00')
        assert null_pos < SUN_PATH_MAX, (
            f"Null terminator at position {null_pos} must be within buffer bounds"
        )


@pytest.mark.parametrize("payload,expected_safe", [
    # Safe inputs
    ("/tmp/kam.sock", True),
    ("/var/run/kamailio.sock", True),
    ("", True),
    ("A" * 107, True),  # exactly fits with null terminator
    # Unsafe inputs
    ("A" * 108, False),  # no room for null terminator
    ("A" * 109, False),
    ("/tmp/" + "x" * 104, False),
    ("B" * 1000, False),
])
def test_sun_path_boundary_conditions(payload, expected_safe):
    """
    Invariant: The boundary between safe and unsafe socket path lengths
    must be strictly enforced at SUN_PATH_MAX-1 characters (107 bytes)
    to always leave room for the null terminator.
    """
    result = validate_sun_path_length(payload)
    assert result == expected_safe, (
        f"validate_sun_path_length('{payload[:20]}...') returned {result}, "
        f"expected {expected_safe} for payload of length "
        f"{len(payload.encode('utf-8'))}"
    )

    if not expected_safe:
        # Confirm that attempting the copy raises an appropriate error
        with pytest.raises((ValueError, BufferError, OverflowError)):
            safe_copy_to_sun_path(payload)


def test_sun_path_max_constant_is_correct():
    """
    Invariant: The SUN_PATH_MAX constant used for bounds checking must
    match the actual platform constraint (108 bytes on Linux).
    """
    # Verify our constant matches the expected Linux value
    assert SUN_PATH_MAX == 108, (
        f"SUN_PATH_MAX should be 108 on Linux, got {SUN_PATH_MAX}"
    )
    # Verify that the maximum safe string length is SUN_PATH_MAX - 1
    max_safe_string = "A" * (SUN_PATH_MAX - 1)
    assert validate_sun_path_length(max_safe_string), (
        f"String of length {SUN_PATH_MAX - 1} should be safe"
    )
    # Verify that SUN_PATH_MAX length string is unsafe
    unsafe_string = "A" * SUN_PATH_MAX
    assert not validate_sun_path_length(unsafe_string), (
        f"String of length {SUN_PATH_MAX} should be unsafe (no room for null terminator)"
    )