import ctypes
from typing import Iterable

import pytest as pytest

from .utils.library import Library, compile_library, load_library


@pytest.fixture(scope="session")
def cdll() -> ctypes.CDLL:
    compile_library()
    return load_library()


@pytest.fixture
def library(cdll: ctypes.CDLL) -> Iterable[Library]:
    library = Library(cdll)
    yield library
    library.clear()
