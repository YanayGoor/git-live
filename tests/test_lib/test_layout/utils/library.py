import ctypes
import subprocess
from dataclasses import dataclass, field
from pathlib import Path
from typing import TYPE_CHECKING, Any, Protocol

PROJ_DIR = Path(__file__).parent.parent.parent.parent.parent

LAYOUT_LIB_NAME = "liblayout.so"
LAYOUT_LIB_DIR = PROJ_DIR / "lib" / "layout"
LAYOUT_LIB_PATH = LAYOUT_LIB_DIR / LAYOUT_LIB_NAME

draw_text_callback = ctypes.CFUNCTYPE(
    ctypes.c_int,
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.c_uint32,
    ctypes.c_uint32,
    ctypes.c_uint32,
    ctypes.c_int,
    ctypes.c_int,
)
draw_color_callback = ctypes.CFUNCTYPE(
    ctypes.c_int,
    ctypes.c_void_p,
    ctypes.c_uint32,
    ctypes.c_uint32,
    ctypes.c_uint32,
    ctypes.c_uint32,
    ctypes.c_int,
)


NODE_NO_WRAP = 0
NODE_WRAP = 1

NODE_DIRECTION_COLS = 0
NODE_DIRECTION_ROWS = 1


class Rect(ctypes.Structure):
    _fields_ = [
        ("col", ctypes.c_uint32),
        ("row", ctypes.c_uint32),
        ("width", ctypes.c_uint32),
        ("height", ctypes.c_uint32),
    ]


class Node(ctypes.Structure):
    pass


class NodeListEntry(ctypes.Structure):
    _fields_ = [
        ("le_next", ctypes.POINTER(Node)),
        ("le_next", ctypes.POINTER(ctypes.POINTER(Node))),
    ]


class NodeListHead(ctypes.Structure):
    _fields_ = [("lh_first", ctypes.POINTER(Node))]


Node._fields_ = [
    ("entry", NodeListEntry),
    ("basis", ctypes.c_uint32),
    ("expand", ctypes.c_uint32),
    ("fit_content", ctypes.c_bool),
    ("wrap", ctypes.c_int),
    ("padding_top", ctypes.c_uint32),
    ("padding_bottom", ctypes.c_uint32),
    ("padding_left", ctypes.c_uint32),
    ("padding_right", ctypes.c_uint32),
    ("nodes_direction", ctypes.c_int),
    ("nodes", NodeListHead),
    ("content", ctypes.c_char_p),
    ("attr", ctypes.c_uint32),
    ("color", ctypes.c_short),
]

Layout = ctypes.c_void_p


if TYPE_CHECKING:
    NodePointer = ctypes._Pointer[Node]
else:
    NodePointer = ctypes.POINTER(Node)


class Screen(Protocol):
    width: int
    height: int

    def draw_text(
        self, text: bytes, length: int, col: int, row: int, color: int, attrs: int
    ):
        ...

    def draw_color(self, x: int, y: int, width: int, height: int, color: int):
        ...


def compile_library() -> None:
    subprocess.run(["make", LAYOUT_LIB_NAME], cwd=LAYOUT_LIB_DIR, check=True)


def load_library() -> ctypes.CDLL:
    lib_layout = ctypes.cdll.LoadLibrary(str(LAYOUT_LIB_PATH))

    lib_layout.init_layout.argtypes = [
        ctypes.POINTER(Layout),
        draw_text_callback,
        draw_color_callback,
        ctypes.c_void_p,
    ]
    lib_layout.free_layout.argtypes = [Layout]
    lib_layout.clear_layout.argtypes = [Layout]
    lib_layout.draw_layout.argtypes = [Layout, Rect]
    lib_layout.get_layout_root.argtypes = [Layout, ctypes.POINTER(ctypes.POINTER(Node))]

    lib_layout.append_text.argtypes = [ctypes.POINTER(Node), ctypes.c_char_p]
    lib_layout.append_styled_text.argtypes = [
        ctypes.POINTER(Node),
        ctypes.c_char_p,
        ctypes.c_short,
        ctypes.c_uint32,
    ]
    lib_layout.append_child.argtypes = [
        ctypes.POINTER(Node),
        ctypes.POINTER(ctypes.POINTER(Node)),
    ]
    lib_layout.clear_children.argtypes = [ctypes.POINTER(Node)]

    return lib_layout


@dataclass
class Library:
    _library: ctypes.CDLL
    _not_garbage: list[Any] = field(init=False, default_factory=list)

    def init_layout(self, scr: Screen) -> tuple[ctypes.c_void_p, NodePointer]:
        layout = ctypes.c_void_p()
        root = ctypes.POINTER(Node)()

        # put in self._ref to the callbacks won't be garbage collected
        text_callback = draw_text_callback(lambda _, *args: scr.draw_text(*args))
        color_callback = draw_color_callback(lambda _, *args: scr.draw_color(*args))
        self._not_garbage += [text_callback, color_callback]

        assert not self._library.init_layout(
            layout, text_callback, color_callback, ctypes.c_void_p()
        ), "init_layout failed"
        assert not self._library.get_layout_root(
            layout, ctypes.byref(root)
        ), "get_layout_root failed"

        return layout, root

    def append_child(self, parent: NodePointer) -> NodePointer:
        child = ctypes.POINTER(Node)()
        assert not self._library.append_child(
            parent, ctypes.byref(child)
        ), "append_child failed"
        return child

    def append_text(self, parent: NodePointer, text: bytes) -> None:
        assert not self._library.append_text(parent, text), "append_text failed"

    def append_styled_text(
        self, parent: NodePointer, text: bytes, color: int, attrs: int
    ) -> None:
        assert not self._library.append_styled_text(
            parent, text, color, attrs
        ), "append_styled_text failed"

    def draw_layout(self, layout: Layout, scr: Screen) -> None:
        assert not self._library.draw_layout(
            layout, Rect(0, 0, scr.width, scr.height)
        ), "draw_layout failed"

    def clear(self):
        self._not_garbage = []
