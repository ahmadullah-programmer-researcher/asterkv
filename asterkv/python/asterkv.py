"""Small binary-safe Python client for the AsterKV C API.

Set ASTERKV_LIBRARY to the path of the built shared library, or build the
project with CMake so the loader can find it in the repository's build folder.
"""
from __future__ import annotations

import ctypes
import os
from pathlib import Path
from typing import Optional

ASTERKV_OK = 0
ASTERKV_NOT_FOUND = 1
ASTERKV_ERROR = 2


def _candidate_libraries() -> list[Path]:
    candidates: list[Path] = []
    configured = os.environ.get("ASTERKV_LIBRARY")
    if configured:
        candidates.append(Path(configured))
    root = Path(__file__).resolve().parents[1]
    names = ["asterkv.dll", "libasterkv.dylib", "libasterkv.so"]
    for build_dir in (root / "build", root / "build" / "Release", root / "build" / "Debug"):
        for name in names:
            candidates.append(build_dir / name)
    return candidates


def _load_library() -> ctypes.CDLL:
    errors = []
    for candidate in _candidate_libraries():
        if candidate.exists():
            try:
                return ctypes.CDLL(str(candidate))
            except OSError as exc:
                errors.append(str(exc))
    raise RuntimeError(
        "AsterKV shared library not found. Build with CMake or set ASTERKV_LIBRARY. "
        + "; ".join(errors)
    )


class Database:
    def __init__(self, path: str | os.PathLike[str]):
        self._lib = _load_library()
        self._configure()
        self._handle = ctypes.c_void_p()
        status = self._lib.asterkv_open(os.fsencode(path), ctypes.byref(self._handle))
        self._check(status)

    def _configure(self) -> None:
        lib = self._lib
        lib.asterkv_open.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
        lib.asterkv_open.restype = ctypes.c_int
        lib.asterkv_put.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
                                    ctypes.c_void_p, ctypes.c_size_t]
        lib.asterkv_put.restype = ctypes.c_int
        lib.asterkv_get.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
                                    ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_size_t)]
        lib.asterkv_get.restype = ctypes.c_int
        lib.asterkv_delete.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
        lib.asterkv_delete.restype = ctypes.c_int
        lib.asterkv_flush.argtypes = [ctypes.c_void_p]
        lib.asterkv_flush.restype = ctypes.c_int
        lib.asterkv_free.argtypes = [ctypes.c_void_p]
        lib.asterkv_close.argtypes = [ctypes.c_void_p]
        lib.asterkv_last_error.restype = ctypes.c_char_p

    def _check(self, status: int) -> None:
        if status == ASTERKV_ERROR:
            message = self._lib.asterkv_last_error()
            raise RuntimeError(message.decode("utf-8", errors="replace") if message else "AsterKV error")

    @staticmethod
    def _buffer(value: bytes) -> tuple[ctypes.Array, ctypes.c_void_p]:
        storage = ctypes.create_string_buffer(value, max(1, len(value)))
        return storage, ctypes.cast(storage, ctypes.c_void_p)

    def put(self, key: bytes | str, value: bytes | str) -> None:
        key_b = key.encode() if isinstance(key, str) else bytes(key)
        value_b = value.encode() if isinstance(value, str) else bytes(value)
        key_buf, key_ptr = self._buffer(key_b)
        value_buf, value_ptr = self._buffer(value_b)
        status = self._lib.asterkv_put(self._handle, key_ptr, len(key_b), value_ptr, len(value_b))
        self._check(status)

    def get(self, key: bytes | str) -> Optional[bytes]:
        key_b = key.encode() if isinstance(key, str) else bytes(key)
        key_buf, key_ptr = self._buffer(key_b)
        out = ctypes.c_void_p()
        out_len = ctypes.c_size_t()
        status = self._lib.asterkv_get(self._handle, key_ptr, len(key_b), ctypes.byref(out), ctypes.byref(out_len))
        if status == ASTERKV_NOT_FOUND:
            return None
        self._check(status)
        try:
            return ctypes.string_at(out, out_len.value)
        finally:
            self._lib.asterkv_free(out)

    def delete(self, key: bytes | str) -> None:
        key_b = key.encode() if isinstance(key, str) else bytes(key)
        key_buf, key_ptr = self._buffer(key_b)
        self._check(self._lib.asterkv_delete(self._handle, key_ptr, len(key_b)))

    def flush(self) -> None:
        self._check(self._lib.asterkv_flush(self._handle))

    def close(self) -> None:
        if getattr(self, "_handle", None):
            self._lib.asterkv_close(self._handle)
            self._handle = ctypes.c_void_p()

    def __enter__(self) -> "Database":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    def __getitem__(self, key: bytes | str) -> bytes:
        value = self.get(key)
        if value is None:
            raise KeyError(key)
        return value

    def __setitem__(self, key: bytes | str, value: bytes | str) -> None:
        self.put(key, value)

    def __delitem__(self, key: bytes | str) -> None:
        self.delete(key)
