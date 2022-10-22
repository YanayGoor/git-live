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
        self, text: bytes, length: int, col: int, row: int, color: int, attrs: int
    ):
        if col + length > self.width:
            return 1
        if row >= self.height:
            return 1
        subtext = text[: min(length, self.width - col)]
        self.text[row] = (
            self.text[row][:col]
            + subtext
            + self.text[row][col + len(subtext) :]  # noqa: E203
        )
        self.color[row] = (
            self.color[row][:col]
            + ([color] * length)
            + self.color[row][col + length :]  # noqa: E203
        )
        self.attr[row] = (
            self.attr[row][:col]
            + ([attrs] * length)
            + self.attr[row][col + length :]  # noqa: E203
        )
        return 0

    def draw_color(self, col: int, row: int, width: int, height: int, color: int):
        if col + width > self.width:
            return 1
        if row + height > self.height:
            return 1
        self.color[row : row + height] = (  # noqa: E203
            color_row[:col] + ([color] * width) + color_row[col + width :]  # noqa: E203
            for color_row in self.color[row : row + height]  # noqa: E203
        )
        return 0
