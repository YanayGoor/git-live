from dataclasses import dataclass, field


@dataclass
class VirtualScreen:
    width: int
    height: int
    text: list[bytes] = field(default_factory=list)
    color: list[list[int]] = field(default_factory=list)
    attr: list[list[int]] = field(default_factory=list)

    def __post_init__(self):
        self.text = self.text or [b" " * self.width] * self.height
        self.color = self.color or [[0] * self.width] * self.height
        self.attr = self.attr or [[0] * self.width] * self.height

    def draw_text(
        self, text: bytes, length: int, row: int, col: int, color: int, attrs: int
    ):
        subtext = text[: min(length, self.width - col)]
        self.text[row] = (
            self.text[row][:col]
            + subtext
            + self.text[row][col + len(subtext) :]  # noqa: E203
        )
        return 0

    def draw_color(self, x: int, y: int, width: int, height: int, color: int):
        print(f"{x=} {y=} {width=} {height=} {color=}")
        return 0
