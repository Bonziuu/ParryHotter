"""Scoped numeric compatibility for an installed Kerrax Blender exporter."""
from contextlib import contextmanager


@contextmanager
def stable_krx_rotations():
    """Avoid the legacy trace formula's precision loss near 180 degrees.

    No exporter code is installed or copied. Other exporters and environments
    without the Kerrax Python module are left to their own implementation.
    """
    try:
        from KrxImpExp import KrxAscExp as exporter
    except ImportError:
        yield
        return
    original = getattr(exporter, "get_rotation_part", None)
    if original is None:
        yield
        return

    def stable_rotation(matrix):
        quat = matrix.to_3x3().to_quaternion().normalized()
        return (quat.x, quat.y, quat.z, quat.w)

    exporter.get_rotation_part = stable_rotation
    try:
        yield
    finally:
        exporter.get_rotation_part = original
