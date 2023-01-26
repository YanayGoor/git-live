import pytest

from .utils.library import NODE_DIRECTION_COLS, NODE_DIRECTION_ROWS, NODE_WRAP, Library
from .utils.screen import VirtualScreen


def make_vertical(text: bytes, sep: bytes = b"\n"):
    return sep.join([i.to_bytes(1, byteorder="little") for i in text])


def test_append_text(library: Library):
    scr = VirtualScreen(10, 5)
    layout, root = library.init_layout(scr)
    root.contents.nodes_direction = NODE_DIRECTION_ROWS

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
    root.contents.nodes_direction = NODE_DIRECTION_ROWS

    top = library.append_child(root)
    top.contents.expand = 1
    top.contents.nodes_direction = NODE_DIRECTION_ROWS
    library.append_text(top, b"blabla")

    bottom = library.append_child(root)
    bottom.contents.expand = 1
    bottom.contents.nodes_direction = NODE_DIRECTION_ROWS
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


def test_color(library: Library):
    scr = VirtualScreen(10, 5)
    layout, root = library.init_layout(scr)
    root.contents.nodes_direction = NODE_DIRECTION_ROWS

    top = library.append_child(root)
    top.contents.expand = 1
    top.contents.nodes_direction = NODE_DIRECTION_COLS
    library.append_styled_text(top, b"aaaa", 0, 3)
    library.append_styled_text(top, b"bbbb", 1, 0)

    bottom = library.append_child(root)
    bottom.contents.expand = 1
    bottom.contents.nodes_direction = NODE_DIRECTION_ROWS
    bottom.contents.color = 5
    library.append_styled_text(bottom, b"cccc", 4, 0)

    library.draw_layout(layout, scr)

    assert scr == VirtualScreen(
        width=10,
        height=5,
        text=[
            b"aaaabbbb  ",
            b"          ",
            b"          ",
            b"cccc      ",
            b"          ",
        ],
        color=[
            [0, 0, 0, 0, 1, 1, 1, 1, 0, 0],
            [0, 0, 0, 0, 1, 1, 1, 1, 0, 0],
            [0, 0, 0, 0, 1, 1, 1, 1, 0, 0],
            [4, 4, 4, 4, 4, 4, 4, 4, 4, 4],
            [5, 5, 5, 5, 5, 5, 5, 5, 5, 5],
        ],
        attr=[
            [3, 3, 3, 3, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        ],
    )


def test_padding(library: Library):
    scr = VirtualScreen(10, 5)
    layout, root = library.init_layout(scr)
    root.contents.nodes_direction = NODE_DIRECTION_ROWS

    top = library.append_child(root)
    top.contents.basis = 3
    top.contents.nodes_direction = NODE_DIRECTION_ROWS
    top.contents.padding_left = 1
    top.contents.padding_right = 1
    top.contents.padding_top = 1
    top.contents.padding_bottom = 1
    library.append_text(top, b"blablablabla")

    bottom = library.append_child(root)
    bottom.contents.expand = 1
    bottom.contents.nodes_direction = NODE_DIRECTION_ROWS
    bottom.contents.padding_left = 2
    bottom.contents.padding_right = 2
    library.append_text(bottom, b"blablablabla")

    library.draw_layout(layout, scr)

    assert scr == VirtualScreen(
        width=10,
        height=5,
        text=[
            b"          ",
            b" blablabl ",
            b"          ",
            b"  blabla  ",
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
    root.contents.nodes_direction = NODE_DIRECTION_ROWS

    top = library.append_child(root)
    top.contents.expand = 1
    top.contents.nodes_direction = NODE_DIRECTION_ROWS
    library.append_text(top, b"blabla")
    library.append_text(top, b"blabla")
    library.append_text(top, b"blabla")
    library.append_text(top, b"blabla")
    library.append_text(top, b"blabla")

    bottom = library.append_child(root)
    bottom.contents.basis = 3
    bottom.contents.nodes_direction = NODE_DIRECTION_ROWS
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


@pytest.mark.parametrize(
    "root_direction, direction, amount, expected",
    [
        (
            NODE_DIRECTION_ROWS,
            NODE_DIRECTION_COLS,
            3,
            [b" aa a a   ", b"bottom    ", b"          ", b"          ", b"          "],
        ),
        (
            NODE_DIRECTION_COLS,
            NODE_DIRECTION_COLS,
            3,
            [b" aa bottom", b" a a      ", b"          ", b"          ", b"          "],
        ),
        (
            NODE_DIRECTION_ROWS,
            NODE_DIRECTION_ROWS,
            3,
            [b" aa a     ", b" a        ", b"bottom    ", b"          ", b"          "],
        ),
        (
            NODE_DIRECTION_COLS,
            NODE_DIRECTION_ROWS,
            3,
            [b" aabottom ", b" a        ", b" a        ", b"          ", b"          "],
        ),
        (
            NODE_DIRECTION_ROWS,
            NODE_DIRECTION_COLS,
            7,
            [b" aa a a aa", b" a a aa   ", b"bottom    ", b"          ", b"          "],
        ),
        (
            NODE_DIRECTION_COLS,
            NODE_DIRECTION_COLS,
            7,
            [b" aa bottom", b" a a      ", b" aa       ", b" a a      ", b" aa       "],
        ),
        (
            NODE_DIRECTION_ROWS,
            NODE_DIRECTION_ROWS,
            7,
            [b" aa aa aa ", b" a  a     ", b" a  a     ", b"bottom    ", b"          "],
        ),
        (
            NODE_DIRECTION_COLS,
            NODE_DIRECTION_ROWS,
            7,
            [b" aa a bott", b" a  aa    ", b" a        ", b" aa       ", b" a        "],
        ),
        (
            NODE_DIRECTION_ROWS,
            NODE_DIRECTION_COLS,
            11,
            [b" aa a a aa", b" a a aa a ", b" a aa a   ", b"bottom    ", b"          "],
        ),
        (
            NODE_DIRECTION_COLS,
            NODE_DIRECTION_COLS,
            11,
            [b" aa a abot", b" aa a a   ", b" aa a a   ", b" aa a     ", b"          "],
        ),
        (
            NODE_DIRECTION_ROWS,
            NODE_DIRECTION_ROWS,
            11,
            [b" aa a  a  ", b" a  a  aa ", b" a  aa a  ", b" aa a     ", b"bottom    "],
        ),
        (
            NODE_DIRECTION_COLS,
            NODE_DIRECTION_ROWS,
            11,
            [b" aa a  a b", b" a  aa    ", b" a  a     ", b" aa a     ", b" a  aa    "],
        ),
    ],
    ids=[
        "rows-cols-3",
        "cols-cols-3",
        "rows-rows-3",
        "cols-rows-3",
        "rows-cols-7",
        "cols-cols-7",
        "rows-rows-7",
        "cols-rows-7",
        "rows-cols-11",
        "cols-cols-11",
        "cols-rows-11",
        "rows-rows-11",
    ],
)
def test_wrap_and_expand_cols(
    library: Library,
    root_direction: int,
    direction: int,
    amount: int,
    expected: list[bytes],
):
    scr = VirtualScreen(10, 5)
    layout, root = library.init_layout(scr)
    root.contents.nodes_direction = root_direction

    top = library.append_child(root)
    top.contents.fit_content = True
    top.contents.wrap = NODE_WRAP
    top.contents.nodes_direction = direction

    bottom = library.append_child(root)
    bottom.contents.expand = 1
    bottom.contents.nodes_direction = NODE_DIRECTION_ROWS
    library.append_text(bottom, b"bottom")

    for i in range(amount):
        if i % 3 == 0:
            library.append_text(top, b" aa")
        else:
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


def test_item_partially_overflowed(library: Library):
    scr = VirtualScreen(10, 5)
    layout, root = library.init_layout(scr)
    root.contents.nodes_direction = NODE_DIRECTION_ROWS

    top = library.append_child(root)
    top.contents.basis = 4
    top.contents.nodes_direction = NODE_DIRECTION_ROWS

    bottom = library.append_child(root)
    bottom.contents.expand = 1
    bottom.contents.basis = 2
    bottom.contents.nodes_direction = NODE_DIRECTION_ROWS
    library.append_text(bottom, b"bottom")

    library.draw_layout(layout, scr)

    assert scr == VirtualScreen(
        width=10,
        height=5,
        text=[
            b"          ",
            b"          ",
            b"          ",
            b"          ",
            b"bottom    ",
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


@pytest.mark.parametrize(
    "direction, word, expected",
    [
        (
            NODE_DIRECTION_ROWS,
            b"abcd",
            [b"abcdabcdab", b"abcdabcdab", b"abcdabcdab", b"abcdabcdab", b"abcdabcdab"],
        ),
        (
            NODE_DIRECTION_COLS,
            b"a\nb",
            [b"aaaaaaaaaa", b"bbbbbbbbbb", b"aaaaaaaaaa", b"bbbbbbbbbb", b"aaaaaaaaaa"],
        ),
    ],
    ids=["rows", "cols"],
)
def test_wrap_overflow_on_the_second_last_line_no_out_of_bounds_draw(
    library: Library, direction, word, expected
):
    """
    Test that the layout cuts the width of the overflowing line in a wrap=True node even if it is not the last line,
    the last line needs to overflow for this to occur.
    If the screen gets an out-of-bounds write, it will fail and so this test would fail.
    """
    width = 10
    height = 5
    scr = VirtualScreen(width, height)
    layout, root = library.init_layout(scr)
    root.contents.nodes_direction = NODE_DIRECTION_ROWS

    top = library.append_child(root)
    top.contents.expand = 1
    top.contents.wrap = NODE_WRAP
    top.contents.nodes_direction = direction
    for _ in range((height if direction == NODE_DIRECTION_ROWS else width) * 4):
        library.append_text(top, word)

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


@pytest.mark.parametrize(
    "direction, word, expected",
    [
        (
            NODE_DIRECTION_ROWS,
            b"abcd",
            [b"abcd      ", b"abcd      ", b"abcd      ", b"abcd      ", b"abcd      "],
        ),
        (
            NODE_DIRECTION_COLS,
            b"a\nb",
            [b"aaaaaaaaaa", b"bbbbbbbbbb", b"          ", b"          ", b"          "],
        ),
    ],
    ids=["rows", "cols"],
)
def test_fit_content_no_out_of_bounds_draw(library: Library, direction, word, expected):
    """
    Test that the even with fit_content set to True, still no out of bound error occurs.
    """
    width = 10
    height = 5
    scr = VirtualScreen(width, height)
    layout, root = library.init_layout(scr)
    root.contents.nodes_direction = direction

    top = library.append_child(root)
    top.contents.fit_content = True
    top.contents.nodes_direction = direction

    for _ in range(20):
        library.append_text(top, word)

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


@pytest.mark.parametrize(
    "direction, word, expected",
    [
        (
            NODE_DIRECTION_ROWS,
            b"abcd",
            [b"abcdabcd  ", b"abcdabcd  ", b"abcdabcd  ", b"abcd      ", b"blabla    "],
        ),
        (
            NODE_DIRECTION_COLS,
            b"a\nb",
            [b"aaaaaaaabl", b"bbbbbbbb  ", b"aaaaaaa   ", b"bbbbbbb   ", b"          "],
        ),
    ],
    ids=["rows", "cols"],
)
def test_wrap_overflowed_last_line_included_in_min_size(
    library: Library, direction, word, expected
):
    """
    I had a bug where I would not consider the width of the last line if it was cut off and returned a smaller size
    then really necessary to render the node, so this test checks that the bottom node gets pushed correctly.
    """
    width = 10
    height = 5
    scr = VirtualScreen(width, height)
    layout, root = library.init_layout(scr)
    root.contents.nodes_direction = direction

    top = library.append_child(root)
    top.contents.fit_content = True
    top.contents.wrap = NODE_WRAP
    top.contents.nodes_direction = direction
    for _ in range(int((height if direction == NODE_DIRECTION_ROWS else width) * 1.5)):
        library.append_text(top, word)

    bottom = library.append_child(root)
    bottom.contents.expand = 1
    bottom.contents.nodes_direction = NODE_DIRECTION_ROWS
    library.append_text(bottom, b"blabla")

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


@pytest.mark.parametrize(
    "direction, seperator, expected",
    [
        (
            NODE_DIRECTION_ROWS,
            b"",
            [b" red    br", b" blue   pu", b" green    ", b" green    ", b" yellow   "],
        ),
        (
            NODE_DIRECTION_COLS,
            b"\n",
            [
                b"     ",
                b"rbggy",
                b"elrre",
                b"dueel",
                b" eeel",
                b"  nno",
                b"    w",
                b"     ",
                b"bp   ",
                b"ru   ",
            ],
        ),
    ],
    ids=["rows", "cols"],
)
def test_wrap_last_line_included_in_min_size(
    library: Library, direction, seperator, expected
):
    if direction == NODE_DIRECTION_ROWS:
        width = 10
        height = 5
    else:
        width = 5
        height = 10

    scr = VirtualScreen(width, height)
    layout, root = library.init_layout(scr)
    root.contents.nodes_direction = direction

    top = library.append_child(root)
    top.contents.expand = 1
    top.contents.fit_content = True
    top.contents.wrap = 1
    top.contents.nodes_direction = direction
    library.append_text(top, make_vertical(b" red", sep=seperator))
    library.append_text(top, make_vertical(b" blue", sep=seperator))
    library.append_text(top, make_vertical(b" green", sep=seperator))
    library.append_text(top, make_vertical(b" green", sep=seperator))
    library.append_text(top, make_vertical(b" yellow", sep=seperator))
    library.append_text(top, make_vertical(b" brown", sep=seperator))
    library.append_text(top, make_vertical(b" purple", sep=seperator))

    library.draw_layout(layout, scr)

    assert scr == VirtualScreen(
        width=width,
        height=height,
        text=expected,
        color=[[0] * width] * height,
        attr=[[0] * width] * height,
    )


def test_dont_use_hidden_items_in_size_calculation_rows(library: Library):
    scr = VirtualScreen(10, 5)
    layout, root = library.init_layout(scr)
    root.contents.nodes_direction = NODE_DIRECTION_ROWS

    top = library.append_child(root)
    top.contents.expand = 1
    top.contents.nodes_direction = NODE_DIRECTION_COLS

    top_left = library.append_child(top)
    top_left.contents.fit_content = True
    top_left.contents.nodes_direction = NODE_DIRECTION_ROWS

    top_right = library.append_child(top)
    top_right.contents.expand = 1
    top_right.contents.nodes_direction = NODE_DIRECTION_ROWS

    library.append_text(top_left, b"bla")
    library.append_text(top_left, b"bla")
    library.append_text(top_left, b"bla")
    library.append_text(top_left, b"longer")

    library.append_text(top_right, b" c")
    library.append_text(top_right, b" c")
    library.append_text(top_right, b" c")
    library.append_text(top_right, b" c")
    library.append_text(top_right, b" c")

    bottom = library.append_child(root)
    bottom.contents.basis = 3
    bottom.contents.nodes_direction = NODE_DIRECTION_ROWS
    library.append_text(bottom, b"yay")

    library.draw_layout(layout, scr)

    assert scr == VirtualScreen(
        width=10,
        height=5,
        text=[
            b"bla c     ",
            b"bla c     ",
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


def test_dont_use_hidden_items_in_size_calculation_cols(library: Library):
    scr = VirtualScreen(10, 5)
    layout, root = library.init_layout(scr)
    root.contents.nodes_direction = NODE_DIRECTION_COLS

    top = library.append_child(root)
    top.contents.expand = 1
    top.contents.nodes_direction = NODE_DIRECTION_ROWS

    top_left = library.append_child(top)
    top_left.contents.fit_content = True
    top_left.contents.nodes_direction = NODE_DIRECTION_COLS

    top_right = library.append_child(top)
    top_right.contents.expand = 1
    top_right.contents.nodes_direction = NODE_DIRECTION_COLS

    library.append_text(top_left, b"b\nl\na")
    library.append_text(top_left, b"b\nl\na")
    library.append_text(top_left, b"b\nl\na")
    library.append_text(top_left, b"b\nl\na")
    library.append_text(top_left, b"b\nl\na")
    library.append_text(top_left, b"l\no\nn\ng\nr")

    library.append_text(top_right, b"c")
    library.append_text(top_right, b"c")
    library.append_text(top_right, b"c")
    library.append_text(top_right, b"c")
    library.append_text(top_right, b"c")

    bottom = library.append_child(root)
    bottom.contents.basis = 5
    bottom.contents.nodes_direction = NODE_DIRECTION_ROWS
    library.append_text(bottom, b"yay")

    library.draw_layout(layout, scr)

    assert scr == VirtualScreen(
        width=10,
        height=5,
        text=[
            b"bbbbbyay  ",
            b"lllll     ",
            b"aaaaa     ",
            b"ccccc     ",
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


@pytest.mark.parametrize(
    "width, height, expected",
    [
        (
            3,
            2,
            [b"Fir", b" re"],
        ),
        (
            5,
            3,
            [b"First", b" red ", b" blue"],
        ),
        (
            7,
            4,
            [b"First h", b" red   ", b" blue  ", b" green "],
        ),
        (
            10,
            5,
            [b"First head", b" red    br", b" blue   pu", b" green    ", b" yellow   "],
        ),
        (
            20,
            10,
            [
                b"First header        ",
                b" red   yellow       ",
                b" blue  brown        ",
                b" green purple       ",
                b"Second header       ",
                b" red  1             ",
                b" blue 2             ",
                b"Third header        ",
                b" very long li 11:11 ",
                b" extremely lo 22:22 ",
            ],
        ),
        (
            30,
            15,
            [
                b"First header                  ",
                b" red    brown                 ",
                b" blue   purple                ",
                b" green                        ",
                b" yellow                       ",
                b"Second header                 ",
                b" red        1                 ",
                b" blue       2                 ",
                b" ocean blue 3                 ",
                b"                              ",
                b"Third header                  ",
                b" very long line bla bla 11:11 ",
                b" extremely long line bl 22:22 ",
                b" not so long line       33:33 ",
                b"                              ",
            ],
        ),
    ],
    ids=["3-2", "5-3", "7-4", "10-5", "20-10", "30-15"],
)
def test_complex_layout(
    library: Library, width: int, height: int, expected: list[bytes]
):
    scr = VirtualScreen(width, height)
    layout, root = library.init_layout(scr)
    root.contents.nodes_direction = NODE_DIRECTION_ROWS

    top_header = library.append_child(root)
    top_header.contents.basis = 1
    top_header.contents.nodes_direction = NODE_DIRECTION_COLS
    library.append_text(top_header, b"First header")

    top = library.append_child(root)
    top.contents.expand = 1
    top.contents.fit_content = True
    top.contents.wrap = 1
    top.contents.nodes_direction = NODE_DIRECTION_ROWS
    library.append_text(top, b" red")
    library.append_text(top, b" blue")
    library.append_text(top, b" green")
    library.append_text(top, b" yellow")
    library.append_text(top, b" brown")
    library.append_text(top, b" purple")

    middle_header = library.append_child(root)
    middle_header.contents.basis = 1
    middle_header.contents.nodes_direction = NODE_DIRECTION_COLS
    library.append_text(middle_header, b"Second header")

    middle = library.append_child(root)
    middle.contents.expand = 1
    middle.contents.padding_left = 1
    middle.contents.nodes_direction = NODE_DIRECTION_COLS

    middle_left = library.append_child(middle)
    middle_left.contents.fit_content = True
    middle_left.contents.nodes_direction = NODE_DIRECTION_ROWS
    library.append_text(middle_left, b"red")
    library.append_text(middle_left, b"blue")
    library.append_text(middle_left, b"ocean blue")

    middle_right = library.append_child(middle)
    middle_right.contents.expand = 1
    middle_right.contents.nodes_direction = NODE_DIRECTION_ROWS
    middle_right.contents.padding_left = 1
    library.append_text(middle_right, b"1")
    library.append_text(middle_right, b"2")
    library.append_text(middle_right, b"3")

    bottom_header = library.append_child(root)
    bottom_header.contents.basis = 1
    bottom_header.contents.nodes_direction = NODE_DIRECTION_COLS
    library.append_text(bottom_header, b"Third header")

    bottom = library.append_child(root)
    bottom.contents.expand = 1
    bottom.contents.padding_left = 1
    bottom.contents.nodes_direction = NODE_DIRECTION_COLS

    bottom_left = library.append_child(bottom)
    bottom_left.contents.nodes_direction = NODE_DIRECTION_ROWS
    bottom_left.contents.expand = 1
    library.append_text(bottom_left, b"very long line bla bla")
    library.append_text(bottom_left, b"extremely long line bla bla")
    library.append_text(bottom_left, b"not so long line")

    bottom_right = library.append_child(bottom)
    bottom_right.contents.padding_right = 1
    bottom_right.contents.padding_left = 1
    bottom_right.contents.fit_content = True
    bottom_right.contents.nodes_direction = NODE_DIRECTION_ROWS
    library.append_text(bottom_right, b"11:11")
    library.append_text(bottom_right, b"22:22")
    library.append_text(bottom_right, b"33:33")

    library.draw_layout(layout, scr)

    assert scr == VirtualScreen(
        width=width,
        height=height,
        text=expected,
        color=[[0] * width] * height,
        attr=[[0] * width] * height,
    )
