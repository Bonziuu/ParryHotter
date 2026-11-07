"""Atomic file replacement with rollback across a small set of game files."""
from __future__ import annotations

import errno
import os
import shutil
import sys
import tempfile
from pathlib import Path
from typing import BinaryIO, Callable


class FileTransaction:
    """Preserve previous files and remove only paths created by this transaction.

    Existing files are snapshotted to disk before their first mutation. Directory
    cleanup uses rmdir rather than recursive deletion, preserving unrelated files
    that appeared while an operation was in progress.
    """

    def __init__(self, root: Path):
        self.root = Path(root).resolve()
        self._storage = Path(tempfile.mkdtemp(prefix="parry-rollback-"))
        self._originals: dict[Path, Path | None] = {}
        self._changed: set[Path] = set()
        self._directories: list[Path] = []

    def __enter__(self) -> FileTransaction:
        return self

    def check_file(self, path: Path) -> None:
        if not path.resolve().is_relative_to(self.root):
            raise OSError(f"File path escapes the selected installation: {path}")
        if path.is_symlink() or (path.exists() and not path.is_file()):
            raise OSError(f"File path is not a regular file: {path}")
        for parent in path.parents:
            if parent == self.root:
                break
            if parent.exists() and not parent.is_dir():
                raise OSError(f"Directory path is occupied by a file: {parent}")

    def _remember(self, path: Path) -> None:
        self.check_file(path)
        if path not in self._originals:
            original = None
            if path.is_file():
                original = self._storage / str(len(self._originals))
                shutil.copy2(path, original)
            self._originals[path] = original

    def _mkdir(self, directory: Path) -> None:
        missing = []
        while not directory.exists():
            missing.append(directory)
            directory = directory.parent
        for path in reversed(missing):
            try:
                path.mkdir()
            except FileExistsError:
                if not path.is_dir():
                    raise
            else:
                self._directories.append(path)

    @staticmethod
    def _replace(path: Path, writer: Callable[[BinaryIO], None], *, source: Path | None = None) -> None:
        temporary = None
        try:
            with tempfile.NamedTemporaryFile(mode="wb", dir=path.parent,
                                             prefix=f".{path.name}.", suffix=".tmp", delete=False) as output:
                temporary = Path(output.name)
                writer(output)
                output.flush()
                os.fsync(output.fileno())
            if source is not None:
                shutil.copystat(source, temporary)
            elif path.is_file():
                shutil.copymode(path, temporary)
            temporary.replace(path)
        except BaseException as error:
            if temporary is not None:
                try:
                    temporary.unlink(missing_ok=True)
                except OSError as cleanup_error:
                    message = f"Could not remove transaction temporary file {temporary}: {cleanup_error}"
                    FileTransaction._report(error, message)
            raise

    def write_bytes(self, path: Path, data: bytes) -> None:
        path = Path(path)
        self._remember(path)
        self._mkdir(path.parent)
        self._replace(path, lambda output: output.write(data))
        self._changed.add(path)

    def copy_file(self, source: Path, path: Path) -> None:
        path = Path(path)
        self._remember(path)
        self._mkdir(path.parent)

        def copy(output: BinaryIO) -> None:
            with source.open("rb") as stream:
                shutil.copyfileobj(stream, output)

        self._replace(path, copy, source=source)
        self._changed.add(path)

    def _rollback(self) -> list[str]:
        failures = []
        for path, original in reversed(self._originals.items()):
            if path not in self._changed:
                continue
            try:
                self.check_file(path)
                if original is None:
                    path.unlink(missing_ok=True)
                else:
                    def restore(output: BinaryIO) -> None:
                        with original.open("rb") as stream:
                            shutil.copyfileobj(stream, output)
                    self._replace(path, restore, source=original)
            except Exception as error:
                failures.append(f"{path}: {error}")
        for directory in reversed(self._directories):
            try:
                directory.rmdir()
            except FileNotFoundError:
                pass
            except OSError as error:
                if error.errno not in (errno.ENOTEMPTY, errno.EEXIST):
                    failures.append(f"{directory}: {error}")
        return failures

    @staticmethod
    def _report(error: BaseException | None, message: str) -> None:
        add_note = getattr(error, "add_note", None)
        if callable(add_note):
            add_note(message)
        try:
            print(message, file=sys.stderr)
        except OSError:
            pass  # An unavailable stderr must not replace the original error.

    def __exit__(self, error_type, error, traceback) -> bool:
        failures = []
        if error is not None:
            failures = self._rollback()
            if failures:
                mapping = "\n".join(f"{backup.name if backup else '<new file>'}: {path}"
                                    for path, backup in self._originals.items())
                try:
                    (self._storage / "RESTORE.txt").write_text(mapping, encoding="utf-8")
                except OSError as manifest_error:
                    failures.append(f"Cannot write recovery manifest: {manifest_error}")
                self._report(error, "Rollback incomplete; saved originals remain at "
                             f"{self._storage}. Restore these paths manually:\n" + "\n".join(failures))
        if not failures:
            try:
                shutil.rmtree(self._storage)
            except OSError as cleanup_error:
                self._report(error, f"Could not remove rollback snapshots {self._storage}: {cleanup_error}")
        return False
