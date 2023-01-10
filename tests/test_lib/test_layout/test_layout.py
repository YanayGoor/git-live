import pytest

from .utils.library import Library
from .utils.screen import VirtualScreen

NO_WRAP = 0
WRAP = 1

DIR_COLS = 1
DIR_ROWS = 2


def test_append_text(library: Library):
    scr = VirtualScreen(10, 5)
    layout, root = library.init_layout(scr)
    root.contents.nodes_direction = DIR_ROWS

    library.append_text(root, b"blabla")
    library.draw_layout(layout, scr)

    assert scr == VirtualScreen(
        width=10,
        height=5,
        text=[
            b"blabla    ",
            b"          ",
            b"          ",
            b"          ",
            b"          ",
        ],
        color=[
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        ],
        attr=[
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        ],
    )


def test_expand(library: Library):
    scr = VirtualScreen(10, 5)
    layout, root = library.init_layout(scr)
    root.contents.nodes_direction = DIR_ROWS

    top = library.append_child(root)
    top.contents.expand = 1
    top.contents.nodes_direction = DIR_ROWS
    library.append_text(top, b"blabla")

    bottom = library.append_child(root)
    bottom.contents.expand = 1
    bottom.contents.nodes_direction = DIR_ROWS
    library.append_text(bottom, b"blabla")

    library.draw_layout(layout, scr)

    assert scr == VirtualScreen(
        width=10,
        height=5,
        text=[
            b"blabla    ",
            b"          ",
            b"          ",
            b"blabla    ",
            b"          ",
        ],
        color=[
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        ],
        attr=[
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        ],
    )


def test_basis(library: Library):
    scr = VirtualScreen(10, 5)
    layout, root = library.init_layout(scr)
    root.contents.nodes_direction = DIR_ROWS

    top = library.append_child(root)
    top.contents.expand = 1
    top.contents.nodes_direction = DIR_ROWS
    library.append_text(top, b"blabla")
    library.append_text(top, b"blabla")
    library.append_text(top, b"blabla")
    library.append_text(top, b"blabla")
    library.append_text(top, b"blabla")

    bottom = library.append_child(root)
    bottom.contents.basis = 3
    bottom.contents.nodes_direction = DIR_ROWS
    library.append_text(bottom, b"yay")

    library.draw_layout(layout, scr)

    assert scr == VirtualScreen(
        width=10,
        height=5,
        text=[
            b"blabla    ",
            b"blabla    ",
            b"yay       ",
            b"          ",
            b"          ",
        ],
        color=[
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        ],
        attr=[
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        ],
    )


@pytest.mark.parametrize("root_direction, direction, amount, expected", [
    (DIR_ROWS, DIR_COLS, 3, [b' a a a    ',
                             b'bottom    ',
                             b'          ',
                             b'          ',
                             b'          ']),
    (DIR_COLS, DIR_COLS, 3, [b' abottom  ',
                             b' a        ',
                             b' a        ',
                             b'          ',
                             b'          ']),
    (DIR_ROWS, DIR_ROWS, 3, [b' a a      ',
                             b' a        ',
                             b'bottom    ',
                             b'          ',
                             b'          '],),
    (DIR_COLS, DIR_ROWS, 3, [b' abottom  ',
                             b' a        ',
                             b' a        ',
                             b'          ',
                             b'          '],),
    (DIR_ROWS, DIR_COLS, 7, [b' a a a a a',
                             b' a a      ',
                             b'bottom    ',
                             b'          ',
                             b'          '],),
    (DIR_COLS, DIR_COLS, 7, [b' a abottom',
                             b' a a      ',
                             b' a a      ',
                             b' a        ',
                             b'          '],),
    (DIR_ROWS, DIR_ROWS, 7, [b' a a a a  ',
                             b' a a a    ',
                             b'bottom    ',
                             b'          ',
                             b'          '],),
    (DIR_COLS, DIR_ROWS, 7, [b' a abottom',
                             b' a a      ',
                             b' a        ',
                             b' a        ',
                             b' a        '],),
    (DIR_ROWS, DIR_COLS, 13, [b' a a a a a',
                              b' a a a a a',
                              b' a a a    ',
                              b'bottom    ',
                              b'          ']),
    (DIR_COLS, DIR_COLS, 13, [b' a a abott',
                              b' a a a    ',
                              b' a a a    ',
                              b' a a a    ',
                              b' a        ']),
    (DIR_ROWS, DIR_ROWS, 13, [b' a a a a a',
                              b' a a a a  ',
                              b' a a a a  ',
                              b'bottom    ',
                              b'          ']),
    (DIR_COLS, DIR_ROWS, 13, [b' a a abott',
                              b' a a a    ',
                              b' a a a    ',
                              b' a a      ',
                              b' a a      '],),
], ids=["rows-cols-3", "cols-cols-3", "rows-rows-3", "cols-rows-3", "rows-cols-7", "cols-cols-7", "rows-rows-7", "cols-rows-7", "rows-cols-13", "cols-cols-13", "cols-rows-13", "rows-rows-13"])
def test_wrap_and_expand_cols(library: Library, root_direction: int, direction: int, amount: int,
                              expected: list[bytes]):
    scr = VirtualScreen(10, 5)
    layout, root = library.init_layout(scr)
    root.contents.nodes_direction = root_direction

    top = library.append_child(root)
    top.contents.fit_content = True
    top.contents.wrap = WRAP
    top.contents.nodes_direction = direction

    bottom = library.append_child(root)
    bottom.contents.expand = 1
    bottom.contents.nodes_direction = DIR_ROWS
    library.append_text(bottom, b"bottom")

    for _ in range(amount):
        library.append_text(top, b" a")

    library.draw_layout(layout, scr)

    assert scr == VirtualScreen(
        width=10,
        height=5,
        text=expected,
        color=[
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        ],
        attr=[
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        ],
    )
