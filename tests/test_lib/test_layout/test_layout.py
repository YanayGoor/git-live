from .utils.library import Library
from .utils.screen import VirtualScreen


def test_append_text(library: Library):
    scr = VirtualScreen(10, 5)
    layout, root = library.init_layout(scr)
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

    top = library.append_child(root)
    top.contents.expand = 1
    library.append_text(top, b"blabla")

    bottom = library.append_child(root)
    bottom.contents.expand = 1
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

    top = library.append_child(root)
    top.contents.expand = 1
    library.append_text(top, b"blabla")
    library.append_text(top, b"blabla")
    library.append_text(top, b"blabla")
    library.append_text(top, b"blabla")
    library.append_text(top, b"blabla")

    bottom = library.append_child(root)
    bottom.contents.basis = 3
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
